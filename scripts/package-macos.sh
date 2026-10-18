#!/usr/bin/env bash
# Signs, validates and packages Bouquet on macOS.
#   scripts/package-macos.sh [build-dir]    -> <build-dir>/Bouquet-macOS.pkg
# Run after: cmake -S . -B build -DCMAKE_BUILD_TYPE=Release "-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64"
#            cmake --build build --config Release
#
# Signing: ad hoc by default (enough for Apple Silicon to load it; Gatekeeper asks once).
# With an Apple Developer ID, set these and the installer is signed and notarized:
#   DEVELOPER_ID_APP        "Developer ID Application: Your Name (TEAMID)"
#   DEVELOPER_ID_INSTALLER  "Developer ID Installer: Your Name (TEAMID)"
#   NOTARY_APPLE_ID, NOTARY_TEAM_ID, NOTARY_PASSWORD (an app-specific password)
set -euo pipefail

BUILD="${1:-build}"
OUT="$(cd "$BUILD" && pwd)"
ART="$OUT/Bouquet"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VERSION="$(cat "$ROOT/VERSION")"

INSTRUMENT_AU="$ART/Bouquet_artefacts/Release/AU/Bouquet.component"
INSTRUMENT_VST3="$ART/Bouquet_artefacts/Release/VST3/Bouquet.vst3"
APP="$ART/Bouquet_artefacts/Release/Standalone/Bouquet.app"
FX_AU="$ART/BouquetFX_artefacts/Release/AU/Bouquet FX.component"
FX_VST3="$ART/BouquetFX_artefacts/Release/VST3/Bouquet FX.vst3"
BUNDLES=("$INSTRUMENT_AU" "$INSTRUMENT_VST3" "$APP" "$FX_AU" "$FX_VST3")

echo "== Architectures"
lipo -info "$INSTRUMENT_VST3/Contents/MacOS/Bouquet"

echo "== Signing"
for bundle in "${BUNDLES[@]}"; do
    if [ -n "${DEVELOPER_ID_APP:-}" ]; then
        codesign --force --deep --options runtime --timestamp --sign "$DEVELOPER_ID_APP" "$bundle"
    else
        # Ad hoc: required for Apple Silicon to load the binaries at all.
        codesign --force --deep --sign - "$bundle"
    fi
    codesign --verify --deep --strict "$bundle"
done

# Apple's Audio Unit validator (what Logic runs before loading a plugin).
echo "== auval"
mkdir -p "$HOME/Library/Audio/Plug-Ins/Components"
cp -R "$INSTRUMENT_AU" "$FX_AU" "$HOME/Library/Audio/Plug-Ins/Components/"
killall -9 AudioComponentRegistrar 2>/dev/null || true
sleep 2
auval -v aumu Bqt1 Bqut
auval -v aumf Bqfx Bqut

echo "== Installer"
STAGE="$(mktemp -d)"
mkdir -p "$STAGE/vst3" "$STAGE/au" "$STAGE/app" "$STAGE/assets"
cp -R "$INSTRUMENT_VST3" "$FX_VST3" "$STAGE/vst3/"
cp -R "$INSTRUMENT_AU" "$FX_AU" "$STAGE/au/"
cp -R "$APP" "$STAGE/app/"
# The recordings and flower renders, shared by every format.
cp -R "$ROOT/Bouquet/assets/flowers" "$STAGE/assets/Flowers"
cp -R "$ROOT/Bouquet/assets/samples" "$STAGE/assets/Samples"
test -f "$STAGE/assets/Flowers/daisy0.json"
test -f "$STAGE/assets/Samples/samples.json"

# Clears out plugins from earlier builds (Murmur, Simple Synth, Bouquet Real).
SCRIPTS="$STAGE/scripts"
mkdir -p "$SCRIPTS"
cp "$ROOT/scripts/macos-preinstall.sh" "$SCRIPTS/preinstall"
chmod +x "$SCRIPTS/preinstall"

# One component package per destination; bundles must install exactly where we say
# (by default pkgbuild "relocates" them to wherever an older copy happens to live).
build_component () {
    local name="$1" root="$2" location="$3"
    local plist="$STAGE/$name.plist"
    pkgbuild --analyze --root "$root" "$plist" >/dev/null
    local count
    count=$(/usr/libexec/PlistBuddy -c "Print" "$plist" | grep -c "BundleIsRelocatable" || true)
    for ((i = 0; i < count; i++)); do
        /usr/libexec/PlistBuddy -c "Set :$i:BundleIsRelocatable false" "$plist"
    done
    pkgbuild --root "$root" --component-plist "$plist" --identifier "com.bouquet.$name" \
             --version "$VERSION" --install-location "$location" ${4:+--scripts "$4"} "$STAGE/$name.pkg"
}
build_component vst3   "$STAGE/vst3"   "/Library/Audio/Plug-Ins/VST3" "$SCRIPTS"
build_component au     "$STAGE/au"     "/Library/Audio/Plug-Ins/Components"
build_component app    "$STAGE/app"    "/Applications"
pkgbuild --root "$STAGE/assets" --identifier "com.bouquet.assets" --version "$VERSION" \
         --install-location "/Library/Application Support/Bouquet" "$STAGE/assets.pkg"

cat > "$STAGE/welcome.txt" <<TXT
Bouquet $VERSION: a granular instrument in bloom.

This installs:
  - Bouquet and Bouquet FX as Audio Units (Logic Pro, GarageBand, Ableton Live)
  - Bouquet and Bouquet FX as VST3 (Ableton Live and other hosts)
  - the standalone Bouquet app
  - the instrument recordings and flower renders they share

It also removes Murmur, Simple Synth and Bouquet Real from earlier test builds.
After installing, rescan plug-ins in your DAW.
TXT

productbuild --synthesize --package "$STAGE/vst3.pkg" --package "$STAGE/au.pkg" --package "$STAGE/app.pkg" \
             --package "$STAGE/assets.pkg" "$STAGE/distribution.xml"
# Add a title and welcome page to the generated distribution.
sed -i '' 's#<installer-gui-script minSpecVersion="[0-9]*">#&<title>Bouquet</title><welcome file="welcome.txt" mime-type="text/plain"/>#' "$STAGE/distribution.xml"
PKG="$OUT/Bouquet-macOS.pkg"
productbuild --distribution "$STAGE/distribution.xml" --package-path "$STAGE" --resources "$STAGE" \
             ${DEVELOPER_ID_INSTALLER:+--sign "$DEVELOPER_ID_INSTALLER"} "$PKG"

if [ -n "${DEVELOPER_ID_INSTALLER:-}" ] && [ -n "${NOTARY_APPLE_ID:-}" ]; then
    echo "== Notarizing"
    xcrun notarytool submit "$PKG" --apple-id "$NOTARY_APPLE_ID" --team-id "$NOTARY_TEAM_ID" \
          --password "$NOTARY_PASSWORD" --wait
    xcrun stapler staple "$PKG"
fi

pkgutil --payload-files "$PKG" | grep -E "\.(vst3|component|app)$|samples.json|daisy0.json" || true
echo "== Done: $PKG"
