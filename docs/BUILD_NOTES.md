# Build notes

Every push to `main` builds both Windows formats:

- `GlitchDeck.clap`, validated with `clap-validator`
- `GlitchDeck.vst3`, validated with pluginval strictness level 3

Successful builds upload both formats as workflow artifacts and publish them together in a prerelease. CLAP is the preferred Bitwig format because GlitchDeck's pad workflow depends on direct MIDI CC events for internal MIDI Learn.

The CLAP wrapper is pinned to a known revision of `free-audio/clap-juce-extensions` with JUCE 9 compatibility fixes. V1 is intentionally being integrated in vertical slices: playable trigger path first, then CI/compiler cleanup, then performance testing in Bitwig, then additional glitch engines.
