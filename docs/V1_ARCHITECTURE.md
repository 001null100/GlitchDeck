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

Stutter and Microloop explicitly capture and define a loop region. Reverse changes the direction of the shared playback head. Pitch Dive and Pitch Rise alter its playback rate. Tape Stop applies a deceleration curve to the same transport.

Standalone gestures do not all use the same transport mode:

- **Reverse** uses a bounded recent capture. A single backwards head cannot remain meaningfully attached to a live write head forever, so an unbounded streaming reverse would simply walk into older and older history.
- **Pitch Rise** also uses a bounded recent capture. Once playback exceeds 1x, a streaming read head can overtake the live write head and reach unwritten/current history; wrapping inside a known capture prevents that failure.
- **Pitch Dive** and **Tape Stop** can safely use a recent-history streaming head because their playback rate does not outrun the writer.
- When Reverse or Pitch Rise is added to an existing Stutter/Microloop, it modifies that existing captured region instead of recapturing it.
- If the loop source is released while Reverse or Pitch Rise remains active, the captured region is preserved. If only Pitch Dive or Tape Stop remains, playback re-anchors to recent-history streaming.

This is intentionally different from serially instantiating independent delay-style effects. A held Stutter plus Pitch Dive should sound like a single repeated fragment progressively descending, not like a pitch effect receiving an unrelated stutter processor.

## Timing

Raw MIDI, native CLAP notes, and host parameter events retain their CLAP sample offsets. Quantized onset scheduling uses CLAP transport tempo/beat information and supports Free, 1/32, 1/16, 1/8, 1/4, and 1 Bar. `1 Bar` follows the host's reported time signature and current bar start, with a 4/4 fallback if the host does not provide meter information. Release remains immediate in V1 so a performer never feels trapped by a quantized release.

## GUI scaling

CLAP's Win32 GUI contract expresses child-window dimensions in physical pixels, while JUCE component bounds are logical/DPI-scaled coordinates. GlitchDeck keeps its editor dimensions in logical pixels, records the scale supplied through `clap_plugin_gui::set_scale()`, and converts physical CLAP sizes to/from logical JUCE sizes at the host boundary. JUCE remains responsible for the actual native Windows DPI transform. This prevents the embedded child HWND from becoming wider than Bitwig's viewport at 125%, 150%, or other non-100% display scaling.

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
