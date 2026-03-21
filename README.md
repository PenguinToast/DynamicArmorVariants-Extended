Framework for mods to define armor variants by swapping armor addons dynamically at runtime.

## Dynamic Armor Variants Extended
`Dynamic Armor Variants Extended` is a fork of the original Dynamic Armor Variants by Parapets, extended with new features/native API.

Runtime compatibility intentionally stays close to the original project:
- plugin name remains `DynamicArmorVariants.dll`
- save data code remains compatible with the original DAV plugin
- config loading still uses `Data/SKSE/Plugins/DynamicArmorVariants`

## Updates From Original DAV
- Added a public SKSE messaging API for native integrations.
- Added support for registration/deletion of new variants at runtime.
- Added support for replacement identifiers that specify both owning armor and armor addon.
  - This lets variants render in completely disjoint slots from the original armor, so long as the actor doesn't have a conflict
- Added caching around variant resolution for minor performance improvements.

## Config Format
Variant replacement maps support two replacement identifier forms:

- `Plugin.esp|AddonLocalID`
  - Replaces with that `ARMA` and keeps the source armor as the default owner.
- `Plugin.esp|ArmorLocalID|Plugin.esp|AddonLocalID`
  - Replaces with that `ARMA` and explicitly supplies the owning `ARMO` to use while visiting the addon.

This applies to values in both `replaceByForm` and `replaceBySlot`, and to arrays of those values.

Example:

```json
{
  "replaceByForm": {
    "Skyrim.esm|0010D6A1": [
      "Dragonborn.esm|0001C655|Skyrim.esm|00027DE4",
      "Skyrim.esm|00027DE5"
    ]
  }
}
```

## Requirements
- [xmake](https://xmake.io/)
- Visual Studio 2022 with Desktop development with C++

## Setup
```bash
git submodule update --init --recursive
```

## Build
From Windows:

```powershell
xmake f -y -m release
xmake build -y
```

From WSL:

```bash
./scripts/build.sh
```

The plugin DLL is produced under `build/windows/x64/<mode>/DynamicArmorVariants.dll`.

## Build And Deploy
To build from WSL and deploy into the `pt_test` MO2 modlist:

```bash
./scripts/build-deploy.sh
./scripts/build-deploy.sh --clean
./scripts/build-deploy.sh debug
```

By default this deploys to `/mnt/f/games/skyrim/modlists/pt_test/mods/Dynamic Armor Variants Extended`. Set `MOD_DIR` to override the destination.
The default deploy mode is `releasedbg`, so the script also deploys `DynamicArmorVariants.pdb` when available. Use `./scripts/build-deploy.sh release` for a stripped release build.

The deploy step copies the built DLL, the repository `data/` tree, and compiled Papyrus output.

## Build And Package
To build a release archive:

```bash
./scripts/build-package.sh
```

This writes a zip under `dist/` named like:
`Dynamic Armor Variants Extended v1.1.0.zip`

The package script will fail unless:
- the worktree is clean
- `HEAD` has exactly one tag
- that tag is valid semver

The release package includes:
- a FOMOD installer layout compatible with the original DAV package structure
- `DynamicArmorVariants.dll`
- `DynamicArmorVariants.pdb`
- compiled Papyrus `.pex`
- Papyrus source `.psc`

## Lint And Format
```bash
./scripts/format.sh
./scripts/format.sh --check
./scripts/lint.sh
./scripts/lint.sh src/main/DynamicArmorManager.cpp
```

From WSL, `format.sh` prefers Linux `clang-format` when available. `lint.sh` prefers the
Visual Studio LLVM `x64` `clang-tidy.exe` because the generic Visual Studio `Llvm/bin`
binary is a 32-bit build that crashes in this environment.

`lint.sh` also passes `/Y-` to disable MSVC PCH use during linting. xmake's generated
`compile_commands.json` points `clang-tidy` at a PCH path that does not exist, so disabling
PCH is the reliable way to lint these translation units.

## Public API
Dynamic Armor Variants Extended exposes a versioned SKSE messaging API for native integrations.

Public consumer files:
- `include/DynamicArmorVariantsExtendedAPI.h`
- `include/DynamicArmorVariantsExtendedAPI.cpp`

Fetch the interface after `SKSE::MessagingInterface::kPostLoad`:

```cpp
auto* dav = DynamicArmorVariantsExtendedAPI::GetDynamicArmorVariantsExtendedInterface001();
```

Interface `001` exposes:
- `RegisterVariantJson(name, variantJson)`
- `DeleteVariant(name)`
- `SetVariantConditionsJson(name, conditionsJson)`
- `RefreshActor(actor)`
- `ApplyVariantOverride(actor, variantName, keepExistingOverrides = false)`
- `RemoveVariantOverride(actor, variantName)`

`RegisterVariantJson` expects the same JSON object shape as a single entry in the `variants` array from the config files. If the variant already exists, it is fully overwritten.

`SetVariantConditionsJson` accepts either:
- a raw JSON array of condition strings
- a JSON object with `conditions` and optional `refs`

Example conditions payload:

```json
{
  "refs": {
    "target": "Skyrim.esm|00000007"
  },
  "conditions": [
    "GetDistance TARGET <= 256",
    "IsSneaking == 1"
  ]
}
```
