# Build notes

GlitchDeck now ships as a native Windows CLAP plugin.

Every push builds `GlitchDeck.clap`, runs `clap-validator`, packages the validated plugin, and uploads it as a workflow artifact. Successful pushes to `main` also publish the package as a GitHub prerelease so test builds are available under **Releases**, not only inside Actions.

## Host framework

GlitchDeck pins an exact revision of [`null-clap`](https://github.com/001null100/null-clap). null-clap owns:

- CLAP lifecycle and entry/factory plumbing
- audio and note ports
- sample-accurate parameter/event routing
- state serialization
- remote-control pages
- GUI host extension plumbing

GlitchDeck owns the actual DSP, MIDI interpretation, trigger scheduling, MIDI Learn, and product-specific parameters. JUCE 9 is linked only for GUI components and native window embedding.

## Validation

CI uses the native `.clap` artifact as the validation target. A build is not published from `main` unless compilation and clap-validator both succeed.
