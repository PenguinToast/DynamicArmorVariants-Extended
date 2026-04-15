# Build, Deploy, And Release

This document describes the WSL-based workflow for:

- local builds with `scripts/build.sh`
- build-and-deploy runs with `scripts/build-deploy.sh`
- dist package creation with `scripts/build-package.sh`
- tag-based GitHub releases with `gh`

## Prerequisites

- Run from WSL in the repo root.
- `powershell.exe` must be available in `PATH`.
- `wslpath` must be available.
- `xmake` must be installed on the Windows side, since `build.sh` invokes it through PowerShell.
- `gh` should be authenticated for release work. `build-papyrus.sh` also uses `gh` to download the Papyrus compiler when needed.

## Versioning

All three build flows use `scripts/version.sh`.

- If `HEAD` is clean and points at a semver tag like `v1.7.1`, the display version is `1.7.1`.
- Otherwise the display version is `<base>-dev+<shortsha>` with an additional `.dirty` suffix when the worktree is dirty.

Examples:

- tagged clean release: `1.7.1`
- untagged dev build: `1.7.1-dev+ac4c1ea`
- dirty worktree: `1.7.1-dev+ac4c1ea.dirty`

For real release packages, keep the worktree clean and put exactly one release tag on `HEAD`.

## Build

`build.sh` builds the native plugin through Windows `xmake`.

```bash
./scripts/build.sh
./scripts/build.sh releasedbg
./scripts/build.sh release
./scripts/build.sh debug
./scripts/build.sh --clean releasedbg
```

Modes:

- `release`: optimized release build
- `debug`: debug build
- `releasedbg`: optimized build with debug info

Outputs:

- DLL: `build/windows/x64/<mode>/DynamicArmorVariants.dll`
- PDB when present: `build/windows/x64/<mode>/DynamicArmorVariants.pdb`

## Build And Deploy

`build-deploy.sh` builds the plugin, compiles Papyrus, then copies the mod payload into a target mod folder.

```bash
./scripts/build-deploy.sh
./scripts/build-deploy.sh release
./scripts/build-deploy.sh debug
./scripts/build-deploy.sh --clean releasedbg
```

Defaults:

- mode: `releasedbg`
- destination:
  `/mnt/f/games/skyrim/modlists/pt_test/mods/Dynamic Armor Variants Extended`

Override the destination with `MOD_DIR`:

```bash
MOD_DIR="/mnt/f/games/skyrim/modlists/some_profile/mods/Dynamic Armor Variants Extended" \
  ./scripts/build-deploy.sh
```

What gets deployed:

- `DynamicArmorVariants.dll`
- `DynamicArmorVariants.pdb` when present
- the repo `data/` tree
- compiled Papyrus scripts from `build/papyrus/Scripts`
- staged Papyrus sources from `build/papyrus/Source`

Notes:

- `build-deploy.sh` will fail if the destination file is locked by Skyrim, MO2, or another process.
- Use `release` when you explicitly want a stripped deploy without the usual `releasedbg` output.

## Build Dist Package

`build-package.sh` creates the release zip under `dist/`.

```bash
./scripts/build-package.sh
./scripts/build-package.sh --clean
```

Output:

- `dist/Dynamic Armor Variants Extended v<version>.zip`

The package contains:

- the FOMOD installer layout
- `DynamicArmorVariants.dll`
- `DynamicArmorVariants.pdb` when present
- Papyrus `.pex`
- Papyrus source `.psc`
- the UIExtensions and HiddenEquipment payloads staged by the script

Important:

- The archive version string comes from `scripts/version.sh`.
- If `HEAD` is not a clean semver tag, the zip name will be a dev build name instead of a release version.

## Release Workflow

Recommended release flow:

1. Make sure `main` contains the exact release commit.
2. Make sure the worktree is clean.
3. Optionally do a final local validation build:

```bash
./scripts/build-deploy.sh
```

4. Create or move the release tag:

```bash
git tag v1.7.1
```

If you intentionally need to retarget an existing tag:

```bash
git tag -f v1.7.1 HEAD
```

5. Push the branch and tag:

```bash
git push origin main
git push origin v1.7.1
```

If you retagged an existing release tag:

```bash
git push origin -f v1.7.1
```

6. Build the release package:

```bash
./scripts/build-package.sh
```

7. Create the GitHub release:

```bash
gh release create v1.7.1 \
  -R PenguinToast/DynamicArmorVariants-Extended \
  --title "v1.7.1" \
  --notes-file /path/to/release-notes.md
```

8. Upload the dist zip:

```bash
gh release upload v1.7.1 \
  "dist/Dynamic Armor Variants Extended v1.7.1.zip" \
  -R PenguinToast/DynamicArmorVariants-Extended
```

If you need to replace an existing asset:

```bash
gh release upload v1.7.1 \
  "dist/Dynamic Armor Variants Extended v1.7.1.zip" \
  -R PenguinToast/DynamicArmorVariants-Extended \
  --clobber
```

## Release Notes

A simple release notes file works well:

```md
## Changelog

- Brief summary of the main fix or feature.
- Additional compatibility or packaging changes.
- Any noteworthy risks or limitations.
```

Then create the release with:

```bash
gh release create v1.7.1 \
  -R PenguinToast/DynamicArmorVariants-Extended \
  --title "v1.7.1" \
  --notes-file release-notes.md
```

## Verification

Useful checks after publishing:

```bash
git tag --points-at HEAD
gh release view v1.7.1 -R PenguinToast/DynamicArmorVariants-Extended --json assets,url
sha256sum "dist/Dynamic Armor Variants Extended v1.7.1.zip"
```

Verify:

- `HEAD` has the intended release tag
- the release exists on GitHub
- the zip asset is attached
- the package checksum is recorded if you need one
