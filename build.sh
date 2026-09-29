#!/usr/bin/env bash
# Builds PedalCues (VST3 + AU + Standalone) and installs the plugins into ~/Library/Audio/Plug-Ins.
set -euo pipefail
cd "$(dirname "$0")"

if [ ! -f external/JUCE/CMakeLists.txt ]; then
  git clone --depth 1 --branch 8.0.8 https://github.com/juce-framework/JUCE.git external/JUCE
fi

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

echo
echo "Built. Plugins copied to ~/Library/Audio/Plug-Ins (VST3/Components)."
echo "Standalone app: build/PedalCues_artefacts/Release/Standalone/PedalCues.app"
echo "In Reaper: Options > Preferences > Plug-ins > VST > Re-scan."
