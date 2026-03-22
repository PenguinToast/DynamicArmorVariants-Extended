#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

MODE="release"
CLEAN=0

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

if ! command -v powershell.exe >/dev/null 2>&1; then
    echo "powershell.exe is required to invoke the Windows toolchain from WSL." >&2
    exit 1
fi

if ! command -v wslpath >/dev/null 2>&1; then
    echo "wslpath is required to convert the repo path for Windows xmake." >&2
    exit 1
fi

DAVE_BUILD_VERSION="$("${SCRIPT_DIR}/version.sh" --numeric)"
DAVE_BUILD_VERSION_STRING="$("${SCRIPT_DIR}/version.sh" --display)"
WIN_REPO_ROOT="$(wslpath -w "$REPO_ROOT")"

if ((CLEAN)); then
    rm -rf "${REPO_ROOT}/build" "${REPO_ROOT}/.xmake"
fi

POWERSHELL_CMD="
\$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath '$WIN_REPO_ROOT'
\$env:DAVE_BUILD_VERSION = '$DAVE_BUILD_VERSION'
\$env:DAVE_BUILD_VERSION_STRING = '$DAVE_BUILD_VERSION_STRING'
xmake f -y -m '$MODE'
xmake build -y
"

powershell.exe -NoProfile -Command "$POWERSHELL_CMD"

echo "Built DynamicArmorVariants (${MODE})"
echo "Version ${DAVE_BUILD_VERSION_STRING}"
