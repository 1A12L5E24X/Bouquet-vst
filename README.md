# Bouquet

A granular instrument for macOS (AU + VST3, universal, macOS 11+). Synthesised instruments recorded
"live in a forest" on the left, a granular Magic Lab on the right, and a garden that grows a bouquet
for every chord you play, then lets it rot.

![Bouquet](docs/bouquet.jpg)

| Plugin | |
| --- | --- |
| **Bouquet** | instrument (+ standalone app) |
| **Bouquet FX** | the same engine as an effect, granulating the track it's on |
| **Bouquet Real** / **Real FX** | the same, with photoreal 3D-rendered flowers |

## Install (Mac)

1. Open the newest green **Build** run in the [Actions tab](https://github.com/1A12L5E24X/vst3-test/actions)
   and download **Bouquet-macOS-installer** from Artifacts (or use the
   [latest release](https://github.com/1A12L5E24X/vst3-test/releases/latest) if there is one).
2. Unzip and open **Bouquet-macOS.pkg**. It isn't notarised, so the first time **right-click → Open**.
3. Rescan plug-ins in your DAW. In Ableton: Settings → Plug-Ins → "Use Audio Units" → Rescan.
   Bouquet goes on a MIDI track, Bouquet FX on an audio track.

The installer also removes Murmur and Simple Synth from older builds.

## Using it

- **Sound Lab**: piano, rhodes, marimba, glass, harp, violin, cello, flute, choir or live input,
  all synthesised. One finger plays a whole chord (shape, voicing, strum). **forest** and **wildlife**
  set how deep in the trees you are and how much of the forest you hear.
- **Magic Lab**: 40 granular spells (chord smear, frozen breath, shimmer halo…). Click one and everything
  glides there. Drag the aura pad, hit **freeze**, **mutate**. The on/off switch bypasses it and folds it away.
- **Garden**: click **garden** to browse presets as flowers in a meadow. Hover to see a name, click to load,
  **plant this sound** to save yours as a new flower. Right-click to uproot.

## Building

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release "-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64"
cmake --build build --config Release -j
ctest --test-dir build -C Release        # render tests
scripts/package-macos.sh build           # Mac: sign ad hoc, run auval, make the .pkg
```

Needs CMake 3.22+ and a C++17 compiler; JUCE 8 is fetched automatically. The flower renders are made with
`tools/flowers/render_flowers.py` (Blender as a Python module). Fonts are SIL OFL, skies CC0.
