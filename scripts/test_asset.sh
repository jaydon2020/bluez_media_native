#!/bin/bash
set -euo pipefail
project_dir="$(cd "$(dirname "$0")/.." && pwd)"
consumer_dir=$(mktemp -d)
trap 'rm -rf "$consumer_dir"' EXIT
unset BLUEZ_MEDIA_LIB
mkdir -p "$consumer_dir/bin"
cat > "$consumer_dir/pubspec.yaml" <<YAML
name: native_asset_consumer
environment:
  sdk: ^3.10.1
dependencies:
  bluez_media_native:
    path: '$project_dir'
YAML
cp "$project_dir/test/support/load_asset.dart" "$consumer_dir/bin/main.dart"
cd "$consumer_dir"
dart pub get
dart build cli --target bin/main.dart --output output
mv output/bundle relocated
./relocated/bin/main
