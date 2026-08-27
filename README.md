# GlitchDeck

GlitchDeck is a playable, performance-first Windows VST3 glitch processor for Bitwig Studio.

The plugin continuously records recent input audio into a circular history buffer, then lets eight trigger slots temporarily reinterpret that history as repeat, reverse, pitch/rate, destruction, and dropout gestures. Multiple compatible triggers can be held together so glitches behave more like an instrument than a random effect generator.

## V1 goals

- Eight hold/latch trigger slots
- Default MIDI mapping: C1 through G1 (MIDI notes 36-43), configurable per slot
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
- Bitwig-friendly VST3 parameters and automation
- Windows GitHub Actions build, pluginval validation, and prerelease artifacts

Probability is deliberately not part of the design. A performed trigger should reliably do what the performer asked.

## Build

```powershell
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Release --parallel
```

CMake fetches JUCE 9.0.0 automatically unless `GLITCHDECK_JUCE_PATH` points to an existing JUCE checkout.

## Status

Early V1 alpha. The architecture and DSP are being built around deterministic, combinable performance gestures first; polish and additional effects follow once the core feels good under the fingers.
