#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

MODE="${1:-check}"  # "check" or "fix"

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

CLANG_FORMAT="${CLANG_FORMAT:-$(find_tool clang-format-21 clang-format-20 clang-format-19 clang-format || true)}"
if [[ -z "${CLANG_FORMAT}" ]]; then
    echo "ERROR: clang-format not found. Install clang-format or set CLANG_FORMAT." >&2
    exit 1
fi

echo "Using: ${CLANG_FORMAT}"

FILES=$(find "${ROOT_DIR}/native/src" "${ROOT_DIR}/native/include" \
    \( -name '*.c' -o -name '*.cpp' -o -name '*.h' \) \
    ! -path '*/third_party/*' \
    ! -path '*/generated/*' \
    -print | sort)

if [[ "${MODE}" == "check" ]]; then
    echo "Checking format..."
    FAILED=0
    for f in ${FILES}; do
        if ! "${CLANG_FORMAT}" --dry-run --Werror "${f}" 2>/dev/null; then
            FAILED=1
        fi
    done
    if [[ ${FAILED} -eq 1 ]]; then
        echo "Format check failed. Run: ./scripts/clang_format.sh fix"
        exit 1
    fi
    echo "Format check passed."
elif [[ "${MODE}" == "fix" ]]; then
    echo "Formatting..."
    for f in ${FILES}; do
        "${CLANG_FORMAT}" -i "${f}"
    done
    echo "Done."
else
    echo "Usage: $0 [check|fix]"
    exit 1
fi
