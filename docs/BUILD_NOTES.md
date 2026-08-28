# Build notes

GlitchDeck now ships as a native Windows CLAP plugin.

Every pull request build and every push to `main` builds `GlitchDeck.clap`, runs `clap-validator`, packages the validated plugin, and uploads it as a workflow artifact. Successful same-repository pull request builds are also published as GitHub prereleases so they can be tested in Bitwig before merging. Successful pushes to `main` publish their own alpha prerelease as the mainline build.

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

CI uses the native `.clap` artifact as the validation target. No GitHub prerelease is published unless compilation and clap-validator both succeed.
