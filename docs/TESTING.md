# GlitchDeck V1 alpha test pass

Put the native `GlitchDeck.clap` after an audio-producing instrument/effect chain in Bitwig, feed it something rhythmically obvious, and keep the transport running.

## 1. Basic load

- Confirm `GlitchDeck.clap` scans and opens without warnings.
- Confirm dry audio passes unchanged when no trigger is active.
- Confirm Global Mix at 0% is clean and 100% is fully available to the gestures.

## 2. Direct controller input / MIDI Learn

GlitchDeck defaults to:

1. CC20, channel 16: Stutter
2. CC21, channel 16: Microloop
3. CC22, channel 16: Reverse
4. CC23, channel 16: Tape Stop
5. CC24, channel 16: Pitch Dive
6. CC25, channel 16: Pitch Rise
7. CC26, channel 16: Bitcrush
8. CC27, channel 16: Dropout

Press and release each configured pad. A Nektar `Trg` pad should send 127 on press and 0 on release, so the effect should follow the physical hold exactly.

Then test Learn explicitly:

1. Select a slot.
2. Click **LEARN**.
3. Hit any configured pad.
4. Confirm the binding updates immediately, for example `CC 20 · CH 16`.

If Learn stays waiting, report that specifically. It means the raw CLAP MIDI event path is not reaching GlitchDeck and is the highest-priority bug.

## 3. Standalone transport gestures

Test Reverse, Tape Stop, Pitch Dive, and Pitch Rise individually before combining them.

- Reverse alone should move backward through recent audio instead of falling into a short repeating loop.
- Pitch Dive/Rise alone should behave as streaming rate gestures instead of implicitly becoming loop effects.
- Tape Stop alone should decelerate the streaming history head.
- Releasing a Stutter/Microloop while Reverse, Pitch, or Tape Stop remains held should leave the remaining gesture running without getting trapped in the old loop region.

## 4. Combinations

Try these deliberately:

- Hold Stutter, then add Pitch Dive.
- Hold Stutter, tap/hold Reverse, then release Reverse while Stutter continues.
- Hold Stutter and add Tape Stop.
- Hold Microloop and add Bitcrush.
- Hold any temporal gesture and add Dropout.

The combinations should sound like modifications of one gesture, especially Stutter + Reverse/Pitch/Tape, rather than unrelated effects being restarted.

## 5. Latch

Enable Latch on a slot. A short press should toggle the effect on; the next press should toggle it off. The CC release value should not cancel a latched gesture.

## 6. Quantization

With Bitwig playing, test Free, 1/16, 1/8, and 1/4. Onsets should wait for the selected host grid boundary. Release is intentionally immediate in this alpha.

Also test **1 Bar** in both 4/4 and a non-4/4 meter such as 3/4 or 7/8. The onset should follow Bitwig's current bar boundaries rather than assuming four quarter-note beats.

## 7. MIDI remap

Use Learn to bind one slot to a different CC or Note message. Verify the old binding stops addressing it and the new binding starts addressing it. Also verify channel filtering works with a different MIDI channel.

While Learn is armed, select a different trigger with right-click. Learn should cancel rather than remain invisibly armed on the previous trigger. Right-clicking a pad should select it without firing its glitch; left-click still performs the pad.

## 8. UI / keyboard / automation

- Pads should visibly light while active.
- Clicking and holding a pad should behave like holding MIDI.
- Right-clicking a pad should select it without triggering audio.
- Number keys 1-8 should trigger slots only while the plugin UI owns keyboard focus.
- Changing the selected slot should retarget the lower editor without changing audio state.
- Host automation of Trigger 1-8 should create the same performance edges as UI/MIDI triggering.

## 9. Things worth reporting precisely

For timing or audio bugs, note:

- Bitwig buffer size / sample rate
- effect(s) active
- hold vs latch
- quantization mode
- incoming binding, e.g. CC20 channel 16
- whether the trigger came from MIDI, mouse, automation, or keyboard
- whether the issue occurs only when combining effects

Clicks at intentional hard glitch boundaries can be stylistic; clicks specifically on activation/release or unrelated to the configured gesture are bugs.
