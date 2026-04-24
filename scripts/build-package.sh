#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

MODE="releasedbg"
PLUGIN_NAME="DynamicArmorVariants"
MOD_NAME="Dynamic Armor Variants Extended"
MOD_AUTHOR="Parapets, PenguinToast"
MOD_DESCRIPTION="Framework for mods to define armor variants by swapping armor addons dynamically at runtime."
DIST_DIR="${REPO_ROOT}/dist"
STAGE_DIR="${DIST_DIR}/.stage"
BUILD_DIR="${REPO_ROOT}/build/windows/x64/${MODE}"
PLUGIN_SRC="${BUILD_DIR}/${PLUGIN_NAME}.dll"
PDB_SRC="${BUILD_DIR}/${PLUGIN_NAME}.pdb"
PAPYRUS_BUILD_DIR="${REPO_ROOT}/build/papyrus"

while (($#)); do
    case "$1" in
        --clean)
            rm -rf "${REPO_ROOT}/build" "${REPO_ROOT}/.xmake" "${DIST_DIR}"
            ;;
        *)
            echo "Usage: $0 [--clean]" >&2
            exit 2
            ;;
    esac
    shift
done

if ! command -v powershell.exe >/dev/null 2>&1; then
    echo "powershell.exe is required to invoke the Windows toolchain from WSL." >&2
    exit 1
fi

if ! command -v wslpath >/dev/null 2>&1; then
    echo "wslpath is required to convert paths for Windows tools." >&2
    exit 1
fi

normalize_repo_url() {
    local remote_url="$1"
    if [[ "${remote_url}" =~ ^git@github\.com:(.+)/(.+)\.git$ ]]; then
        printf 'https://github.com/%s/%s\n' "${BASH_REMATCH[1]}" "${BASH_REMATCH[2]}"
    elif [[ "${remote_url}" =~ ^https://github\.com/(.+)/(.+)\.git$ ]]; then
        printf 'https://github.com/%s/%s\n' "${BASH_REMATCH[1]}" "${BASH_REMATCH[2]}"
    else
        printf '%s\n' "${remote_url}"
    fi
}

REMOTE_URL="$(normalize_repo_url "$(git -C "${REPO_ROOT}" remote get-url origin)")"
DAVE_BUILD_VERSION="$("${SCRIPT_DIR}/version.sh" --numeric)"
DAVE_BUILD_VERSION_STRING="$("${SCRIPT_DIR}/version.sh" --display)"
VERSION="${DAVE_BUILD_VERSION_STRING}"
ARCHIVE_NAME="${MOD_NAME} v${VERSION}.zip"
ARCHIVE_PATH="${DIST_DIR}/${ARCHIVE_NAME}"
WIN_ARCHIVE_PATH="$(wslpath -w "${ARCHIVE_PATH}")"
WIN_STAGE_DIR="$(wslpath -w "${STAGE_DIR}")"

"${SCRIPT_DIR}/build.sh" "${MODE}"
"${SCRIPT_DIR}/build-papyrus.sh"

if [[ ! -f "${PLUGIN_SRC}" ]]; then
    echo "Build succeeded but ${PLUGIN_SRC} was not found." >&2
    exit 1
fi

rm -rf "${STAGE_DIR}"
mkdir -p \
    "${STAGE_DIR}/Data/Scripts" \
    "${STAGE_DIR}/Data/Source/Scripts" \
    "${STAGE_DIR}/Data/SKSE/Plugins" \
    "${STAGE_DIR}/Data/SKSE/Plugins/DynamicArmorVariants" \
    "${STAGE_DIR}/SkillLevelingHookEnabled" \
    "${STAGE_DIR}/UIExtensions_Menu/Interface/Translations" \
    "${STAGE_DIR}/UIExtensions_Menu/MCM/Config/DynamicArmorMenu" \
    "${STAGE_DIR}/UIExtensions_Menu/Scripts" \
    "${STAGE_DIR}/UIExtensions_Menu/Source/Scripts" \
    "${STAGE_DIR}/HiddenEquipment/Interface/Translations" \
    "${STAGE_DIR}/HiddenEquipment/SKSE/Plugins/DynamicArmorVariants" \
    "${STAGE_DIR}/fomod"

cp -R "${REPO_ROOT}/data/fomod/." "${STAGE_DIR}/fomod/"

cp "${PLUGIN_SRC}" "${STAGE_DIR}/Data/SKSE/Plugins/${PLUGIN_NAME}.dll"
if [[ -f "${PDB_SRC}" ]]; then
    cp "${PDB_SRC}" "${STAGE_DIR}/Data/SKSE/Plugins/${PLUGIN_NAME}.pdb"
fi
cp "${REPO_ROOT}/data/SKSE/Plugins/DynamicArmorVariants/settings.json" \
    "${STAGE_DIR}/Data/SKSE/Plugins/DynamicArmorVariants/"
cp -R "${REPO_ROOT}/installer/SkillLevelingHookEnabled/." \
    "${STAGE_DIR}/SkillLevelingHookEnabled/"

cp "${PAPYRUS_BUILD_DIR}/Scripts/DynamicArmor.pex" "${STAGE_DIR}/Data/Scripts/"
cp "${PAPYRUS_BUILD_DIR}/Source/Scripts/DynamicArmor.psc" "${STAGE_DIR}/Data/Source/Scripts/"

cp "${PAPYRUS_BUILD_DIR}/Scripts/DynamicArmor_MCM.pex" "${STAGE_DIR}/UIExtensions_Menu/Scripts/"
cp "${PAPYRUS_BUILD_DIR}/Scripts/DynamicArmor_Menu.pex" "${STAGE_DIR}/UIExtensions_Menu/Scripts/"
cp "${PAPYRUS_BUILD_DIR}/Scripts/DynamicArmor_MenuPower.pex" "${STAGE_DIR}/UIExtensions_Menu/Scripts/"
cp "${PAPYRUS_BUILD_DIR}/Source/Scripts/DynamicArmor_MCM.psc" "${STAGE_DIR}/UIExtensions_Menu/Source/Scripts/"
cp "${PAPYRUS_BUILD_DIR}/Source/Scripts/DynamicArmor_Menu.psc" "${STAGE_DIR}/UIExtensions_Menu/Source/Scripts/"
cp "${PAPYRUS_BUILD_DIR}/Source/Scripts/DynamicArmor_MenuPower.psc" "${STAGE_DIR}/UIExtensions_Menu/Source/Scripts/"

cp "${REPO_ROOT}/data/DynamicArmorMenu.esp" "${STAGE_DIR}/UIExtensions_Menu/"
cp "${REPO_ROOT}/data/Interface/Translations/DynamicArmorMenu_ENGLISH.txt" \
    "${STAGE_DIR}/UIExtensions_Menu/Interface/Translations/"
cp -R "${REPO_ROOT}/data/MCM/Config/DynamicArmorMenu/." \
    "${STAGE_DIR}/UIExtensions_Menu/MCM/Config/DynamicArmorMenu/"

cp "${REPO_ROOT}/data/DAV_HiddenEquipment.esp" "${STAGE_DIR}/HiddenEquipment/"
cp "${REPO_ROOT}/data/Interface/Translations/DAV_HiddenEquipment_ENGLISH.txt" \
    "${STAGE_DIR}/HiddenEquipment/Interface/Translations/"
cp "${REPO_ROOT}/data/SKSE/Plugins/DynamicArmorVariants/DAV_HiddenEquipment.json" \
    "${STAGE_DIR}/HiddenEquipment/SKSE/Plugins/DynamicArmorVariants/"

python3 - <<'PY' "${STAGE_DIR}/fomod"
from pathlib import Path
import sys

fomod_dir = Path(sys.argv[1])
if fomod_dir.is_dir():
    for xml_path in fomod_dir.glob("*.xml"):
        raw = xml_path.read_bytes()
        text = None
        if raw.startswith(b"\xff\xfe\xef\xbb\xbf"):
            text = raw[2:].decode("utf-8-sig")
        for encoding in ("utf-8-sig", "utf-16", "utf-16-le", "utf-16-be"):
            if text is not None:
                break
            try:
                text = raw.decode(encoding)
                break
            except UnicodeDecodeError:
                continue
        if text is None:
            raise SystemExit(f"Could not decode {xml_path}")
        xml_path.write_text(text.lstrip("\ufeff"), encoding="utf-8", newline="\n")
PY

cat > "${STAGE_DIR}/fomod/info.xml" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<fomod>
  <Name>${MOD_NAME}</Name>
  <Author>${MOD_AUTHOR}</Author>
  <Version>${VERSION}</Version>
  <Website>${REMOTE_URL}</Website>
  <Description>${MOD_DESCRIPTION}</Description>
</fomod>
EOF

mkdir -p "${DIST_DIR}"
rm -f "${ARCHIVE_PATH}"

POWERSHELL_ZIP_CMD="
\$ErrorActionPreference = 'Stop'
if (Test-Path -LiteralPath '$WIN_ARCHIVE_PATH') {
  Remove-Item -LiteralPath '$WIN_ARCHIVE_PATH' -Force
}
Compress-Archive -Path '$WIN_STAGE_DIR\\*' -DestinationPath '$WIN_ARCHIVE_PATH' -CompressionLevel Optimal
"
powershell.exe -NoProfile -Command "${POWERSHELL_ZIP_CMD}"

echo "Built ${PLUGIN_NAME} (${MODE})"
echo "Packaged mod to ${ARCHIVE_PATH}"
