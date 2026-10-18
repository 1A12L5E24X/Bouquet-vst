# vst3-test

Plugins built with [JUCE](https://juce.com) 8, sharing one CMake project. **macOS is the main platform.**

| Plugin | What it is | Formats |
| --- | --- | --- |
| **Bouquet** | A granular instrument: synthesised live instruments on the left, a cloud of granular "spells" on the right, and a garden that grows a bouquet for every chord | AU + VST3 instrument, standalone app |
| **Bouquet FX** | The same engine as an audio effect that granulates whatever track it sits on | AU (music effect) + VST3 effect |
| **Simple Synth** | A small polyphonic subtractive synth | AU, VST3, standalone |

## Install on a Mac

1. Open the repository's **Actions** tab on GitHub, open the newest green **Build** run, and download
   **Bouquet-macOS-installer** from its Artifacts.
2. Unzip it and open **Bouquet-macOS.pkg**. The installer isn't signed by a registered Apple developer,
   so the first time **right-click → Open** (or allow it in System Settings → Privacy & Security).
3. It installs Bouquet and Bouquet FX as Audio Units (`/Library/Audio/Plug-Ins/Components`) and VST3
   (`/Library/Audio/Plug-Ins/VST3`), plus the standalone app.
4. Rescan plug-ins in your DAW:
   - **Logic Pro / GarageBand**: they appear under Audio Units → VST3Test.
   - **Ableton Live**: Settings → Plug-Ins → turn on "Use Audio Units" (and/or the VST3 system folder),
     then Rescan. Put **Bouquet** on a MIDI track; put **Bouquet FX** on an audio track to granulate it.

The binaries are universal (Apple Silicon and Intel) and need macOS 11 or later. Every macOS CI run
passes Apple's `auval` validator for both Audio Units.

## Bouquet

![Bouquet](docs/bouquet.jpg)

*a granular instrument in bloom.*

### The garden

Every chord you play grows its own **bouquet** along the bottom of the window, rising out of the keys
you played. It's unique to that chord: the notes, chord shape, instrument and how hard you play decide
which flowers grow (daisies, tulips, poppies, cosmos, bellflowers, wild roses), how many, their
colours (bright warm colours for major chords, lilac and blue for minor, mint for suspended, plum for
diminished) and how they're tied. Each chord tone becomes one flower, and higher notes grow taller.
The bouquet blooms while you hold the chord and sways with the sound. When you let go it slowly rots:
it browns, bows over, drops its petals and fades. A loud granular tail keeps it alive a little
longer. On live input, a swell after silence plants a bouquet too.

### Sound Lab: choose a voice

Nine instruments, all synthesised from scratch (no sample libraries), plus live input:

| Voice | How it's made |
| --- | --- |
| live input | the audio on the track (Bouquet FX, or hosts that route audio into instruments) |
| piano | 16 inharmonic string partials with detuned unison pairs and a felt hammer |
| rhodes | FM tine with a fading bell bark |
| marimba | tuned wooden-bar modes (1 : 3.9 : 9.2) with a resonator tube |
| glass | struck-crystal modes, each slowly beating against a twin |
| harp | Karplus–Strong plucked string |
| violin, cello | band-limited bowed string through a resonant wooden body, delayed vibrato, bow noise |
| flute | breath noise tuned to the note, chiff on the attack |
| choir | three detuned voices through sung-vowel formants (tone morphs oo → oh → ah → ee) |

**One-finger chords**, in the spirit of chord instruments like the Orchid: pick a chord
shape (maj, min, sus2, sus4, maj7, min7, 7, add9, maj9, min9, 6/9, dim, fifths), a
voicing (close / open / wide) and a strum, and every key plays the whole chord.
Plus tone, attack, release, octave and level. Sustain pedal and pitch bend are supported.

### Magic Lab: cast a spell

A granular engine with a 16-second memory. Every grain can be transposed to a chord
tone, reversed, scattered in time and space, fed back into the memory and drowned in a
modulated 8-line reverb. Spells are complete settings: click one and every control
glides there over a second and a half, so you hear (and see) the sound transform.

40 spells in five families:

- **harmony**: chord smear, minor sigh, fifths aurora, lydian glow, octave ghosts, pentatonic rain, nine moons, suspended
- **time**: frozen breath, reverse bloom, tape memory, time smear, stutter dust, echo garden, slow motion, infinite sustain
- **texture**: glass rain, pollen, murmuration, velvet, sand, broken music box, whisper, cluster
- **space**: shimmer halo, cathedral swarm, underwater choir, moon tide, deep field, sub bloom, cloud chamber, northern hall
- **motion**: drift, orbit, pulse, tremolo dream, wind chimes, falling, rising, chaos garden

Then play with it:

- **The aura pad**: drag left for fine dust and right for long smears; drag down for close and up for vast
  (more feedback, space, detune and width). Every grain appears as a mote of light: height
  is pitch, distance from the left is how far back in memory it came from; gold motes are
  chord tones and rose ones are reversed. Double-click to recentre.
- **mix, space, life**: life sets how much slow random modulation keeps the sound moving.
- **freeze**: holds the memory still, so the cloud keeps singing what it last heard.
- **harmony**: the interval set grains are transposed into.
- **mutate**: nudges the current setting somewhere nearby.
- **Inner workings**: all 14 engine controls (density, size, reach, spray, pitch, chord
  amount, detune, reverse, chaos, edge, width, echo, colour, decay).

Spells are also exposed to the host as programs, and every control is automatable.

### Design

A bright day: serif type (Newsreader) and small tracked sans labels (Inter) in warm charcoal on
sunlit paper. Warm light (butter, peach, rose) drifts behind the Sound Lab and sky light (blue,
lilac, mint) behind the Magic Lab; both brighten with the sound. Frosted glass panels, arcs of
colour for knobs, a small sun beside the name that pulses with the output, and hand-drawn flowers.
The window can be resized (with the aspect ratio locked) and renders at full Retina sharpness.

Both fonts are under the SIL Open Font License; see `Bouquet/assets/fonts/`.

## Building

Requirements: CMake ≥ 3.22, a C++17 compiler, and Git (JUCE is fetched automatically).

On Linux, install the JUCE dependencies first:

```sh
sudo apt install libasound2-dev libx11-dev libxrandr-dev libxinerama-dev \
                 libxcursor-dev libxext-dev libfreetype-dev libfontconfig1-dev
```

Then:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j
```

On a Mac, build universal and make the installer yourself (needs Xcode command-line tools):

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release "-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64"
cmake --build build --config Release -j
scripts/package-macos.sh build      # signs ad hoc, runs auval, writes build/Bouquet-macOS.pkg
```

Outputs (under `build/Bouquet/`):

- `Bouquet_artefacts/Release/AU/Bouquet.component`, `.../VST3/Bouquet.vst3`, `.../Standalone/Bouquet.app`
- `BouquetFX_artefacts/Release/AU/Bouquet FX.component`, `.../VST3/Bouquet FX.vst3`

On Windows, copy the `.vst3` bundles to `C:\Program Files\Common Files\VST3`; on Linux, to `~/.vst3`.

## Testing

```sh
ctest --test-dir build -C Release --output-on-failure
```

The Bouquet test plays every instrument dry, runs every spell, granulates and freezes a
live input, checks the note events the garden grows from (and that every chord gets its own
bouquet), round-trips state, and checks it renders well faster than real time. The test
binary can also render demos and screenshots:

```sh
BouquetTest --demo out.wav <spell> <instrument>   # e.g. 24 6 = shimmer halo on violin
BouquetTest --snapshot out.png [inst|fx] <spell> <instrument> [rot] [zoom]
```

All plugins pass [pluginval](https://github.com/Tracktion/pluginval) at strictness level 10.
