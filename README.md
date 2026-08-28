# GlitchDeck

GlitchDeck is a playable, performance-first Windows glitch processor for Bitwig Studio. It is a **native CLAP plugin** built on [null-clap](https://github.com/001null100/null-clap), with JUCE used only for the graphical editor.

The plugin continuously records recent input audio into a circular history buffer, then lets eight trigger slots temporarily reinterpret that history as repeat, reverse, pitch/rate, destruction, and dropout gestures. Multiple compatible triggers can be held together so glitches behave more like an instrument than a random effect generator.

## V1 goals

- Eight hold/latch trigger slots
- Dedicated controller-pad scheme: CC20-27 on MIDI channel 16 by default
- Per-slot MIDI Learn for Note or CC bindings
- Direct raw `CLAP_EVENT_MIDI` handling with sample offsets preserved
- Continuous stereo history buffer
- Stutter / microloop
- Reverse modifier
- Pitch dive / rise
- Tape stop
- Bitcrush burst
- Dropout
- Quantized trigger starts from CLAP transport
- Smooth wet/dry attack and release
- Bitwig-friendly native CLAP parameters, state, and remote-control pages
- Windows GitHub Actions build validated with clap-validator

Probability is deliberately not part of the design. A performed trigger should reliably do what the performer asked.

## MIDI input

GlitchDeck's performance pads are intended to send ordinary MIDI CC messages: 127 on press and 0 on release. The plugin advertises a native CLAP MIDI input port, so Bitwig can send those packets directly to GlitchDeck and its internal MIDI Learn system can see them.

The recommended Nektar Impact LX25 MK3 layout is CC20-27 on channel 16, saved to a dedicated pad map. See [`docs/MIDI_SETUP.md`](docs/MIDI_SETUP.md).

## Build

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel --target GlitchDeck_CLAP
```

CMake fetches JUCE 9.0.0 and the exact pinned null-clap revision automatically. JUCE provides GUI/windowing only; the plugin lifecycle, audio/event routing, parameters, state, note ports, and host integration are native CLAP through null-clap.

## Install on Windows

Copy `GlitchDeck.clap` to:

`C:\Program Files\Common Files\CLAP\`

Then rescan plugins in Bitwig if necessary.

## Status

Early V1 alpha. The current build is focused on validating the native CLAP trigger path and the shared glitch-transport DSP under real performance input before expanding the effect set.
