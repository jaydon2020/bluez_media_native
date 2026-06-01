#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

BUILD_DIR="${1:-build}"

if [[ ! -f "${BUILD_DIR}/compile_commands.json" ]]; then
    echo "ERROR: compile_commands.json not found in ${BUILD_DIR}."
    echo "Build the project first with -DCMAKE_EXPORT_COMPILE_COMMANDS=ON."
    exit 1
fi

find_tool() {
    local tool
    for tool in "$@"; do
        if command -v "${tool}" &>/dev/null; then
            command -v "${tool}"
            return 0
        fi
    done
    return 1
}

CLANG_TIDY="${CLANG_TIDY:-$(find_tool clang-tidy-21 clang-tidy-20 clang-tidy-19 clang-tidy || true)}"
if [[ -z "${CLANG_TIDY}" ]]; then
    echo "ERROR: clang-tidy not found. Install clang-tidy or set CLANG_TIDY." >&2
    exit 1
fi

echo "Using: ${CLANG_TIDY}"

find "${ROOT_DIR}/native/src" "${ROOT_DIR}/native/include" \
    \( -name '*.c' -o -name '*.cpp' -o -name '*.h' \) \
    ! -path '*/third_party/*' \
    ! -path '*/generated/*' \
    ! -path '*/internal/*' \
    ! -name 'dart_api.h' \
    ! -name 'dart_api_dl.h' \
    ! -name 'dart_api_dl.c' \
    ! -name 'dart_native_api.h' \
    ! -name 'dart_version.h' \
    -print | sort | \
    xargs "${CLANG_TIDY}" -p "${BUILD_DIR}" --warnings-as-errors='*' 2>&1

echo "clang-tidy passed."
