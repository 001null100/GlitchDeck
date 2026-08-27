# GlitchDeck V1 alpha test pass

The first useful test is about feel, not presets. Put GlitchDeck on an audio-producing track in Bitwig, feed it something rhythmically obvious, and keep the transport running.

## 1. Basic load

- Confirm the VST3 scans and opens without warnings.
- Confirm dry audio passes unchanged when no trigger is active.
- Confirm Global Mix at 0% is clean and 100% is fully available to the gestures.

## 2. Default pads

The eight slots default to MIDI notes 36-43, shown as C1-G1 in GlitchDeck:

1. C1: Stutter
2. C#1: Microloop
3. D1: Reverse
4. D#1: Tape Stop
5. E1: Pitch Dive
6. F1: Pitch Rise
7. F#1: Bitcrush
8. G1: Dropout

Hold each pad individually, then release it. The important checks are immediate response, no stuck states, and a clean return to dry audio.

## 3. Combinations

Try these deliberately:

- Hold Stutter, then add Pitch Dive.
- Hold Stutter, tap/hold Reverse, then release Reverse while Stutter continues.
- Hold Stutter and add Tape Stop.
- Hold Microloop and add Bitcrush.
- Hold any temporal gesture and add Dropout.

The combinations should sound like modifications of one gesture, especially Stutter + Reverse/Pitch/Tape, rather than unrelated effects being restarted.

## 4. Latch

Enable Latch on a slot. A short press should toggle the effect on; the next press should toggle it off. Note-off should not cancel a latched gesture.

## 5. Quantization

With Bitwig playing, test Free, 1/16, 1/8, and 1/4. Onsets should wait for the selected host grid boundary. Release is intentionally immediate in this alpha.

## 6. MIDI remap

Change one slot's MIDI note and verify the old note stops addressing it and the new note starts addressing it. This is the fallback if the controller is shifted to a different octave.

## 7. UI / keyboard

- Pads should visibly light while active.
- Clicking and holding a pad should behave like holding MIDI.
- Number keys 1-8 should trigger slots only while the plugin UI owns keyboard focus.
- Changing the selected slot should retarget the lower editor without changing audio state.

## 8. Things worth reporting precisely

For timing or audio bugs, note:

- Bitwig buffer size / sample rate
- effect(s) active
- hold vs latch
- quantization mode
- whether the trigger came from MIDI, mouse, automation, or keyboard
- whether the issue occurs only when combining effects

Clicks at intentional hard glitch boundaries can be stylistic; clicks specifically on activation/release or unrelated to the configured gesture are bugs.
