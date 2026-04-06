# RaceMenu Compatibility Notes

## Goal

Document the reverse engineering and implementation work needed to make DAVE's
visual armor resolution compatible with RaceMenu's NiOverride-driven systems.

The initial motivating bug was:

- Wet Function Redux failed to apply wet skin effects when DAVE visually
  swapped armor in a way that exposed body skin.

This note records:

- what RaceMenu actually does
- which seams were safe to hook
- which seams were unstable or misleading
- the final design currently implemented in DAVE
- the remaining edge cases we explicitly chose not to solve yet

## Problem Statement

DAVE changes the effective rendered armor/addon set without changing the actor's
equipped inventory objects.

RaceMenu's NiOverride code does not reason about "what is visually exposed?" in
the abstract. It reasons from:

- worn slot ownership
- owning `ARMO`
- that armor's source `ARMA`s
- then live geometry traversal

That mismatch matters for systems like Wet Function Redux, which use NiOverride
skin-property APIs instead of querying DAVE directly.

## Repro Summary

Observed behavior:

- if DAVE hid armor entirely, wet effects could still apply
- if DAVE swapped the worn armor to a skin-showing visual replacement, wet
  effects stopped applying
- if the player actually equipped the skin-showing armor directly, wet effects
  worked again

This strongly suggested the issue was not Wet Function's decision logic alone,
but RaceMenu's notion of slot ownership and addon traversal.

## Wet Function Findings

Wet Function uses NiOverride skin APIs from Papyrus:

- `NiOverride.GetSkinPropertyString`
- `NiOverride.GetSkinPropertyFloat`
- `NiOverride.AddSkinOverrideString`
- `NiOverride.AddSkinOverrideFloat`

Relevant script:

- [WetFunctionEffect.psc](/mnt/f/games/skyrim/modlists/pt_test/mods/WetFunctionRedux%20SE/Scripts/Source/WetFunctionEffect.psc)

Important finding:

- `alwaysOperate` was not the real fix
- the mod was still trying to apply skin overrides
- the problem was that RaceMenu was targeting the wrong owner/addon path for the
  queried slot

We confirmed the exposed surface was real body skin because overlays still
appeared on it.

## RaceMenu Source Findings

The cloned RaceMenu source is in:

- `/mnt/f/games/skyrim/workspace/SKSE64Plugins/skee64`

The core path for Wet Function is:

1. Papyrus NiOverride entrypoint in `PapyrusNiOverride.cpp`
2. `OverrideInterface::Impl_GetSkinProperty` or
   `OverrideInterface::Impl_SetSkinProperty`
3. `GetSkinForm(actor, slotMask)`
4. iterate `armor->armorAddons`
5. `VisitArmorAddon(actor, armor, addon, ...)`
6. walk live geometry and read/write skin shader properties

The most important helpers are:

- [PapyrusNiOverride.cpp](/mnt/f/games/skyrim/workspace/SKSE64Plugins/skee64/PapyrusNiOverride.cpp)
- [OverrideInterface.cpp](/mnt/f/games/skyrim/workspace/SKSE64Plugins/skee64/OverrideInterface.cpp)
- [NifUtils.cpp](/mnt/f/games/skyrim/workspace/SKSE64Plugins/skee64/NifUtils.cpp)

### `GetSkinForm`

`GetSkinForm(actor, slotMask)` does:

1. ask `GetWornForm(actor, slotMask)`
2. if none, fall back to actor-base skin
3. if none, fall back to race skin

This is the core mismatch:

- DAVE may make a slot visually exposed
- but RaceMenu still sees the original worn armor as the owner of that slot

### `VisitArmorAddon`

After owner selection, RaceMenu iterates the chosen armor's source `ARMA`s and
calls `VisitArmorAddon(...)`.

This means DAVE compatibility has two distinct concerns:

- owner/fallback semantics
- actual visual addon traversal

### `VisitAllWornItems`

RaceMenu also has paths that enumerate worn items through:

- `VisitAllWornItems(actor, mask, std::function<void(InventoryEntryData*)>)`

Source-level behavior:

- iterate worn inventory entries
- apply a slot matcher
- invoke a callback for matching worn entries

This matters because RaceMenu has broader equipped-node and skin-reapply paths
that are driven by worn-entry mask selection, not just `GetSkinForm`.

## The Compatibility Model We Landed On

The current DAVE RaceMenu compatibility layer uses three seams:

1. `GetSkinForm`
2. `VisitAllWornItems::MatchBySlot`
3. `VisitArmorAddon`

Current implementation:

- [RaceMenuCompat.cpp](/mnt/f/games/skyrim/workspace/DynamicArmorVariants-Extended/src/main/RaceMenuCompat.cpp)
- [RaceMenuCompat.h](/mnt/f/games/skyrim/workspace/DynamicArmorVariants-Extended/src/main/RaceMenuCompat.h)
- [SKSEPlugin.cpp](/mnt/f/games/skyrim/workspace/DynamicArmorVariants-Extended/src/main/SKSEPlugin.cpp)

### 1. `GetSkinForm` hook

Purpose:

- preserve owner/fallback semantics for skin-slot queries

Behavior:

- find the original worn `ARMO` whose DAVE-effective coverage still contains the
  queried slot
- if none exists, fall back to actor/race skin

Important detail:

- this returns the original effective owner `ARMO`, not the resolved replacement
  `ARMO`
- that avoids accidental second-stage variant chaining

### 2. `VisitAllWornItems::MatchBySlot` internal patch

Purpose:

- preserve RaceMenu's original `VisitAllWornItems` callback and iteration logic
- replace only the slot-match decision for worn `ARMO`s

Behavior:

- let `skee64` keep ownership of the `std::function` callback invocation
- patch the internal matcher branch so it uses DAVE-effective slot coverage for
  `ARMO` entries
- preserve original raw slot-mask logic for non-`ARMO` biped forms

Important detail:

- we do **not** replace `VisitAllWornItems` wholesale anymore
- we only use a pure-forward trampoline on `VisitAllWornItems` to stash the
  current actor in thread-local storage
- the actual selection logic happens in the internal matcher patch

AE `0.4.19.16` disassembly detail:

```asm
1800c2c15  mov [rsp+0x48], rax   ; matcher vptr
1800c2c1a  mov [rsp+0x50], ebx   ; matcher.mask
...
1800c2c31  mov rax, [rsp+0x48]
1800c2c36  mov rdx, [rsi]
1800c2c39  lea rcx, [rsp+0x48]
1800c2c3e  call qword ptr [rax]  ; matcher.Matches(form)
1800c2c40  test al, al
1800c2c42  je  1800c2c94
```

That means the patch site is not an inline compare. It is a virtual predicate
call on a tiny stack-local `MatchBySlot` object.

Important correction from the April 5, 2026 debugger session:

- the original `test al, al; je ...` bytes after the predicate call must remain
  intact
- on AE `0.4.19.16`, later loop code jumps back into that `je` as a latch after
  advancing `rbx`
- our old 19-byte patch erased that branch and caused a null `rbx` fallthrough
  to `mov rdi, [rbx]`
- the fixed patch now replaces only the predicate-call setup/call bytes and
  resumes at the original `test/jcc`

This was verified across the current supported DLL set:

- VR `0.4.14`
- SE `0.4.16`
- AE `0.4.19.9`
- AE `0.4.19.10`
- AE `0.4.19.11`
- AE/GOG `0.4.19.13`
- AE/GOG `0.4.19.14`
- AE `0.4.19.15`
- AE `0.4.19.16`

All of those builds use the same relevant structure:

- one 15-byte predicate-call region
- trailing `test al, al; jcc miss`
- later loop code jumps back into that branch rather than branching directly to
  the miss label

### 3. `VisitArmorAddon` hook

Purpose:

- redirect source addon traversal to DAVE's resolved visual pair(s)

Behavior:

- call `DynamicArmorManager::VisitArmorAddons(...)`
- collect unique resolved `(ARMO, ARMA)` pairs
- if there are resolved pairs, call RaceMenu's original `VisitArmorAddon` for
  each resolved pair
- otherwise fall back to the original `(ARMO, ARMA)` pair

This is what makes ARMA-only visual swaps work in practice.

## Why We Do Not Fully Replace `VisitAllWornItems`

This was the main failed direction during the session.

We tried a full-function detour that:

- re-enumerated effective worn items through DAVE
- then invoked RaceMenu's callback from DAVE

That shape appeared to work in one older deployed build, but repeatedly crashed
in current testing.

Important finding:

- a pure-forward `VisitAllWornItems` detour was stable
- a full replacement was not

That strongly suggests the unsafe part was not the entrypoint patch itself, but
replacing the whole function behavior and moving callback invocation into DAVE.

Even if the callback target is logically the same, the original function keeps:

- local matcher state
- local visitor state
- callback invocation

inside `skee64.dll`.

The matcher patch keeps that contract intact.

## Failed / Rejected Approaches

### Hook global gameplay-facing worn-form queries

Rejected because:

- DAVE should not change gameplay semantics globally
- the RaceMenu problem is narrower than general gameplay slot ownership

### Rely on Wet Function settings alone

Rejected because:

- `alwaysOperate` and the Skyrim Outfit System compatibility patch were only
  script-level workarounds
- they did not fix the deeper owner/addon mismatch inside RaceMenu

### Whole-function `VisitAllWornItems` replacement

Rejected because:

- unstable in current testing
- hard to reason about at the callback/ABI seam
- unnecessary once the internal matcher patch worked

### Swap out the stack-local `MatchBySlot` object

Considered after disassembling AE `0.4.19.16`.

Why it looked attractive:

- source and binary both show that `VisitAllWornItems` builds a small
  stack-local matcher object and calls it virtually
- in theory, replacing that object would be a cleaner abstraction than
  replacing the call site

Rejected for now because:

- the matcher is not constructed through one clean constructor call; the object
  is assembled directly into stack storage
- replacing it safely would require owning the exact `FormMatcher` object ABI
  and vtable contract for each supported RaceMenu build
- that is narrower than patching `MatchBySlot::Matches` globally, but still
  more brittle than the current fix
- the current patch shape is now instruction-correct: it replaces only the
  predicate call and preserves RaceMenu's original branch/latch structure

### Whole-function `VisitEquippedNodes` replacement

Considered, but not needed once the `VisitAllWornItems` internal matcher patch
worked.

### Temporary `OverrideInterface` tracing hooks

We temporarily instrumented:

- `OverrideInterface::OnAttach`
- `OverrideInterface::Impl_ApplySkinOverrides`

Those traces did not fire in the Wet Function rebuild repro we were using, so
they were removed.

## Thread-Safety / Reentrancy

The current matcher patch needs to know the actor when RaceMenu evaluates the
internal `VisitAllWornItems` matcher.

Current design:

- `VisitAllWornItems` is still trampoline-hooked
- the hook is a pure forwarder
- it stores the current actor in `thread_local` storage for the duration of the
  original call
- the matcher patch reads that actor through a helper

Current implementation uses a scoped guard:

- `ScopedVisitAllWornItemsActor`

This means:

- different threads do not share actor context
- nested/reentrant calls on the same thread restore the previous actor

Relevant commit:

- `725e51a` `Make DAVE RaceMenu matcher context scoped`

## Current Static Version Support

The current static layout table in
[RaceMenuCompat.cpp](/mnt/f/games/skyrim/workspace/DynamicArmorVariants-Extended/src/main/RaceMenuCompat.cpp)
supports:

- VR `0.4.14`
- SE `0.4.16`
- AE `0.4.19.9`
- AE `0.4.19.10`
- AE `0.4.19.11`
- AE `0.4.19.13`
- GOG `0.4.19.13`
- AE `0.4.19.14`
- GOG `0.4.19.14`
- AE `0.4.19.15`
- AE `0.4.19.16`

Selection is keyed by:

1. loaded module name
   - `skeevr.dll` preferred over `skee64.dll`
2. fixed DLL version
3. PE timestamp to break ties for ambiguous same-version builds

## Settings

RaceMenu compatibility hooks are guarded by:

- `installRaceMenuCompatHooks`

Relevant files:

- [Settings.h](/mnt/f/games/skyrim/workspace/DynamicArmorVariants-Extended/src/main/Settings.h)
- [Settings.cpp](/mnt/f/games/skyrim/workspace/DynamicArmorVariants-Extended/src/main/Settings.cpp)
- [settings.json](/mnt/f/games/skyrim/workspace/DynamicArmorVariants-Extended/data/SKSE/Plugins/DynamicArmorVariants/settings.json)

## What Was Verified

Verified in current testing:

- Wet Function works correctly for DAVE-exposed skin
- ARMA-only substitutions still work with the current `VisitArmorAddon` remap
- the matcher-patch design is stable where the full `VisitAllWornItems`
  replacement was not

Relevant commits from this session:

- `910c75d` `Add DAVE RaceMenu compatibility hooks`
- `9a750ab` `Expand DAVE RaceMenu compat across versions`
- `725e51a` `Make DAVE RaceMenu matcher context scoped`

## Remaining Known Edge Case

There is still one niche unresolved RaceMenu edge case:

- `OverrideInterface::Impl_SetSkinProperties` gates source addons with
  `IsSlotMatch(...)` before it reaches `VisitArmorAddon(...)`

That means a future case could still fail if:

- the source `ARMA` mask does not match the queried slot
- but the DAVE-resolved visual `ARMA` should be the one servicing that slot

Current status:

- this did **not** block the Wet Function path we verified
- overlays also appeared to work in current testing
- we chose not to chase this edge case further for now

If it ever matters, the intended fix is:

- patch the internal `IsSlotMatch(...)` decision in
  `OverrideInterface::Impl_SetSkinProperties`
- using the same strategy as `VisitAllWornItems::MatchBySlot`

That keeps the change local and avoids another whole-function detour.

## Practical Guidance

If RaceMenu compatibility breaks again:

1. Check whether the loaded `skee64.dll` / `skeevr.dll` version and timestamp are
   already covered by the static layout table.
2. If not, derive the new RVAs/prologues for:
   - `GetSkinForm`
   - `VisitAllWornItems`
   - `VisitAllWornItems::MatchBySlot`
   - `VisitArmorAddon`
3. Prefer internal branch patches over whole-function replacement when the
   original function owns callback invocation.
4. Treat any attempt to fully replace `VisitAllWornItems` as experimental and
   high-risk.
