# MIDI / Pad Setup

GlitchDeck is designed to let a controller keybed keep behaving like a musical keyboard while dedicated pads control glitch triggers.

## Recommended scheme: pads send CC, keys send notes

Do **not** assign the GlitchDeck pads to C1-G1 notes if those pitches are also reachable from the keybed. A MIDI Note 36 from a pad is indistinguishable from MIDI Note 36 from a key.

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
8. Select **Sav**, choose an unused Pad Map (for example PM8) with the pad-map arrows, and press **Select** to save it.

Now the pads no longer generate musical notes, so playing the same pitches on the keybed cannot accidentally trigger GlitchDeck and striking a pad cannot play the instrument.

## GlitchDeck MIDI Learn

For any trigger slot:

1. Select the trigger.
2. Click **LEARN**.
3. Hit the physical pad once.
4. The button updates to the received binding, for example `CC 20 · CH 16`.

Learn accepts Note On messages or CC values >= 64. For a learned CC binding, values >= 64 are treated as pad-down and values < 64 as pad-up. The Nektar's `Trg` assignment therefore maps naturally to hold/release behavior.

If **LEARN** remains waiting after you hit a pad, GlitchDeck did not receive the MIDI event. Check the Bitwig track input/controller routing before debugging the effect itself.

## Why channel 16?

The separate channel is not strictly required for CC bindings, but it gives the pads an extra identity layer and reduces accidental collisions with other controller mappings. GlitchDeck can learn any channel, and a stored channel value of 0 means omni.
