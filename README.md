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
