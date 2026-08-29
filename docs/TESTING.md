# GlitchDeck V1 alpha test pass

Put the native `GlitchDeck.clap` after an audio-producing instrument/effect chain in Bitwig, feed it something rhythmically obvious, and keep the transport running.

## 1. Basic load / editor geometry

- Confirm `GlitchDeck.clap` scans and opens without warnings.
- Confirm the complete editor is visible, especially the right edge and Global Mix area.
- On a Windows display using non-100% scaling if available, resize the Bitwig plug-in window smaller and larger. The embedded editor should follow the host frame instead of remaining physically wider and clipped on the right.
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

The Learn area now includes a **MIDI IN** diagnostic. Before hitting anything it should read `NO HOST MIDI`. Press and release each configured pad and watch that line as well as the effect.

A Nektar `Trg` pad configured as intended sends 127 on press and 0 on release. If Bitwig forwards it to the plug-in, the label should show messages such as `CC23 CH16 127` and then `CC23 CH16 0`.

Then test Learn explicitly:

1. Select a slot.
2. Click **LEARN**.
3. Hit any configured pad.
4. Confirm the binding updates immediately, for example `CC 20 · CH 16`.

Interpret failures precisely:

- **MIDI IN updates correctly but Learn/trigger fails:** GlitchDeck's event interpretation is broken.
- **MIDI IN remains `NO HOST MIDI` while an external MIDI tester sees the controller:** Bitwig/controller routing did not deliver that event to the CLAP device.
- **Native CLAP notes appear but CCs do not:** note routing works, raw CC is being filtered upstream.

If hardware CC remains absent, perform a control test with a Bitwig-generated CC immediately upstream of GlitchDeck. Send CC20 channel 16 at 127/0. If that reaches `MIDI IN`, the plug-in's CLAP raw-MIDI path is alive and the Nektar controller layer is the remaining boundary.

## 3. Standalone transport gestures

Test Reverse, Tape Stop, Pitch Dive, and Pitch Rise individually before combining them.

- **Reverse alone** should reverse a bounded recent fragment. It should not simply march backward through progressively older history forever.
- **Pitch Rise alone** should remain coherent throughout the rise. It must not become silent, stale, or read ahead of the live write head as playback exceeds 1x.
- **Pitch Dive alone** should remain a streaming recent-history rate gesture.
- **Tape Stop alone** should decelerate the streaming history head.

The automated DSP regression suite specifically checks bounded standalone Reverse, bounded standalone Pitch Rise, and that Pitch Rise cannot escape its captured history region even after accelerating for many samples.

## 4. Shared transport combinations

Try these deliberately:

- Hold Stutter, then add Pitch Dive.
- Hold Stutter, then add Pitch Rise. Pitch Rise must modify the current Stutter fragment without recapturing a new region.
- Hold Stutter, tap/hold Reverse. Reverse must flip the already-playing Stutter region rather than capturing a new one.
- Release Stutter while Reverse remains held. Reverse should remain on the captured fragment rather than jumping into unbounded old-history playback.
- Hold Stutter and add Tape Stop.
- Hold Microloop and add Bitcrush.
- Hold any temporal gesture and add Dropout.

The combinations should sound like modifications of one gesture, especially Stutter + Reverse/Pitch/Tape, rather than unrelated effects being restarted.

## 5. Latch

Enable Latch on a slot. A short press should toggle the effect on; the next press should toggle it off. The CC release value should not cancel a latched gesture.

## 6. Quantization

With Bitwig playing, test Free, 1/16, 1/8, and 1/4. Onsets should wait for the selected host grid boundary. Release is intentionally immediate in this alpha.

Also test **1 Bar** in both 4/4 and a non-4/4 meter such as 3/4 or 7/8. The onset should follow Bitwig's current bar boundaries rather than assuming four quarter-note beats.

## 7. MIDI remap and dialect coverage

Use Learn to bind one slot to a different CC or Note message. Verify the old binding stops addressing it and the new binding starts addressing it. Also verify channel filtering works with a different MIDI channel.

GlitchDeck now accepts both raw MIDI Note/CC events and native CLAP Note On/Off/Choke events. If practical, verify that a note source in Bitwig shows activity and can be learned even if the hardware CC path is still filtered.

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
- Windows display scaling percentage if the issue is GUI-related
- effect(s) active
- hold vs latch
- quantization mode
- incoming binding, e.g. CC20 channel 16
- the exact **MIDI IN** text shown after the event
- whether the trigger came from MIDI, mouse, automation, or keyboard
- whether the issue occurs only when combining effects

Clicks at intentional hard glitch boundaries can be stylistic; clicks specifically on activation/release or unrelated to the configured gesture are bugs.
