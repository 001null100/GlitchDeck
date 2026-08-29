# MIDI / Pad Setup

GlitchDeck is designed to let a controller keybed keep behaving like a musical keyboard while dedicated pads control glitch triggers.

## Use the CLAP build in Bitwig

GlitchDeck V1 is CLAP-only. Its performance note port advertises both raw MIDI and native CLAP note events, with raw MIDI preferred so Control Change messages can reach plugin-side bindings and MIDI Learn.

On Windows, the standard system CLAP directory is:

`C:\Program Files\Common Files\CLAP\`

After copying `GlitchDeck.clap` there, rescan plug-ins in Bitwig and load the **CLAP** entry.

## Recommended scheme: pads send CC, keys send notes

Do **not** assign the GlitchDeck pads to C1-G1 notes if those pitches are also reachable from the keybed. A MIDI Note 36 from a pad is indistinguishable from MIDI Note 36 from a key unless you separate them by channel.

Instead, configure the pads as momentary MIDI CC trigger/release controls:

- Pad 1: CC 20, channel 16, press 127, release 0
- Pad 2: CC 21, channel 16, press 127, release 0
- Pad 3: CC 22, channel 16, press 127, release 0
- Pad 4: CC 23, channel 16, press 127, release 0
- Pad 5: CC 24, channel 16, press 127, release 0
- Pad 6: CC 25, channel 16, press 127, release 0
- Pad 7: CC 26, channel 16, press 127, release 0
- Pad 8: CC 27, channel 16, press 127, release 0

These are GlitchDeck's defaults. MIDI Learn also supports arbitrary Notes or CCs and records the incoming channel.

## Impact LX25 mk3 setup

The LX mk3 can program each pad independently.

1. Press **Shift + Setup/Preset** to enter Setup.
2. Select **Ctl** with the large encoder and press **Select**.
3. Select **Asg**, press **Select**, hit Pad 1, and set its assignment type to **Trg** (MIDI CC trigger/release). Repeat for Pads 2-8.
4. Return to the Control menu, select **CC**, press **Select**, and assign Pads 1-8 to CC 20-27.
5. Select **d1** and set each pad's trigger/press value to **127**.
6. Select **d2** and set each pad's release value to **0**.
7. Select **Ch** and set all eight pads to **16**. The keybed can remain on the normal global channel, usually channel 1.
8. Use **Sav** if you want to store the controller configuration in a pad map/preset slot.

Now the pads no longer generate musical notes, so playing the same pitches on the keybed cannot accidentally trigger GlitchDeck and striking a pad cannot play the instrument.

## GlitchDeck MIDI Learn and the MIDI IN diagnostic

For any trigger slot:

1. Select the trigger.
2. Click **LEARN**.
3. Hit the physical pad once.
4. The button updates to the received binding, for example `CC 20 · CH 16`.

The label above Learn continuously shows the last MIDI/note event that **Bitwig actually delivered to GlitchDeck**:

- `MIDI IN · NO HOST MIDI` means the plugin has not received a qualifying raw MIDI or native CLAP note event since activation.
- `MIDI IN · CC23 CH16 127` means Bitwig delivered CC23, channel 16, value 127 to the plugin.
- note input appears as a compact note/channel ON or OFF message.

This is deliberately different from an external MIDI tester. A tester proves that the controller emitted bytes. It does **not** prove that Bitwig's controller integration inserted those bytes into the track/device-chain MIDI signal that reaches a CLAP plug-in.

Learn accepts Note On messages or CC values >= 64. For a learned CC binding, values >= 64 are treated as pad-down and values < 64 as pad-up. The Nektar's `Trg` assignment therefore maps naturally to hold/release behavior when the messages reach the plug-in.

## `NO HOST MIDI`: isolate the Bitwig boundary

Bitwig's device chain is capable of carrying MIDI CC streams to plug-ins, but a dedicated controller extension can process hardware MIDI before it becomes track/device-chain input. The Nektar mk3 Bitwig integration is built heavily around Bitwig Remote Controls, so the following tests separate those paths.

### A. Bitwig-generated CC control test

1. Insert Bitwig's **MIDI CC** device immediately before GlitchDeck in the same device chain.
2. Set its MIDI channel to **16**.
3. Assign one knob to **CC20**.
4. Move that knob between 0 and 127 while watching GlitchDeck's `MIDI IN` line.

Expected result: `MIDI IN` should show CC20/channel 16 activity. If it does, GlitchDeck's CLAP raw-MIDI port and parser are working inside Bitwig and the physical-controller route is the remaining problem.

### B. Bypass the Nektar-specific controller extension

This is a diagnostic, not a permanent recommendation.

1. Open **Dashboard → Settings → Controllers**.
2. Temporarily disable or remove the Nektar-specific Impact LX mk3 controller entry so it no longer owns the Impact input port.
3. Add **Generic → MIDI Keyboard** and select the Impact's MIDI input port.
4. On the track containing GlitchDeck, choose that Generic controller as the note/MIDI input, or leave the track on **All inputs**.
5. Hit the programmed CC pad and watch `MIDI IN`.

If the same hardware pad now appears as `CC20…CC27 CH16`, the Nektar-specific Bitwig extension is consuming or diverting the pad CC before it reaches the track stream. Re-enable the normal Nektar integration afterward.

### C. Host mapping fallback

If you want the Nektar integration active and Bitwig continues to consume the pad CCs, GlitchDeck exposes Trigger 1-8 as host parameters and on a dedicated **Triggers** Remote Controls page.

Open Bitwig's **Mappings Browser** and set **Map source priority** so raw mappings get first chance at the incoming controller message. Then map each physical pad directly to the corresponding GlitchDeck Trigger parameter. This bypasses plugin-side MIDI Learn entirely: Bitwig converts the hardware CC into a CLAP parameter event, which GlitchDeck already treats as the same trigger edge as its UI and automation.

This is a functional fallback, not a replacement for the raw-MIDI path. Plugin-side **LEARN** cannot discover a CC number/channel when Bitwig never forwards that MIDI event to the plugin.

## Native note fallback

A second diagnostic is to program one pad as an otherwise-unused MIDI note on channel 16. GlitchDeck accepts both raw MIDI notes and native CLAP Note On/Off events. If notes appear in `MIDI IN` while programmed CCs do not, Bitwig is forwarding note data from the controller but filtering/diverting arbitrary CC messages.

## Why channel 16?

The separate channel is not strictly required for CC bindings, but it gives the pads an extra identity layer and reduces accidental collisions with other controller mappings. GlitchDeck can learn any channel, and a stored channel value of 0 means omni.
