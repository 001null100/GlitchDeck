# Build notes

Every push to `main` builds the Windows VST3, validates it with pluginval strictness level 3, uploads the build artifact, and publishes a prerelease when validation succeeds.

V1 is intentionally being integrated in vertical slices: playable trigger path first, then CI/compiler cleanup, then performance testing in Bitwig, then additional glitch engines.
