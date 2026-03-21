#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

DEFAULT_GAME_ROOT="/mnt/f/SteamLibrary/steamapps/common/Skyrim Special Edition"
DEFAULT_SKSE_SOURCE_DIR="/mnt/f/games/skyrim/modlists/pt_test/mods/Skyrim Script Extender (SKSE64) - Scripts/Scripts/Source"
GAME_ROOT="${GAME_ROOT:-$DEFAULT_GAME_ROOT}"
SKSE_SOURCE_DIR="${SKSE_SOURCE_DIR:-$DEFAULT_SKSE_SOURCE_DIR}"
PAPYRUS_COMPILER_VERSION="${PAPYRUS_COMPILER_VERSION:-2026.03.15}"
PAPYRUS_COMPILER_DIR="${REPO_ROOT}/tools/papyrus-compiler"
PAPYRUS_COMPILER_BIN="${PAPYRUS_COMPILER_DIR}/papyrus"
PAPYRUS_OUTPUT_DIR="${REPO_ROOT}/build/papyrus"
PAPYRUS_SOURCE_STAGE_DIR="${PAPYRUS_OUTPUT_DIR}/Source/Scripts"
PAPYRUS_PEX_STAGE_DIR="${PAPYRUS_OUTPUT_DIR}/Scripts"
GAME_SOURCE_DIR="${GAME_ROOT}/Data/Source/Scripts"
LOCAL_HEADER_DIR="${SCRIPT_DIR}/headers"

if ! command -v gh >/dev/null 2>&1; then
    echo "gh is required to download papyrus-compiler releases." >&2
    exit 1
fi

if [[ ! -d "${GAME_SOURCE_DIR}" ]]; then
    echo "Papyrus source headers not found at ${GAME_SOURCE_DIR}." >&2
    echo "Set GAME_ROOT to your Skyrim install root." >&2
    exit 1
fi

if [[ ! -d "${SKSE_SOURCE_DIR}" ]]; then
    echo "SKSE Papyrus source headers not found at ${SKSE_SOURCE_DIR}." >&2
    echo "Set SKSE_SOURCE_DIR to your extracted SKSE Scripts/Source directory." >&2
    exit 1
fi

mkdir -p "${PAPYRUS_OUTPUT_DIR}" "${PAPYRUS_PEX_STAGE_DIR}" "${PAPYRUS_SOURCE_STAGE_DIR}" "${PAPYRUS_COMPILER_DIR}"

PAPYRUS_SOURCES=(
    "${SCRIPT_DIR}/DynamicArmor.psc"
    "${SCRIPT_DIR}/DynamicArmor_MCM.psc"
    "${SCRIPT_DIR}/DynamicArmor_Menu.psc"
    "${SCRIPT_DIR}/DynamicArmor_MenuPower.psc"
)

if [[ ! -x "${PAPYRUS_COMPILER_BIN}" ]]; then
    TMP_DIR="$(mktemp -d)"
    trap 'rm -rf "${TMP_DIR}"' EXIT
    gh release download "${PAPYRUS_COMPILER_VERSION}" \
        --repo russo-2025/papyrus-compiler \
        --pattern 'papyrus-compiler-ubuntu.tar.gz' \
        --dir "${TMP_DIR}"
    tar -xzf "${TMP_DIR}/papyrus-compiler-ubuntu.tar.gz" -C "${TMP_DIR}"
    install -m 0755 "${TMP_DIR}/papyrus-compiler/papyrus" "${PAPYRUS_COMPILER_BIN}"
fi

mkdir -p "${PAPYRUS_PEX_STAGE_DIR}" "${PAPYRUS_SOURCE_STAGE_DIR}"

latest_mtime() {
    local latest=0
    local path=""

    for path in "$@"; do
        if [[ -e "${path}" ]]; then
            local mtime
            mtime="$(stat -c '%Y' "${path}")"
            if ((mtime > latest)); then
                latest="${mtime}"
            fi
        fi
    done

    printf '%s\n' "${latest}"
}

mapfile -t LOCAL_HEADERS < <(find "${LOCAL_HEADER_DIR}" -type f -name '*.psc' | sort)
LOCAL_HEADERS_MTIME="$(latest_mtime "${LOCAL_HEADERS[@]}")"

compiled_count=0
skipped_count=0

for source_file in "${PAPYRUS_SOURCES[@]}"; do
    source_name="$(basename "${source_file}")"
    source_stem="${source_name%.psc}"
    pex_file="${PAPYRUS_PEX_STAGE_DIR}/${source_stem}.pex"

    source_mtime="$(stat -c '%Y' "${source_file}")"
    pex_mtime=0
    if [[ -f "${pex_file}" ]]; then
        pex_mtime="$(stat -c '%Y' "${pex_file}")"
    fi

    if [[ ! -f "${pex_file}" ]] || ((source_mtime > pex_mtime)) || ((LOCAL_HEADERS_MTIME > pex_mtime)); then
        "${PAPYRUS_COMPILER_BIN}" compile \
            -nocache \
            -i "${source_file}" \
            -o "${PAPYRUS_PEX_STAGE_DIR}" \
            -h "${GAME_SOURCE_DIR}" \
            -h "${SKSE_SOURCE_DIR}" \
            -h "${LOCAL_HEADER_DIR}"
        ((compiled_count += 1))
    else
        ((skipped_count += 1))
    fi
done

for source_file in "${PAPYRUS_SOURCES[@]}"; do
    cp -a "${source_file}" "${PAPYRUS_SOURCE_STAGE_DIR}/"
done

echo "Compiled Papyrus scripts to ${PAPYRUS_PEX_STAGE_DIR}"
echo "Staged Papyrus sources to ${PAPYRUS_SOURCE_STAGE_DIR}"
echo "Papyrus compile summary: ${compiled_count} compiled, ${skipped_count} skipped"
