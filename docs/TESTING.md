# GlitchDeck V1 alpha test pass

Put the native `GlitchDeck.clap` after an audio-producing instrument/effect chain in Bitwig, feed it something rhythmically obvious, and keep the transport running.

## 1. Basic load / editor geometry

- Confirm `GlitchDeck.clap` scans and opens without warnings.
- Confirm the complete editor is visible, especially the right edge and Global Mix area.
- On a Windows display using non-100% scaling if available, resize the Bitwig plug-in window smaller and larger. The embedded editor should follow the host frame instead of remaining physically wider and clipped on the right.
- The current branch uses null-clap's upstream `PhysicalPixelGuiSizing` path from build 43 rather than a GlitchDeck-local DPI workaround.
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

The Learn area includes a **MIDI IN** diagnostic. Before hitting anything it should read `NO HOST MIDI`. Press and release each configured pad and watch that line as well as the effect.

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

If hardware CC remains absent, perform a control test with a Bitwig-generated CC immediately upstream of GlitchDeck. Send CC20 channel 16 at 127/0. If that reaches `MIDI IN`, the plug-in's CLAP raw-MIDI path is alive and the controller integration is the remaining boundary.

## 3. Standalone transport gestures

Test Reverse, Tape Stop, Pitch Dive, and Pitch Rise individually before combining them.

- **Reverse alone:** it should reverse through one recent captured fragment exactly once. It must not repeatedly jump from the oldest sample back to the newest sample. If you continue holding after the fragment is exhausted, the endpoint may hold until release.
- **Pitch Rise alone:** listen specifically for rapid restarts, clicks, periodic resets, or a repeated fragment as the pitch accelerates. The head should traverse a sufficiently long recent capture once, rise continuously, and then hold the endpoint if the gesture outlasts available source history. It must not wrap back to the capture start.
- **Pitch Dive alone:** it should traverse a bounded recent capture while decelerating in pitch. It should not wander into arbitrary older history or repeatedly restart the capture.
- **Tape Stop alone:** it should decelerate the recent-history streaming head as before.

The DSP regression suite now checks the seam behavior directly: standalone Reverse must stop rather than wrap, Pitch Rise must advance monotonically across a ramp source without wrapping or reading live audio, and Stutter must still wrap normally.

## 4. Shared transport combinations

Try these deliberately:

- Hold Stutter, then add Pitch Dive. Pitch Dive should bend the already-repeating fragment rather than replacing it with a one-shot capture.
- Hold Stutter, then add Pitch Rise. Pitch Rise should accelerate the current Stutter fragment and the fragment should continue looping because Stutter owns the transport.
- Hold Stutter, tap/hold Reverse. Reverse should flip the already-playing Stutter read head rather than capture a new fragment.
- Release Stutter while Reverse remains held. Reverse should continue using the shared captured fragment rather than jumping into live/unbounded history.
- Hold Stutter and add Tape Stop.
- Hold Microloop and add Bitcrush.
- Hold any temporal gesture and add Dropout.

The key distinction is **standalone versus modifier**: standalone Reverse/Pitch use one-shot captures to avoid arbitrary wrap seams, while Reverse/Pitch layered onto Stutter/Microloop inherit that repeating loop because the loop itself is the intended effect.

## 5. Fresh-instance history safety

Immediately after inserting a fresh GlitchDeck instance, trigger Reverse or Pitch Rise before eight seconds of audio have accumulated. The result should use only history that actually exists. It must not expose a long silent/zero-filled region just because the configured gesture length is longer than the captured history currently available.

## 6. Latch

Enable Latch on a slot. A short press should toggle the effect on; the next press should toggle it off. The CC release value should not cancel a latched gesture.

## 7. Quantization

With Bitwig playing, test Free, 1/16, 1/8, and 1/4. Onsets should wait for the selected host grid boundary. Release is intentionally immediate in this alpha.

Also test **1 Bar** in both 4/4 and a non-4/4 meter such as 3/4 or 7/8. The onset should follow Bitwig's current bar boundaries rather than assuming four quarter-note beats.

## 8. MIDI remap and dialect coverage

Use Learn to bind one slot to a different CC or Note message. Verify the old binding stops addressing it and the new binding starts addressing it. Also verify channel filtering works with a different MIDI channel.

GlitchDeck accepts both raw MIDI Note/CC events and native CLAP Note On/Off/Choke events. If practical, verify that a note source in Bitwig shows activity and can be learned even if the hardware CC path is still filtered.

While Learn is armed, select a different trigger with right-click. Learn should cancel rather than remain invisibly armed on the previous trigger. Right-clicking a pad should select it without firing its glitch; left-click still performs the pad.

## 9. UI / keyboard / automation

- Pads should visibly light while active.
- Clicking and holding a pad should behave like holding MIDI.
- Right-clicking a pad should select it without triggering audio.
- Number keys 1-8 should trigger slots only while the plugin UI owns keyboard focus.
- Changing the selected slot should retarget the lower editor without changing audio state.
- Host automation of Trigger 1-8 should create the same performance edges as UI/MIDI triggering.

## 10. Things worth reporting precisely

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
- for Reverse/Pitch bugs, whether the fault sounds like a wrap/restart, a click, a frozen endpoint, silence, or an incorrect pitch curve

Clicks specifically on activation/release or at unintended transport resets are bugs. A held endpoint at the end of a standalone one-shot Reverse/Pitch capture is currently intentional and should be reported only if the transition into that endpoint is itself rough.
