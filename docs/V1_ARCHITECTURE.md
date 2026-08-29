# GlitchDeck V1 architecture

## Product rule

GlitchDeck is deterministic and performance-first. A trigger press must produce the configured gesture every time. There is no probability layer in V1.

## Host architecture

GlitchDeck is a native CLAP plugin built on `null-clap`.

- null-clap owns CLAP lifecycle, audio/note ports, parameter/event routing, state, remote controls, and GUI extension plumbing.
- GlitchDeck owns DSP, MIDI interpretation, MIDI Learn, trigger scheduling, and product behavior.
- JUCE is used only for editor components and native window embedding. There is no JUCE AudioProcessor/APVTS/MIDI/playhead/plugin wrapper layer.

The performance note port advertises both raw MIDI and native CLAP note dialects, preferring raw MIDI so Control Change messages can reach the eight performance bindings. GlitchDeck handles `CLAP_EVENT_MIDI` for CC/Note traffic and `CLAP_EVENT_NOTE_ON/OFF/CHOKE` for hosts that use native CLAP note events. Quantized scheduling reads the current native `clap_process_t` transport context.

The editor also keeps a lock-free atomic mirror of the last host-delivered MIDI/note event. This is diagnostic only and lets the UI distinguish a GlitchDeck mapping problem from a controller/host routing problem without logging or allocating on the audio thread.

## Signal model

1. Input is continuously written into an 8-second stereo circular history buffer.
2. Trigger slots change how recent history is read and transformed.
3. Compatible temporal gestures share one transport/read head so combinations have a defined musical meaning.
4. Destruction effects are applied after temporal playback.
5. Per-trigger attack/release envelopes and the global mix return to clean audio without hard discontinuities.

## Trigger slots

Eight slots default to CC20-27 on MIDI channel 16. Every slot exposes:

- momentary writable trigger parameter
- effect type
- hold/latch behavior
- MIDI binding type, number, and channel
- onset quantization
- stereo behavior
- intensity
- length
- attack
- release
- shape

The initial effect set is Stutter, Microloop, Reverse, Tape Stop, Pitch Dive, Pitch Rise, Bitcrush, and Dropout.

Writable CLAP parameters participate in project state so hosts can reproduce their values exactly. Trigger parameter values therefore serialize like other writable parameters. The actual performance gesture state is separate and non-persistent: state load resets pending edges, envelopes, active slots, and the glitch engine before updating the restored configuration. A saved trigger value of `1` remains host-visible after load but does not itself re-engage the DSP gesture.

## Temporal grammar

Stutter and Microloop are the repeating transports. They capture a recent region and wrap the shared read head inside that region.

Standalone Reverse, Pitch Dive, and Pitch Rise use **one-shot captures** instead. They read a bounded recent region once and clamp at its endpoint rather than wrapping back across an arbitrary waveform seam. This distinction matters most for Pitch Rise: an accelerating read head can cross a short capture boundary many times per gesture, turning a mathematically valid wrap into a rapid audible restart/click pattern.

- **Reverse alone** starts at the newest end of a recent capture, travels backward once, then holds the oldest captured sample while the gesture remains held.
- **Pitch Rise alone** traverses a longer recent capture once. The capture is sized conservatively for the maximum configured rise speed so the head does not overtake the live writer or wrap during the ramp. If held past the available capture, it holds the endpoint until release.
- **Pitch Dive alone** also uses a bounded one-shot capture, so its source remains deterministic and cannot drift into unrelated history.
- **Tape Stop alone** uses a recent-history streaming head because its playback rate only decelerates and therefore cannot overtake the live writer.
- **Reverse/Pitch added to Stutter or Microloop** act as modifiers of the existing repeating loop. They do not replace that shared loop with a one-shot capture.
- Capture length is limited by the amount of history actually filled since activation, preventing newly loaded instances from reading uninitialized/zero history.
- Fractional interpolation in captured transport always keeps both interpolation taps inside the capture. Repeating captures wrap both taps; one-shot captures clamp both taps at the endpoint.

This is intentionally different from serially instantiating independent delay-style effects. A held Stutter plus Pitch Dive should sound like a single repeated fragment progressively descending, not like a pitch effect receiving an unrelated stutter processor.

## Timing

Raw MIDI, native CLAP notes, and host parameter events retain their CLAP sample offsets. Quantized onset scheduling uses CLAP transport tempo/beat information and supports Free, 1/32, 1/16, 1/8, 1/4, and 1 Bar. `1 Bar` follows the host's reported time signature and current bar start, with a 4/4 fallback if the host does not provide meter information. Release remains immediate in V1 so a performer never feels trapped by a quantized release.

## GUI scaling

CLAP's Win32 GUI contract expresses child-window dimensions in physical pixels, while JUCE component bounds are logical/DPI-scaled coordinates. The physical/logical conversion is centralized upstream in null-clap's `PhysicalPixelGuiSizing` helper. GlitchDeck supplies only its logical design/min/max sizes and delegates host scale, physical `get_size`/`set_size`, and resize clamping to the framework helper. This is the same upstream path used by the validated build 43 scaling fix and avoids each plug-in carrying a slightly different Win32 DPI implementation.

## Real-time constraints

The audio path performs no file IO and takes no mutexes. History storage is allocated at activation. Trigger scheduling uses fixed-size real-time-safe storage. DSP state is mutated on the audio thread; the editor reads atomic mirrors and sends host-visible parameter gestures through null-clap.

## Planned next layers

After the core performance feel is validated:

- Smart Capture timing
- End-of-loop release
- Scramble / slice engine
- spectral freeze
- synced loop-length modes
- deeper stereo modes
- trigger modifiers such as Double and Brake
- optional high-quality time stretch
