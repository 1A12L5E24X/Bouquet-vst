# vst3-test — Simple Synth

A polyphonic subtractive synthesizer, built with [JUCE](https://juce.com) 8 as a **VST3** plugin and a **Standalone** app.

## Features

- 8-voice polyphony, velocity sensitive, ±2 semitone pitch bend
- Oscillator: Sine, Saw, Square, Triangle (PolyBLEP anti-aliased)
- Amp ADSR envelope
- Resonant low-pass filter (state-variable TPT) with envelope modulation (±4 octaves)
- Smoothed output gain
- Editor with rotary knobs and an on-screen MIDI keyboard
- Host state save/restore

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

Outputs:

- VST3: `build/SimpleSynth_artefacts/Release/VST3/Simple Synth.vst3`
- Standalone: `build/SimpleSynth_artefacts/Release/Standalone/`

Copy the `.vst3` bundle to your plugin folder (`~/.vst3` on Linux,
`C:\Program Files\Common Files\VST3` on Windows, `~/Library/Audio/Plug-Ins/VST3` on macOS)
and rescan in your DAW. To have the build install it for you, change
`COPY_PLUGIN_AFTER_BUILD` to `TRUE` in `CMakeLists.txt`.

## Testing

```sh
ctest --test-dir build -C Release --output-on-failure
```

The test renders chords through every waveform and checks the output is audible,
finite, releases to silence, and that plugin state round-trips. The plugin also passes
[pluginval](https://github.com/Tracktion/pluginval) at strictness level 10.
