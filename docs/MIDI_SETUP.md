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

This is deliberately different from an external MIDI tester. A tester proves that the controller emitted bytes. It does **not** prove that Bitwig's controller integration inserted those bytes into the track/device-chain note signal that reaches a CLAP plug-in.

Learn accepts Note On messages or CC values >= 64. For a learned CC binding, values >= 64 are treated as pad-down and values < 64 as pad-up. The Nektar's `Trg` assignment therefore maps naturally to hold/release behavior.

## If the Nektar CCs still do not appear

Use the MIDI IN diagnostic to separate two failure classes:

1. Arm Learn and hit a Nektar pad.
2. If the label changes to the expected `CCxx CH16 127` but Learn does not complete or the slot does not trigger, that is a GlitchDeck bug.
3. If the label remains `NO HOST MIDI` while a standalone MIDI tester sees the Nektar CC, the event is being filtered or consumed before it reaches GlitchDeck.

For a control test, generate a CC inside Bitwig immediately upstream of GlitchDeck using a Bitwig MIDI/CC device or another source known to feed the device-chain note signal. Send, for example, CC20 on channel 16 with value 127 then 0. If **that** appears in `MIDI IN` and triggers/learns correctly while the physical Nektar CC does not, the native CLAP path is working and the controller integration is the filter.

The Nektar/Bitwig integration also uses Bitwig Remote Controls heavily. GlitchDeck exposes a **Triggers** remote-control page containing Trigger 1-8, so Bitwig's own hardware/remote mapping is the immediate fallback even when controller CC bytes are not forwarded into the track stream.

A second fallback is to program the pads as otherwise-unused notes on channel 16. GlitchDeck accepts both raw MIDI notes and native CLAP Note On/Off events, so this can work when a controller script forwards notes but filters arbitrary CCs. Use note numbers outside the range you normally play if you want to avoid keybed collisions.

## Why channel 16?

The separate channel is not strictly required for CC bindings, but it gives the pads an extra identity layer and reduces accidental collisions with other controller mappings. GlitchDeck can learn any channel, and a stored channel value of 0 means omni.
