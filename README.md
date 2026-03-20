Framework for mods to define variants of armors by swapping armor addons dynamically at runtime.

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

By default this deploys to `/mnt/f/games/skyrim/modlists/pt_test/mods/dynamic_armor_variants_extended`. Set `MOD_DIR` to override the destination.
The default deploy mode is `releasedbg`, so the script also deploys `DynamicArmorVariants.pdb` when available. Use `./scripts/build-deploy.sh release` for a stripped release build.

The deploy step copies the built DLL, the repository `data/` tree, and the existing `DynamicArmor*.pex` scripts from `/mnt/f/games/skyrim/modlists/pt_test/mods/Dynamic Armor Variants/Scripts`. Set `PAPYRUS_SOURCE_MOD_DIR` to override that source mod path.

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
Dynamic Armor Variants Extended exposes a versioned SKSE messaging API modeled after Modex.

Public consumer files:
- `include/DynamicArmorVariantsExtendedAPI.h`
- `include/DynamicArmorVariantsExtendedAPI.cpp`

Fetch the interface after `SKSE::MessagingInterface::kPostLoad`:

```cpp
auto* dav = DynamicArmorVariantsExtendedAPI::GetDynamicArmorVariantsExtendedInterface001();
```

Interface v001 exposes:
- `RegisterVariantJson(name, variantJson)`
- `DeleteVariant(name)`
- `SetVariantConditionsJson(name, conditionsJson)`

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
