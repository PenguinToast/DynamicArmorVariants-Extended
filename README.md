Framework for mods to define variants of armors by swapping armor addons dynamically at runtime.

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

By default this deploys to `/mnt/f/games/skyrim/modlists/pt_test/mods/dynamic_armor_variants_ng`. Set `MOD_DIR` to override the destination.

The deploy step copies the built DLL, the repository `data/` tree, and the existing `DynamicArmor*.pex` scripts from `/mnt/f/games/skyrim/modlists/pt_test/mods/Dynamic Armor Variants/Scripts`. Set `PAPYRUS_SOURCE_MOD_DIR` to override that source mod path.

## Public API
DAV-NG exposes a versioned SKSE messaging API modeled after Modex.

Public consumer files:
- `include/DynamicArmorVariantsAPI.h`
- `include/DynamicArmorVariantsAPI.cpp`

Fetch the interface after `SKSE::MessagingInterface::kPostLoad`:

```cpp
auto* dav = DynamicArmorVariantsAPI::GetDynamicArmorVariantsInterface001();
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
