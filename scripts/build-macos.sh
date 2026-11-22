#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON \
  '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64' -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0
cmake --build build --config Release --parallel 4
ctest --test-dir build -C Release --output-on-failure

artifacts="build/LYBLiveVocal_artefacts/Release"
stage="$(mktemp -d "$PWD/build/pkg-root.XXXXXX")"
mkdir -p "$stage/Library/Audio/Plug-Ins/Components" "$stage/Library/Audio/Plug-Ins/VST3" "$stage/Applications" dist
ditto "$artifacts/AU/LYB Live Vocal.component" "$stage/Library/Audio/Plug-Ins/Components/LYB Live Vocal.component"
ditto "$artifacts/VST3/LYB Live Vocal.vst3" "$stage/Library/Audio/Plug-Ins/VST3/LYB Live Vocal.vst3"
ditto "$artifacts/Standalone/LYB Live Vocal.app" "$stage/Applications/LYB Live Vocal.app"
for bundle in "$stage/Library/Audio/Plug-Ins/Components/LYB Live Vocal.component" \
              "$stage/Library/Audio/Plug-Ins/VST3/LYB Live Vocal.vst3" \
              "$stage/Applications/LYB Live Vocal.app"; do
  codesign --force --deep --sign - "$bundle"
  codesign --verify --deep --strict "$bundle"
done
# This runs on an ephemeral CI runner. Validate the AU before packaging.
mkdir -p "$HOME/Library/Audio/Plug-Ins/Components"
ditto "$stage/Library/Audio/Plug-Ins/Components/LYB Live Vocal.component" \
      "$HOME/Library/Audio/Plug-Ins/Components/LYB Live Vocal.component"
killall -9 AudioComponentRegistrar 2>/dev/null || true
auval -v aufx Lyvl Lybe
pkgbuild --root "$stage" --identifier com.lyb.livevocal.beta --version 0.1.1 \
  --install-location / "dist/LYB-Live-Vocal-0.1.1-macOS-Universal-Beta.pkg"
pkgutil --payload-files "dist/LYB-Live-Vocal-0.1.1-macOS-Universal-Beta.pkg"
