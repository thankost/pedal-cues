#!/bin/bash
# Builds the macOS installer: PedalCues.app -> /Applications, VST3 and AU -> /Library/Audio/Plug-Ins.
#
#   Tools/make_macos_pkg.sh <folder with PedalCues.app, .vst3, .component> <version> <out.pkg>
#
# The bundles must already be signed (CI re-seals them ad hoc). The package itself isn't signed with a
# Developer ID, so macOS asks once: System Settings > Privacy & Security > Open Anyway.
set -euo pipefail

src="$1"; version="$2"; out="$3"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

component() {   # <bundle> <install location> <id suffix> [postinstall script]
    local bundle="$1" location="$2" id="com.athkost.pedalcues.$3"
    local root="$work/root-$3"
    mkdir -p "$root"
    ditto "$src/$bundle" "$root/$bundle"

    # Never "relocate": without this, Installer updates any copy of the bundle it finds elsewhere on the disk
    # (a build folder, a backup) instead of installing to the location below.
    pkgbuild --analyze --root "$root" "$work/$3.plist" > /dev/null
    plutil -replace 0.BundleIsRelocatable -bool NO "$work/$3.plist"

    local scripts=()
    if [ $# -ge 4 ]; then
        mkdir -p "$work/scripts-$3"
        printf '%s\n' "#!/bin/sh" "$4" "exit 0" > "$work/scripts-$3/postinstall"
        chmod +x "$work/scripts-$3/postinstall"
        scripts=(--scripts "$work/scripts-$3")
    fi

    pkgbuild --root "$root" --component-plist "$work/$3.plist" --install-location "$location" \
             --identifier "$id" --version "$version" ${scripts[@]+"${scripts[@]}"} "$work/$3.pkg" > /dev/null
}

component PedalCues.app       /Applications                       app
component PedalCues.vst3      /Library/Audio/Plug-Ins/VST3        vst3
# Make Logic and GarageBand notice the new AU without a restart.
component PedalCues.component /Library/Audio/Plug-Ins/Components  au "killall -9 AudioComponentRegistrar 2>/dev/null"

cat > "$work/distribution.xml" <<EOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
    <title>PedalCues $version</title>
    <options customize="allow" require-scripts="false" hostArchitectures="arm64,x86_64"/>
    <domains enable_anywhere="false" enable_currentUserHome="false" enable_localSystem="true"/>
    <os-version min="11.0"/>
    <choices-outline>
        <line choice="app"/>
        <line choice="vst3"/>
        <line choice="au"/>
    </choices-outline>
    <choice id="app" title="PedalCues app" description="The standalone app, in Applications.">
        <pkg-ref id="com.athkost.pedalcues.app"/>
    </choice>
    <choice id="vst3" title="VST3 plugin" description="For Reaper, Ableton Live, Cubase, Bitwig, Studio One and other VST3 hosts.">
        <pkg-ref id="com.athkost.pedalcues.vst3"/>
    </choice>
    <choice id="au" title="Audio Unit (AU) plugin" description="For Logic Pro, GarageBand and other AU hosts.">
        <pkg-ref id="com.athkost.pedalcues.au"/>
    </choice>
    <pkg-ref id="com.athkost.pedalcues.app" version="$version">app.pkg</pkg-ref>
    <pkg-ref id="com.athkost.pedalcues.vst3" version="$version">vst3.pkg</pkg-ref>
    <pkg-ref id="com.athkost.pedalcues.au" version="$version">au.pkg</pkg-ref>
</installer-gui-script>
EOF

productbuild --distribution "$work/distribution.xml" --package-path "$work" "$out" > /dev/null
echo "wrote $out"
