#!/bin/sh
# Installs (or updates) PedalCues for the current user. Shipped as install.sh in PedalCues-Linux.zip.
#
#   ./install.sh               the VST3 to ~/.vst3, the LV2 to ~/.lv2, the app to ~/.local/bin (+ a menu entry)
#   ./install.sh --uninstall   removes them again
#
# An older version is replaced. Your projects and settings are kept. Quit your DAW first: a DAW that has PedalCues
# loaded keeps using the old version until it restarts.
set -eu

here=$(cd "$(dirname "$0")" && pwd)
vst3="$HOME/.vst3/PedalCues.vst3"
lv2="$HOME/.lv2/PedalCues.lv2"
bin="$HOME/.local/bin/PedalCues"
desktop="$HOME/.local/share/applications/pedalcues.desktop"

if [ "${1:-}" = "--uninstall" ]; then
    rm -rf "$vst3" "$lv2" "$bin" "$desktop"
    echo "PedalCues removed (your projects and settings are kept)."
    exit 0
fi

for f in PedalCues.vst3 PedalCues.lv2 PedalCues; do
    if [ ! -e "$here/$f" ]; then
        echo "Can't find $f next to install.sh: run it from the unzipped PedalCues-Linux folder." >&2
        exit 1
    fi
done

mkdir -p "$HOME/.vst3" "$HOME/.lv2" "$HOME/.local/bin" "$HOME/.local/share/applications"
rm -rf "$vst3" "$lv2"
cp -R "$here/PedalCues.vst3" "$vst3"
cp -R "$here/PedalCues.lv2" "$lv2"
cp "$here/PedalCues" "$bin"
chmod +x "$bin"

cat > "$desktop" <<EOF
[Desktop Entry]
Type=Application
Name=PedalCues
Comment=Drag-and-drop MIDI cues for your pedals
Exec=$bin
Terminal=false
Categories=AudioVideo;Audio;Music;
EOF

echo "PedalCues installed:"
echo "  VST3 plugin   $vst3"
echo "  LV2 plugin    $lv2"
echo "  App           $bin (also in your applications menu)"
echo "Restart your DAW (or re-scan plug-ins) to load the new version."
