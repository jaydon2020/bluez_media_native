#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

BUILD_DIR="${1:-build-asan}"

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

CC="${CC:-$(find_tool clang-21 clang-20 clang-19 clang || true)}"
CXX="${CXX:-$(find_tool clang++-21 clang++-20 clang++-19 clang++ || true)}"

if [[ -z "${CC}" || -z "${CXX}" ]]; then
    echo "ERROR: clang compiler not found. Install clang or set CC/CXX." >&2
    exit 1
fi

echo "Using C compiler: ${CC}"
echo "Using CXX compiler: ${CXX}"

cmake -B "${BUILD_DIR}" "${ROOT_DIR}/native" -GNinja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_COMPILER="${CXX}" \
    -DCMAKE_C_COMPILER="${CC}" \
    -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
    -DCMAKE_C_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
    -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined" \
    -DBUILD_TESTING=ON

cmake --build "${BUILD_DIR}" --parallel
ctest --test-dir "${BUILD_DIR}" --output-on-failure -j4
