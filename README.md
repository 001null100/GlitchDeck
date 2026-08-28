# GlitchDeck

GlitchDeck is a playable, performance-first Windows glitch processor for Bitwig Studio. It ships as **CLAP (preferred for Bitwig)** and VST3.

The plugin continuously records recent input audio into a circular history buffer, then lets eight trigger slots temporarily reinterpret that history as repeat, reverse, pitch/rate, destruction, and dropout gestures. Multiple compatible triggers can be held together so glitches behave more like an instrument than a random effect generator.

## V1 goals

- Eight hold/latch trigger slots
- Dedicated controller-pad scheme: CC20-27 on MIDI channel 16 by default
- Per-slot MIDI Learn for Note or CC bindings
- Sample-offset-aware MIDI triggering
- Continuous stereo history buffer
- Stutter / microloop
- Reverse modifier
- Pitch dive / rise
- Tape stop
- Bitcrush burst
- Dropout
- Quantized trigger starts
- Smooth wet/dry attack and release
- Bitwig-friendly parameters and automation
- Windows GitHub Actions builds for CLAP + VST3
- CLAP validation with clap-validator and VST3 validation with pluginval

Probability is deliberately not part of the design. A performed trigger should reliably do what the performer asked.

## Why CLAP is preferred in Bitwig

GlitchDeck's performance pads are intended to send ordinary MIDI CC messages: 127 on press and 0 on release. CLAP exposes a real MIDI event input to the plugin, so Bitwig can send those CC packets directly into GlitchDeck and its internal MIDI Learn system can see them.

VST3 remains available as a compatibility build, but VST3 represents MIDI CC through host parameter mapping rather than ordinary plugin MIDI events. Depending on the host, that can prevent plugin-side CC Learn from seeing the controller at all.

## Build

```powershell
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Release --parallel
```

CMake fetches JUCE 9.0.0 and the pinned `clap-juce-extensions` revision automatically unless `GLITCHDECK_JUCE_PATH` and/or `GLITCHDECK_CLAP_JUCE_PATH` point to existing checkouts.

## Install on Windows

- CLAP: copy `GlitchDeck.clap` to `C:\Program Files\Common Files\CLAP\` (or a user CLAP path).
- VST3: copy `GlitchDeck.vst3` to `C:\Program Files\Common Files\VST3\`.

For Bitwig controller testing, use the CLAP build first.

## Status

Early V1 alpha. The architecture and DSP are being built around deterministic, combinable performance gestures first; polish and additional effects follow once the core feels good under the fingers.
