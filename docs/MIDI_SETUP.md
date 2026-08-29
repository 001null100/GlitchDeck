# MIDI / Pad Setup

GlitchDeck is designed to let a controller keybed keep behaving like a musical keyboard while dedicated pads control glitch triggers.

## Use the CLAP build in Bitwig

GlitchDeck V1 is CLAP-only. Its performance note port accepts raw MIDI and native CLAP note events, preferring raw MIDI so Control Change messages can reach plugin-side bindings and MIDI Learn.

On Windows, the standard system CLAP directory is:

`C:\Program Files\Common Files\CLAP\`

After copying `GlitchDeck.clap` there, rescan plug-ins in Bitwig and load the **CLAP** entry.

## Recommended pad messages

The defaults are momentary CC20-27 on MIDI channel 16, press 127 and release 0. MIDI Learn can bind arbitrary Notes or CCs and records the incoming channel.

- Pad 1: CC20
- Pad 2: CC21
- Pad 3: CC22
- Pad 4: CC23
- Pad 5: CC24
- Pad 6: CC25
- Pad 7: CC26
- Pad 8: CC27

## MIDI IN diagnostic

The editor shows the last MIDI/note event that **the host actually placed in GlitchDeck's CLAP event input**.

- `MIDI IN · NO HOST MIDI`: GlitchDeck has not received a qualifying event since activation.
- `MIDI IN · CC20 CH16 127`: the host delivered the pad-down CC.
- `MIDI IN · CC20 CH16 0`: the host delivered the release.

An external MIDI tester only proves that the controller emitted bytes. It does not prove that the CLAP instance received them.

## What has already been ruled out

A **Bitwig-generated MIDI CC device immediately upstream of GlitchDeck also produced `NO HOST MIDI`**. The failure therefore occurs before GlitchDeck's CC parser and is not being treated as a Nektar-only configuration problem.

Two earlier host-routing experiments were also rejected after Bitwig testing:

- advertising GlitchDeck as `audio-effect + note-effect`
- automatically adding the `MIDI_MPE` dialect to raw-MIDI inputs

Neither made Bitwig deliver the event, and both were broader than the CLAP semantics justified.

## Current host-role candidate

The current null-clap candidate models GlitchDeck as what it actually is: a stereo audio effect whose audio output is also controlled by incoming note/MIDI events.

The CLAP descriptor therefore advertises:

- `audio-effect`
- `instrument`
- `stereo`

CLAP defines `instrument` as a plug-in which processes note events and produces audio. `note-effect` is intended for plug-ins which process or generate note events, which GlitchDeck does not do.

The performance input remains ordinary and explicit:

- supported dialects: raw MIDI + native CLAP notes
- preferred dialect: raw MIDI
- no implicit MIDI-MPE capability

null-clap independently tests a stereo audio-input/output probe with this role profile and verifies that a live channel-16 raw CC (`0xBF`, CC20, 127) reaches `Plugin::onEvent()` byte-for-byte when the host supplies it.

## Test the current candidate

With the candidate CLAP installed:

1. Put Bitwig's **MIDI CC** device immediately before GlitchDeck.
2. Send CC20 on channel 16 at 127, then 0.
3. Watch `MIDI IN`.

If it shows `CC20 CH16 127` and `CC20 CH16 0`, the host-role classification was the missing routing piece. Then test the physical pads and MIDI Learn.

If it still says `NO HOST MIDI`, stop there. Do not debug mappings or GlitchDeck's parser. The next step is a null-clap host-role matrix probe so the remaining descriptor combinations can be tested in one Bitwig pass instead of changing GlitchDeck repeatedly.

## Learn behavior once MIDI arrives

1. Select a trigger slot.
2. Click **LEARN**.
3. Hit the desired pad once.
4. The binding should update immediately, for example `CC 20 · CH 16`.

Learn accepts Note On or CC values >= 64. For a learned CC binding, values >= 64 are pad-down and values < 64 are pad-up.

If `MIDI IN` updates but Learn or triggering fails, that is finally a GlitchDeck-side bug and should be reported with the exact `MIDI IN` text.

## Host mapping fallback

GlitchDeck exposes Trigger 1-8 as writable host parameters and on a dedicated **Triggers** Remote Controls page. Bitwig can therefore map hardware controls directly to those trigger parameters even when raw MIDI is not delivered to the plugin event input.

This is a functional fallback, not a substitute for fixing the CLAP note/MIDI route, because plugin-side Learn cannot discover a CC number/channel it never receives.

## Why channel 16?

The separate channel is not strictly required for CC bindings, but it gives the pads an extra identity layer and reduces collisions with ordinary musical input. A stored GlitchDeck channel value of 0 means omni.
