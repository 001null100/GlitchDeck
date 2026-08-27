# GlitchDeck V1 architecture

## Product rule

GlitchDeck is deterministic and performance-first. A trigger press must produce the configured gesture every time. There is no probability layer in V1.

## Signal model

1. Input is continuously written into an 8-second stereo circular history buffer.
2. Trigger slots change how recent history is read and transformed.
3. Compatible temporal gestures share one transport/read head so combinations have a defined musical meaning.
4. Destruction effects are applied after temporal playback.
5. Per-trigger attack/release envelopes and the global mix return to clean audio without hard discontinuities.

## Trigger slots

Eight slots default to MIDI notes 36-43, displayed as C1-G1 using Bitwig-style octave naming. Every slot exposes:

- momentary trigger parameter
- effect type
- hold/latch behavior
- MIDI note
- onset quantization
- stereo behavior
- intensity
- length
- attack
- release
- shape

The initial effect set is Stutter, Microloop, Reverse, Tape Stop, Pitch Dive, Pitch Rise, Bitcrush, and Dropout.

## Temporal grammar

Stutter and Microloop capture and define a loop region. Reverse changes the direction of the same shared playback head. Pitch Dive and Pitch Rise continuously alter its playback rate. Tape Stop applies a deceleration curve to the same transport. When Tape Stop is used alone it follows a streaming read head instead of forcing an audible loop.

This is intentionally different from serially instantiating independent delay-style effects. A held Stutter plus Pitch Dive should sound like a single repeated fragment progressively descending, not like a pitch effect receiving an unrelated stutter processor.

## Timing

MIDI note offsets inside each audio block are preserved. Quantized onset scheduling uses host BPM and PPQ position and supports Free, 1/32, 1/16, 1/8, 1/4, and 1 Bar. Release remains immediate in V1 so a performer never feels trapped by a quantized release.

## Real-time constraints

The audio path performs no file IO and takes no mutexes. History storage is allocated in prepareToPlay. Trigger scheduling uses a fixed-size pending-event array. DSP state is mutated only by the audio thread; the editor receives a small atomic active-state mirror for drawing.

## Planned next layers

After the core performance feel is validated:

- Smart Capture timing
- End-of-loop release
- Scramble / slice engine
- spectral freeze
- synced loop-length modes
- deeper stereo modes
- MIDI learn UX
- trigger modifiers such as Double and Brake
- optional high-quality time stretch
