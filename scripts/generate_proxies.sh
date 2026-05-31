#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

_find_xml2cpp() {
    local candidates=(
        "${ROOT_DIR}/build/tools/sdbus-c++-xml2cpp"
        "${ROOT_DIR}/build-codegen/tools/sdbus-c++-xml2cpp"
        "${ROOT_DIR}/build/native/third_party/sdbus-cpp/tools/sdbus-c++-xml2cpp"
        "${ROOT_DIR}/cmake-build-debug/tools/sdbus-c++-xml2cpp"
        "${ROOT_DIR}/cmake-build-release/tools/sdbus-c++-xml2cpp"
    )
    for candidate in "${candidates[@]}"; do
        if [[ -x "${candidate}" ]]; then
            echo "${candidate}"
            return 0
        fi
    done
    if command -v sdbus-c++-xml2cpp &>/dev/null; then
        command -v sdbus-c++-xml2cpp
        return 0
    fi
    echo "ERROR: sdbus-c++-xml2cpp not found. Build the generator first or install sdbus-c++." >&2
    return 1
}

XML2CPP="$(_find_xml2cpp)"
echo "Using: ${XML2CPP}"

cd "${ROOT_DIR}"
mkdir -p native/generated

${XML2CPP} --verbose --proxy=native/generated/media1_proxy.h \
    interfaces/org.bluez.Media1.xml

${XML2CPP} --verbose --proxy=native/generated/media_control1_proxy.h \
    interfaces/org.bluez.MediaControl1.xml

${XML2CPP} --verbose --proxy=native/generated/media_player1_proxy.h \
    interfaces/org.bluez.MediaPlayer1.xml

${XML2CPP} --verbose --proxy=native/generated/media_folder1_proxy.h \
    interfaces/org.bluez.MediaFolder1.xml

${XML2CPP} --verbose --proxy=native/generated/media_item1_proxy.h \
    interfaces/org.bluez.MediaItem1.xml

${XML2CPP} --verbose --proxy=native/generated/media_endpoint1_proxy.h \
    interfaces/org.bluez.MediaEndpoint1.xml

${XML2CPP} --verbose --proxy=native/generated/media_transport1_proxy.h \
    interfaces/org.bluez.MediaTransport1.xml

echo "Proxy generation complete."
