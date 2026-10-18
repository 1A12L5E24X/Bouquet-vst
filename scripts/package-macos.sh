#!/usr/bin/env bash
# Signs (ad hoc), validates and packages the Bouquet plugins on macOS.
#   scripts/package-macos.sh [build-dir]    -> <build-dir>/Bouquet-macOS.pkg
# Run after: cmake -S . -B build -DCMAKE_BUILD_TYPE=Release "-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64"
#            cmake --build build --config Release
set -euo pipefail

BUILD="${1:-build}"
OUT="$(cd "$BUILD" && pwd)"
ART="$OUT/Bouquet"
VERSION="0.3.0"

INSTRUMENT_AU="$ART/Bouquet_artefacts/Release/AU/Bouquet.component"
INSTRUMENT_VST3="$ART/Bouquet_artefacts/Release/VST3/Bouquet.vst3"
APP="$ART/Bouquet_artefacts/Release/Standalone/Bouquet.app"
FX_AU="$ART/BouquetFX_artefacts/Release/AU/Bouquet FX.component"
FX_VST3="$ART/BouquetFX_artefacts/Release/VST3/Bouquet FX.vst3"

# Bouquet Real: the same plugins with pre-rendered photoreal flowers inside the bundles.
REAL_AU="$ART/BouquetReal_artefacts/Release/AU/Bouquet Real.component"
REAL_VST3="$ART/BouquetReal_artefacts/Release/VST3/Bouquet Real.vst3"
REAL_APP="$ART/BouquetReal_artefacts/Release/Standalone/Bouquet Real.app"
REAL_FX_AU="$ART/BouquetRealFX_artefacts/Release/AU/Bouquet Real FX.component"
REAL_FX_VST3="$ART/BouquetRealFX_artefacts/Release/VST3/Bouquet Real FX.vst3"

echo "== Architectures"
lipo -info "$INSTRUMENT_VST3/Contents/MacOS/Bouquet"

# Ad-hoc signatures: required for Apple Silicon to load the binaries at all.
echo "== Signing (ad hoc)"
for bundle in "$INSTRUMENT_AU" "$INSTRUMENT_VST3" "$APP" "$FX_AU" "$FX_VST3" \
              "$REAL_AU" "$REAL_VST3" "$REAL_APP" "$REAL_FX_AU" "$REAL_FX_VST3"; do
    codesign --force --deep --sign - "$bundle"
    codesign --verify --deep --strict "$bundle"
done

# Apple's Audio Unit validator (what Logic runs before loading a plugin).
echo "== auval"
mkdir -p "$HOME/Library/Audio/Plug-Ins/Components"
cp -R "$INSTRUMENT_AU" "$FX_AU" "$REAL_AU" "$REAL_FX_AU" "$HOME/Library/Audio/Plug-Ins/Components/"
killall -9 AudioComponentRegistrar 2>/dev/null || true
sleep 2
auval -v aumu Bqt1 Vst3
auval -v aumf Bqfx Vst3
auval -v aumu Bqr1 Vst3
auval -v aumf Bqrx Vst3
test -f "$REAL_VST3/Contents/Resources/Flowers/daisy.json"   # the renders made it into the bundle

echo "== Installer"
STAGE="$(mktemp -d)"
mkdir -p "$STAGE/vst3" "$STAGE/au" "$STAGE/app"
cp -R "$INSTRUMENT_VST3" "$FX_VST3" "$REAL_VST3" "$REAL_FX_VST3" "$STAGE/vst3/"
cp -R "$INSTRUMENT_AU" "$FX_AU" "$REAL_AU" "$REAL_FX_AU" "$STAGE/au/"
cp -R "$APP" "$REAL_APP" "$STAGE/app/"

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
    pkgbuild --root "$root" --component-plist "$plist" --identifier "com.vst3test.bouquet.$name" \
             --version "$VERSION" --install-location "$location" "$STAGE/$name.pkg"
}
build_component vst3 "$STAGE/vst3" "/Library/Audio/Plug-Ins/VST3"
build_component au   "$STAGE/au"   "/Library/Audio/Plug-Ins/Components"
build_component app  "$STAGE/app"  "/Applications"

cat > "$STAGE/welcome.txt" <<'TXT'
Bouquet: a granular instrument in bloom.

This installs:
  - Bouquet and Bouquet FX as Audio Units (Logic Pro, GarageBand, Ableton Live)
  - Bouquet and Bouquet FX as VST3 (Ableton Live and other hosts)
  - Bouquet Real and Bouquet Real FX: the same, with photoreal rendered flowers
  - the standalone Bouquet and Bouquet Real apps

After installing, rescan plug-ins in your DAW.
TXT

productbuild --synthesize --package "$STAGE/vst3.pkg" --package "$STAGE/au.pkg" --package "$STAGE/app.pkg" "$STAGE/distribution.xml"
# Add a title and welcome page to the generated distribution.
sed -i '' 's#<installer-gui-script minSpecVersion="[0-9]*">#&<title>Bouquet</title><welcome file="welcome.txt" mime-type="text/plain"/>#' "$STAGE/distribution.xml"
productbuild --distribution "$STAGE/distribution.xml" --package-path "$STAGE" --resources "$STAGE" "$OUT/Bouquet-macOS.pkg"

pkgutil --payload-files "$OUT/Bouquet-macOS.pkg" | grep -E "\.(vst3|component|app)$" || true
echo "== Done: $OUT/Bouquet-macOS.pkg"
