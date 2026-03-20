#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

MODE="releasedbg"
CLEAN=0
PLUGIN_NAME="DynamicArmorVariants"
DEFAULT_MOD_DIR="/mnt/f/games/skyrim/modlists/pt_test/mods/dynamic_armor_variants_extended"
MOD_DIR="${MOD_DIR:-$DEFAULT_MOD_DIR}"

while (($#)); do
    case "$1" in
        --clean)
            CLEAN=1
            ;;
        release|debug|releasedbg)
            MODE="$1"
            ;;
        *)
            echo "Usage: $0 [--clean] [release|debug|releasedbg]" >&2
            exit 2
            ;;
    esac
    shift
done

copy_file() {
    local src="$1"
    local dst="$2"

    if ! cp "$src" "$dst"; then
        echo "Failed to copy ${src} to ${dst}." >&2
        echo "The destination file may be locked by Skyrim, MO2, or another Windows process." >&2
        exit 1
    fi
}

copy_tree() {
    local src="$1"
    local dst="$2"

    mkdir -p "$dst"
    cp -a "${src}/." "$dst/"
}

BUILD_ARGS=()
if ((CLEAN)); then
    BUILD_ARGS+=("--clean")
fi
BUILD_ARGS+=("$MODE")

"${SCRIPT_DIR}/build.sh" "${BUILD_ARGS[@]}"
"${SCRIPT_DIR}/build-papyrus.sh"

BUILD_DIR="${REPO_ROOT}/build/windows/x64/${MODE}"
PAPYRUS_BUILD_DIR="${REPO_ROOT}/build/papyrus"
PLUGIN_SRC="${BUILD_DIR}/${PLUGIN_NAME}.dll"
PDB_SRC="${BUILD_DIR}/${PLUGIN_NAME}.pdb"
PLUGIN_DST_DIR="${MOD_DIR}/SKSE/Plugins"

if [[ ! -f "$PLUGIN_SRC" ]]; then
    echo "Build succeeded but ${PLUGIN_SRC} was not found." >&2
    exit 1
fi

mkdir -p "$PLUGIN_DST_DIR"
copy_file "$PLUGIN_SRC" "${PLUGIN_DST_DIR}/${PLUGIN_NAME}.dll"

if [[ -f "$PDB_SRC" ]]; then
    copy_file "$PDB_SRC" "${PLUGIN_DST_DIR}/${PLUGIN_NAME}.pdb"
else
    rm -f "${PLUGIN_DST_DIR}/${PLUGIN_NAME}.pdb"
fi

copy_tree "${REPO_ROOT}/data" "$MOD_DIR"

copy_tree "${PAPYRUS_BUILD_DIR}/Scripts" "${MOD_DIR}/Scripts"
copy_tree "${PAPYRUS_BUILD_DIR}/Source" "${MOD_DIR}/Source"

echo "Built ${PLUGIN_NAME} (${MODE})"
echo "Deployed plugin to ${PLUGIN_DST_DIR}/${PLUGIN_NAME}.dll"
echo "Deployed mod data to ${MOD_DIR}"
echo "Deployed Papyrus scripts from ${PAPYRUS_BUILD_DIR}"
