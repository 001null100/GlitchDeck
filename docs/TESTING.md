# GlitchDeck V1 alpha test pass

Put the native `GlitchDeck.clap` after an audio-producing instrument/effect chain in Bitwig, feed it something rhythmically obvious, and keep the transport running.

This candidate deliberately restores the effect engine to the first published alpha (`v1-alpha-6`, commit `8fbc09bafb0356fcee868074230cc07d54f97d22`). Judge Reverse/Pitch behavior against that version rather than the one-shot experiments from PR #4 builds 42-55.

## 1. Basic load / editor geometry

- Confirm `GlitchDeck.clap` scans and opens without warnings.
- Confirm the complete editor is visible, especially the right edge and Global Mix area.
- On a Windows display using non-100% scaling if available, resize the Bitwig plug-in window smaller and larger. The embedded editor should follow the host frame instead of remaining physically wider and clipped on the right.
- The branch uses null-clap's upstream `PhysicalPixelGuiSizing` path from build 43 rather than a GlitchDeck-local DPI workaround.
- Confirm dry audio passes unchanged when no trigger is active.

## 2. MIDI routing control test first

GlitchDeck defaults to CC20-27 on MIDI channel 16.

This build opts into null-clap's reusable hybrid audio/note-input host-routing profile. The framework CI independently verifies that a raw channel-16 CC (`0xBF`, CC20, value 127) passed in a CLAP process event list reaches `Plugin::onEvent()` unchanged.

Repeat the decisive Bitwig test first:

1. Put Bitwig's MIDI CC device immediately upstream of GlitchDeck.
2. Send CC20, channel 16, value 127 then 0.
3. Watch **MIDI IN**.

Expected if Bitwig now constructs the note/MIDI route for this hybrid CLAP descriptor:

- `MIDI IN · CC20 CH16 127`
- `MIDI IN · CC20 CH16 0`

Then test the physical controller and Learn.

Interpret failures precisely:

- **Bitwig-generated CC reaches MIDI IN:** framework/host routing is fixed; any remaining physical-controller failure is upstream of the device chain.
- **Bitwig-generated CC still says `NO HOST MIDI`:** do not debug GlitchDeck's CC parser. The host is still not putting the event in this CLAP instance's process event list, so the next fix belongs at the null-clap/host-routing boundary.
- **MIDI IN updates but Learn/trigger fails:** that is a GlitchDeck event interpretation bug.

## 3. Restored alpha-6 transport gestures

Test each temporal effect alone with the same source you used when the first alpha felt right.

- **Reverse:** should have the original looping captured-reverse character from alpha-6. It intentionally wraps the captured region rather than stopping at an endpoint.
- **Pitch Dive:** should use the original captured-loop playback-rate curve.
- **Pitch Rise:** should use the original captured-loop playback-rate curve, including its original modulo read-head behavior. There is no one-shot safety/capture redesign in this candidate.
- **Tape Stop:** remains the original standalone recent-history streaming/decelerating case.
- **Stutter / Microloop:** retain the original loop behavior.

The regression suite is now a fidelity suite. At a deterministic 1 kHz test rate it freezes the original Reverse loop sequence, Stutter sequence, Pitch Dive capture start, Pitch Rise fractional read-head sequence, and Reverse-over-Stutter recapture behavior.

## 4. Combinations

The original alpha grammar is restored here too. Reverse, Pitch Dive, and Pitch Rise are themselves loop-defining capture gestures, so triggering one over an existing transport gesture can recapture the shared region. Do not expect the later PR #4 modifier-only semantics.

Try the combinations you actually care about musically and compare them against your memory of alpha-6. If one combination is worse than alpha-6 while the standalone effects match, report that combination specifically and we can refine it without rewriting the whole transport model again.

## 5. Latch and quantization

- Latch: a short press toggles the slot on; the next press toggles it off.
- Quantization: test Free, 1/16, 1/8, 1/4, and 1 Bar while Bitwig is playing.
- Release remains immediate.

## 6. UI / keyboard / automation

- Pads should visibly light while active.
- Clicking and holding a pad should behave like holding MIDI.
- Right-clicking a pad should select it without triggering audio.
- Number keys 1-8 should trigger slots only while the plugin UI owns keyboard focus.
- Host automation of Trigger 1-8 should create the same performance edges as UI/MIDI triggering.

## 7. What to report

The most useful pass for this candidate is compact:

- scaling: fixed / broken
- Reverse vs alpha-6: same / different, and how
- Pitch Rise vs alpha-6: same / different, and how
- any other effect that no longer matches alpha-6
- exact result of **Bitwig MIDI CC device -> MIDI IN**
- if MIDI arrives, whether Learn and CC20-27 triggering work

For DSP differences, describe the audible fault rather than only saying "broken": restart/wrap character, wrong pitch direction/curve, silence, frozen audio, discontinuity/click, wrong capture length, or wrong combination behavior.
