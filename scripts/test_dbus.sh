#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
if [[ "${BLUEZ_TEST_BUS:-}" != 1 ]]; then
  export BLUEZ_TEST_BUS=1
  exec dbus-run-session -- "$0"
fi
export DBUS_SYSTEM_BUS_ADDRESS="$DBUS_SESSION_BUS_ADDRESS"
export BLUEZ_MEDIA_LIB="$PWD/build/libbluez_media_native.so"
fixture_dir=$(mktemp -d)
export BLUEZ_TEST_READY="$fixture_dir/ready"
/usr/bin/python3 test/support/bluez_service.py &
fixture_pid=$!
trap 'kill "$fixture_pid"; rm -rf "$fixture_dir"' EXIT
for ((i=0; i<100; i++)); do
  [[ -e "$BLUEZ_TEST_READY" ]] && break
  kill -0 "$fixture_pid"
  sleep 0.05
done
[[ -e "$BLUEZ_TEST_READY" ]]
dart test test/integration --reporter expanded
