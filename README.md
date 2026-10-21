# Bouquet

A granular instrument for macOS (AU + VST3, universal, macOS 11+; VST3 on Windows and Linux too).
Recordings of real instruments on the left, played as if in a forest clearing; a granular Magic Lab
on the right; and a garden that grows a bouquet for every chord you play, then lets it rot;
and a preset garden where you water your sounds into richer ones.

![Bouquet](docs/bouquet.jpg)

| Plugin | |
| --- | --- |
| **Bouquet** | instrument (+ standalone app) |
| **Bouquet FX** | the same engine as an effect, granulating the track it's on |

## Install (Mac)

1. Download **Bouquet-macOS.pkg** from the [latest release](https://github.com/1A12L5E24X/Bouquet-vst/releases/latest)
   (or, for the newest build, from the Artifacts of the latest green run in the
   [Actions tab](https://github.com/1A12L5E24X/Bouquet-vst/actions)).
2. Open it. The installer isn't notarized yet, so macOS may say it can't check it: open
   **System Settings → Privacy & Security**, scroll down and click **Open Anyway**.
3. Rescan plug-ins in your DAW. In Ableton: Settings → Plug-Ins → "Use Audio Units" → Rescan.
   Bouquet goes on a MIDI track, Bouquet FX on an audio track. In Logic they're under Audio Units → Bouquet.

The installer also removes Murmur, Simple Synth and Bouquet Real from older test builds.

## Using it

- **Sound Lab**: piano, rhodes, marimba, harp, violin, cello and flute are recordings of real
  instruments; glass and choir are synthesised; or use live input. One finger plays a whole chord
  (shape, voicing, strum). **Distance** sets how deep in the trees the instrument is, **Ambience** how
  much of the forest you hear.
- **Magic Lab**: 40 granular spells (chord smear, frozen breath, shimmer halo…). Click one and everything
  glides there. Drag the pad (left–right: grain length; down–up: space), hit **Freeze**, **Mutate**. The **Magic Lab** switch bypasses it and folds it away.
  **Tempo sync** locks the grains to your DAW's beat grid (the grain rate snaps to the nearest note
  length, straight or triplet) and makes echoes land on whole note lengths.
- **Expression**: press into held keys (aftertouch, channel or polyphonic) to swell and brighten a note
  and lift its flowers; the mod wheel blows wind through the grains and the garden. The recordings
  cross-fade smoothly between soft and loud layers, and no two notes are played quite the same.
- **The garden**: grass springs up under the keys you play and every chord grows a bouquet that rots when
  you let go. The **Photoreal** switch chooses between 3D-rendered and illustrated flowers; photoreal
  also puts a real daylight sky behind the glass, its clouds drifting slowly across, and lays the
  keyboard in a meadow, the bouquets growing up from behind the keys.
- **Presets**: click **Garden** to look down on a lawn where every preset is a flower. Hover to see a name,
  click to load, **plant this sound** to save yours as a new flower. Right-click to uproot.

  ![The preset garden](docs/meadow.jpg)
- **Watering**: in the preset garden, water a flower to grow a richer version of its sound beside it.
  The lab bench along the bottom holds a beaker of water and four flasks of tonic:

  | Tonic | What it does to a sound |
  | --- | --- |
  | **rain water** | more air, room and forest |
  | **honey** | thicker and warmer, more voices |
  | **moonwater** | shimmering echoes and harmony |
  | **storm water** | wild, scattered, unpredictable |

  Click a flask to stir a dose into the beaker (up to three; each pour measures a little differently),
  then drag the beaker onto a flower. It tips and pours, a new flower grows, and the sound glides to it.
  Plain water alone makes everything a little fuller; each tonic pushes the sound further its own way.
  About one pour in five fizzes into something unexpected (a new harmony, a jump in register, a wild
  turn), and storm water makes that likelier. The new preset is saved and named after the flower it came
  from and what it was given, like *nocturne, honeyed & moonlit*. Water it again to keep experimenting,
  or click the beaker to tip the mix out.

  ![Watering a flower](docs/watering.jpg)

## Building

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release "-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64"
cmake --build build --config Release -j
ctest --test-dir build -C Release        # render tests
scripts/package-macos.sh build           # Mac: sign, run auval, make the .pkg
```

Needs CMake 3.22+ and a C++17 compiler; JUCE 8 is fetched automatically. Every green build of `main`
is published as a release, tagged from `VERSION`.

To sign and notarize the Mac installer, add these repository secrets (an Apple Developer account is
needed): `MACOS_CERTIFICATES_P12` (base64 of a .p12 with both Developer ID certificates),
`MACOS_CERTIFICATES_PASSWORD`, `DEVELOPER_ID_APP`, `DEVELOPER_ID_INSTALLER`, `NOTARY_APPLE_ID`,
`NOTARY_TEAM_ID`, `NOTARY_PASSWORD`. Without them the installer is signed ad hoc.

## Assets

- `Bouquet/assets/samples`: instrument recordings from [VSCO-2 CE](https://github.com/sgossner/VSCO-2-CE)
  (CC0) and the FluidR3 electric piano (MIT), prepared by `tools/samples/build_samples.py`.
- `Bouquet/assets/flowers`: flowers, grass and lawn rendered in Blender by `tools/flowers/render_flowers.py`,
  lit by a CC0 sky from Poly Haven; the sky by `tools/flowers/render_sky.py`; the meadow around the keys
  by `tools/flowers/render_meadow.py`.
- Fonts: Newsreader and Inter, SIL Open Font License.

## Licence

[AGPL-3.0](LICENSE), as required by the free licence of the JUCE framework it is built on.
