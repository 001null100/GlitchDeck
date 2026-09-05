# GlitchDeck V1 architecture

## Product rule

GlitchDeck is deterministic and performance-first. A trigger press must produce the configured gesture every time. There is no probability layer in V1.

## Host architecture

GlitchDeck is a native CLAP plugin built on `null-clap`.

- null-clap owns CLAP lifecycle, audio/note ports, parameter/event routing, state, remote controls, host-facing descriptor routing metadata, and GUI extension plumbing.
- GlitchDeck owns DSP, MIDI interpretation, MIDI Learn, trigger scheduling, and product behavior.
- JUCE is used only for editor components and native window embedding. There is no JUCE AudioProcessor/APVTS/MIDI/playhead/plugin wrapper layer.

GlitchDeck explicitly enables null-clap's hybrid note-input routing profile because it is an audio processor whose performance layer consumes the host note/MIDI stream. The framework keeps the plug-in's audio-effect identity and adds host-facing note-effect metadata consistently at both factory discovery and instance descriptor boundaries.

The performance note port accepts raw MIDI and native CLAP note events, preferring raw MIDI so Control Change messages can reach the eight performance bindings. The editor keeps a lock-free atomic mirror of the last host-delivered MIDI/note event. This diagnostic distinguishes application-side mapping problems from host routing problems without logging or allocating on the audio thread.

## Signal model

1. Input is continuously written into an 8-second stereo circular history buffer.
2. Trigger slots change how recent history is read and transformed.
3. Compatible temporal gestures use the original alpha shared transport/read-head model.
4. Destruction effects are applied after temporal playback.
5. Per-trigger attack/release envelopes and the global mix return to clean audio.

## Trigger slots

Eight slots default to CC20-27 on MIDI channel 16. Every slot exposes effect type, latch, MIDI binding, quantization, stereo mode, intensity, length, attack, release, shape, and a writable trigger parameter.

The initial effect set is Stutter, Microloop, Reverse, Tape Stop, Pitch Dive, Pitch Rise, Bitcrush, and Dropout.

Writable CLAP parameters participate in project state. Performance gesture state itself is non-persistent: state load resets pending edges, envelopes, active slots, and the glitch engine before applying restored configuration.

## Temporal grammar: alpha-6 reference behavior

The DSP behavior is intentionally restored to the first published alpha (`v1-alpha-6`, commit `8fbc09bafb0356fcee868074230cc07d54f97d22`). The native engine source is the direct stdlib/process-sample translation of that implementation rather than a redesigned transport model.

- **Stutter** captures a recent region and loops it.
- **Microloop** does the same with its capture clamped to the original 2-50 ms range.
- **Reverse** is itself a loop-defining capture. It starts from the newest end and wraps backward through that captured region.
- **Pitch Dive** is also a loop-defining capture and changes the shared playback rate downward over the gesture.
- **Pitch Rise** is also a loop-defining capture and changes the shared playback rate upward over the gesture.
- **Tape Stop** is the special standalone streaming transport case. If no other transport effect is active, it begins from recent live history and decelerates that head.
- Triggering another loop-defining transport gesture can recapture the shared region. This is original alpha behavior and is deliberately preserved rather than normalized into a newer combination grammar.
- Captured playback uses the original global-history linear interpolation and modulo loop-head behavior, including its characteristic boundary behavior.

This is now protected by alpha-fidelity tests for Reverse, Stutter, Pitch Dive, Pitch Rise, and Reverse-over-Stutter recapture behavior. Future DSP changes should be made one effect at a time against this known-good musical baseline.

## Timing

Raw MIDI, native CLAP notes, and host parameter events retain their CLAP sample offsets. Quantized onset scheduling uses CLAP transport tempo/beat information and supports Free, 1/32, 1/16, 1/8, 1/4, and 1 Bar. `1 Bar` follows the host's reported time signature and current bar start, with a 4/4 fallback if meter information is absent. Release remains immediate in V1.

## GUI scaling

CLAP's Win32 GUI contract expresses child-window dimensions in physical pixels, while JUCE component bounds are logical/DPI-scaled coordinates. The conversion is centralized upstream in null-clap's `PhysicalPixelGuiSizing` helper. This is the same upstream path used by validated build 43 and avoids carrying a second GlitchDeck-specific DPI implementation.

## Real-time constraints

The audio path performs no file IO and takes no mutexes. History storage is allocated at activation. Trigger scheduling uses fixed-size real-time-safe storage. DSP state is mutated on the audio thread; the editor reads atomic mirrors and sends host-visible parameter gestures through null-clap.

## Planned next layers

After the restored alpha behavior and host MIDI path are validated:

- effect-by-effect DSP refinement without changing unrelated gestures
- Smart Capture timing
- End-of-loop release
- Scramble / slice engine
- spectral freeze
- synced loop-length modes
- deeper stereo modes
- trigger modifiers such as Double and Brake
- optional high-quality time stretch
