# GlitchDeck V1 architecture

## Product rule

GlitchDeck is deterministic and performance-first. A trigger press must produce the configured gesture every time. There is no probability layer in V1.

## Host architecture

GlitchDeck is a native CLAP plugin built on `null-clap`.

- null-clap owns CLAP lifecycle, audio/note ports, parameter/event routing, state, remote controls, and GUI extension plumbing.
- GlitchDeck owns DSP, raw MIDI interpretation, MIDI Learn, trigger scheduling, and product behavior.
- JUCE is used only for editor components and native window embedding. There is no JUCE AudioProcessor/APVTS/MIDI/playhead/plugin wrapper layer.

The plugin advertises a raw MIDI note port using the CLAP MIDI dialect and interprets `CLAP_EVENT_MIDI` directly. Quantized scheduling reads the current native `clap_process_t` transport context.

## Signal model

1. Input is continuously written into an 8-second stereo circular history buffer.
2. Trigger slots change how recent history is read and transformed.
3. Compatible temporal gestures share one transport/read head so combinations have a defined musical meaning.
4. Destruction effects are applied after temporal playback.
5. Per-trigger attack/release envelopes and the global mix return to clean audio without hard discontinuities.

## Trigger slots

Eight slots default to CC20-27 on MIDI channel 16. Every slot exposes:

- momentary writable trigger parameter
- effect type
- hold/latch behavior
- MIDI binding type, number, and channel
- onset quantization
- stereo behavior
- intensity
- length
- attack
- release
- shape

The initial effect set is Stutter, Microloop, Reverse, Tape Stop, Pitch Dive, Pitch Rise, Bitcrush, and Dropout.

Writable CLAP parameters participate in project state so hosts can reproduce their values exactly. Trigger parameter values therefore serialize like other writable parameters. The actual performance gesture state is separate and non-persistent: state load resets pending edges, envelopes, active slots, and the glitch engine before updating the restored configuration. A saved trigger value of `1` remains host-visible after load but does not itself re-engage the DSP gesture.

## Temporal grammar

Stutter and Microloop capture and define a loop region. Reverse changes the direction of the same shared playback head. Pitch Dive and Pitch Rise continuously alter its playback rate. Tape Stop applies a deceleration curve to the same transport. When Tape Stop is used alone it follows a streaming read head instead of forcing an audible loop.

This is intentionally different from serially instantiating independent delay-style effects. A held Stutter plus Pitch Dive should sound like a single repeated fragment progressively descending, not like a pitch effect receiving an unrelated stutter processor.

## Timing

Raw MIDI and host parameter events retain their CLAP sample offsets. Quantized onset scheduling uses CLAP transport tempo/beat information and supports Free, 1/32, 1/16, 1/8, 1/4, and 1 Bar. Release remains immediate in V1 so a performer never feels trapped by a quantized release.

## Real-time constraints

The audio path performs no file IO and takes no mutexes. History storage is allocated at activation. Trigger scheduling uses fixed-size real-time-safe storage. DSP state is mutated on the audio thread; the editor reads atomic mirrors and sends host-visible parameter gestures through null-clap.

## Planned next layers

After the core performance feel is validated:

- Smart Capture timing
- End-of-loop release
- Scramble / slice engine
- spectral freeze
- synced loop-length modes
- deeper stereo modes
- trigger modifiers such as Double and Brake
- optional high-quality time stretch
