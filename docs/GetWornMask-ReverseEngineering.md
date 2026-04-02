# GetWornMask Reverse Engineering Notes

## Goal

Determine how vanilla Skyrim Special Edition `1.6.1170` computes the worn biped
slot mask for equipped armor:

- does it use `TESObjectARMO::BGSBipedObjectForm::bipedModelData.bipedObjectSlots`
- or does it walk `TESObjectARMO::armorAddons` and derive the mask from
  `TESObjectARMA`

This note documents the live-process reverse engineering used to answer that.

## Environment

- Runtime: Skyrim Special Edition `1.6.1170`
- Binary: `SkyrimSE.exe`
- Address library source:
  `Data/SKSE/Plugins/versionlib-1-6-1170-0.bin`
- Process note: DAVE had already hooked `InventoryChanges::GetWornMask`, so the
  cleaner vanilla evidence came from `GetWornMaskVisitor::Visit` and its callee.

## Relevant Types

- `InventoryEntryData::object` is a `TESBoundObject*`
- `TESObjectARMO` embeds `BGSBipedObjectForm` at offset `0x1B0`
- `TESObjectARMA` embeds `BGSBipedObjectForm` at offset `0x30`
- `BGSBipedObjectForm::bipedModelData.bipedObjectSlots` is at offset `0x08`
  from the `BGSBipedObjectForm` base
- `TESObjectARMO::armorAddons` is at offset `0x208`

Relevant local headers:

- `lib/commonlibsse-ng/include/RE/I/InventoryEntryData.h`
- `lib/commonlibsse-ng/include/RE/T/TESObjectARMO.h`
- `lib/commonlibsse-ng/include/RE/T/TESObjectARMA.h`
- `lib/commonlibsse-ng/include/RE/B/BGSBipedObjectForm.h`
- `lib/commonlibsse-ng/include/RE/F/FormTypes.h`

## Address Resolution

Using the installed AE address library for `1.6.1170`:

- `InventoryChanges::GetWornMask` ID `16044` -> `0x225CB0`
- `InventoryChanges::VisitWornItems` ID `16096` -> `0x231AA0`
- `GetWornMaskVisitor::Visit` ID `16233` -> `0x23CB20`

With process base `0x7FF718F40000`, the live addresses were:

- `GetWornMask`: `0x7FF719165CB0`
- `VisitWornItems`: `0x7FF719171AA0`
- `GetWornMaskVisitor::Visit`: `0x7FF71917CB20`

## Why `GetWornMaskVisitor::Visit` Was The Best Target

`InventoryChanges::GetWornMask` was already patched by DAVE in the live process.
That made its prologue unreliable as vanilla evidence.

`GetWornMaskVisitor::Visit` was still unmodified, and it contains the logic that
converts an equipped inventory entry into a bitmask contribution.

## Live Vanilla Assembly

### `GetWornMaskVisitor::Visit`

The important block in the live process was:

```asm
0x7ff71917cb30: mov  rcx, qword ptr [rdx]    ; entryData->object
0x7ff71917cb33: test rcx, rcx
0x7ff71917cb36: je   0x7ff71917cba4
0x7ff71917cb38: movzx eax, byte ptr [rcx + 0x1a] ; formType
0x7ff71917cb3c: cmp  al, 0x1f
0x7ff71917cb3e: je   0x7ff71917cba4
0x7ff71917cb40: sub  al, 0x29
0x7ff71917cb42: cmp  al, 1
0x7ff71917cb44: jbe  0x7ff71917cba4
0x7ff71917cb4d: call 0x7ff71910caf0
0x7ff71917cb52: test rax, rax
0x7ff71917cb57: mov  ebx, dword ptr [rax + 8]
0x7ff71917cb9c: or   dword ptr [rsi + 8], ebx
```

Interpretation:

- `rdx` is `InventoryEntryData*`
- `[rdx]` is `InventoryEntryData::object`
- the function calls a helper with that object
- if the helper returns non-null, it reads a `uint32_t` at `helperResult + 8`
- that value is ORed into the visitor's mask accumulator

Notably, there is no walk over `TESObjectARMO::armorAddons` in this function.

### Helper at `0x7FF71910CAF0`

The callee used by `Visit` was:

```asm
0x7ff71910caf0: test rcx, rcx
0x7ff71910caf3: je   0x7ff71910cb0e
0x7ff71910caf5: movzx eax, byte ptr [rcx + 0x1a] ; formType
0x7ff71910caf9: cmp  al, 0x1a                    ; ARMO
0x7ff71910cafb: je   0x7ff71910cb06
0x7ff71910cafd: cmp  al, 0x66                    ; ARMA
0x7ff71910caff: jne  0x7ff71910cb0e
0x7ff71910cb01: lea  rax, [rcx + 0x30]
0x7ff71910cb05: ret
0x7ff71910cb06: lea  rax, [rcx + 0x1b0]
0x7ff71910cb0d: ret
0x7ff71910cb0e: xor  eax, eax
0x7ff71910cb10: ret
```

Form type constants from `FormTypes.h`:

- `0x1A` = `Armor` (`TESObjectARMO`)
- `0x66` = `Armature` (`TESObjectARMA`)

So the helper returns:

- `armor + 0x1B0` for `TESObjectARMO`
- `addon + 0x30` for `TESObjectARMA`

Those are exactly the embedded `BGSBipedObjectForm` subobjects for those types.

## Final Interpretation

The next instruction in `Visit` is:

```asm
mov ebx, dword ptr [rax + 8]
```

Since `rax` points at `BGSBipedObjectForm`, `[rax + 8]` is:

- `BGSBipedObjectForm::bipedModelData.bipedObjectSlots`

That means vanilla computes the worn mask by:

1. taking the entry's inventory object
2. mapping `ARMO` or `ARMA` to its embedded `BGSBipedObjectForm`
3. reading `bipedObjectSlots`
4. ORing that mask into the accumulator

## Conclusion

For normal worn inventory entries, vanilla uses the armor form's own
`bipedObjectSlots`. It does **not** traverse `TESObjectARMO::armorAddons` in the
`GetWornMaskVisitor::Visit` path.

The helper supports both `ARMO` and `ARMA`, but that is best understood as a
generic "return the embedded `BGSBipedObjectForm` for this form" utility. Since
`InventoryEntryData::object` is a `TESBoundObject*`, normal inventory enumeration
should primarily encounter `ARMO`, not `ARMA`.

## Why This Matters For DAVE

DAVE's variant-aware replacement is intentionally broader than vanilla:

- vanilla reads the form's embedded biped slot mask directly
- DAVE resolves variant substitutions and may recompute a mask from resolved
  armor-addons plus additional head/hair override rules

This reverse engineering confirms that vanilla's baseline behavior is the armor
form's own `bipedObjectSlots`, not an aggregated addon-derived mask.
