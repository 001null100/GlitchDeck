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

## Current Bitwig finding

The decisive control test has already been performed: a **Bitwig-generated MIDI CC device immediately upstream of GlitchDeck still produced `NO HOST MIDI`**. That means this is not currently being treated as a Nektar-only configuration problem and should not be "fixed" inside GlitchDeck's CC parser.

The current candidate therefore moves the host-routing change into null-clap itself:

- null-clap PR #4 adds a reusable `nullclap_enable_note_input_routing(target)` capability.
- The capability preserves the consumer's audio-effect descriptor features and adds host-facing `note-effect` metadata at the CLAP factory boundary.
- The created `clap_plugin_t` is explicitly pointed at the same augmented descriptor, so discovery metadata and instance metadata cannot disagree.
- null-clap's unit test creates a real probe plug-in through the factory and passes a live raw channel-16 CC (`0xBF`, CC20, 127) through `clap_process_t::in_events`; `Plugin::onEvent()` must receive the bytes unchanged.
- GlitchDeck opts into that capability instead of carrying a Bitwig-specific descriptor workaround.

## Test the framework candidate

With the candidate CLAP installed, perform this first:

1. Put Bitwig's **MIDI CC** device immediately before GlitchDeck.
2. Send CC20 on channel 16 at 127, then 0.
3. Watch `MIDI IN`.

If it now shows `CC20 CH16 127/0`, the framework routing metadata solved the host boundary. Then test the physical pads and MIDI Learn.

If it **still** says `NO HOST MIDI`, stop there. The framework's own process-event contract is already proven by CI, so the remaining problem is Bitwig's construction of the CLAP note/MIDI route for this kind of audio effect. That result is useful and should drive the next null-clap/host-compatibility experiment rather than another GlitchDeck parser rewrite.

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
