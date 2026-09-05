# GlitchDeck 0.2.1: performance reliability and editor polish

This update preserves the alpha-6 effect algorithms, existing parameter/port IDs,
MIDI CC20-27/channel-16 defaults, stereo audio topology, and NCLP v1 state format.
The shared framework is pinned to `9de3106360f324ee4b08906595c54a3e138f6cb7`.
The engine's existing alpha envelopes and Reverse/Pitch Rise regression tests
are retained rather than replacing the sound with new effect algorithms. Temporal
effects are fully wet after the attack settles; attack/release still crossfade
between captured and live audio. Intensity does not add a permanent dry blend.

## Fixed performance boundaries

- Mouse, keyboard, raw MIDI channels, native CLAP note IDs, and host trigger
  automation track independent holds. One source's release cannot steal another.
- UI queue saturation retains the newest state per pad, especially release.
  Congested intermediate taps may coalesce; no audio-thread allocation or wait is added.
- GUI parameter edits retain their latest unsent value and gesture end until the
  framework accepts them. Text entry, wheel changes and double-click resets also
  produce complete gestures.
- Native CLAP note-on velocity zero is an onset; raw MIDI note-on velocity zero
  is a release. Wildcard note-offs/chokes, CC120/123, and controller reset CC121
  release their applicable owners without requiring a particular editor state.
- Duplicate held CC messages do not toggle a latch repeatedly. A second press
  cancels an onset that is still waiting on a quantization grid. Switching latch
  mode, effect, or MIDI binding releases that pad's old performance state.
- Quantized starts are bound to the musical beat chosen at the press, not a fresh
  grid at every audio block. Continuous tempo changes and ramps retime that beat;
  stop, seek, or missing transport cancels queued quantized starts. Free triggers
  still work when the transport is stopped. Existing active latches are not
  automatically disabled by merely stopping playback.
- Configuration changes at sample zero apply before a new capture. Targeted or
  invalid trigger-parameter events cannot masquerade as global trigger presses.
- Learn uses a generation-tagged publication and a host main-thread callback, so
  cancel/re-arm cannot apply an older result and no editor timer is required.
- State restore requests the audio-owned performance reset at a block boundary,
  rather than clearing history concurrently from the main thread. Unknown opaque
  payloads are rejected; allocation failures during activation return failure.

## Editor

The existing dark/cyan/magenta identity is retained. Pads distinguish HOLD,
LATCH, QUEUED and ACTIVE. The selected pad has explicit MIDI type, number and
channel controls in addition to Learn. The full-width MIDI status distinguishes
no delivered host MIDI from delivered messages and matching bindings.

Keys **1-8** remain the pad shortcuts. Right-click selects without playing.
**PANIC / Escape** releases all gates/latches, cancels queued starts and Learn,
and retains settings. Panic uses the configured release envelopes, so it is not
an instantaneous hard mute. Escape cancels ordinary text editing while a value
field has focus; use the button for panic in that situation.

Closing/hiding the editor or losing its focus releases held mouse/keyboard
inputs without disturbing independent MIDI/automation holds or deliberately
latched effects. A physically held keyboard shortcut cannot immediately retrigger
after Panic until the shortcut keys have been released.

Percentage text accepts `50` or `50%` as 50%, while CLAP values remain 0..1.
Invalid percentage text preserves the current value. Double-click restores each
control's registered per-pad default. Timer refresh does not overwrite an active
value editor or an open dropdown. The layout has explicit minimum/default/large
render tests and a minimum logical size of 820x620.

## Automated checks

The headless `GlitchDeckContracts` target runs the actual CLAP plugin/event/state
code, excluding only construction of its JUCE window delegate. It is not a
separate reimplementation of MIDI dispatch. It covers direct channel-16 CC,
source overlap, wildcard choke, latching, learn cancellation, exact onset/release
samples, same-offset configuration, transport/tempo changes, queue saturation,
and GUI gesture backpressure. Tests remain active in Release.

Full builds also run `GlitchDeckEditorTests`, render PNGs at 820x620, 1040x740 and
1500x1000, check that every child fits, and exercise percentage entry/Panic wiring.
Windows and Linux CI run the engine, plugin, editor and five framework suites.
A separate headless workflow runs Debug and optimized ASan/UBSan contracts.

## Bitwig verification still required

The historical `NO HOST MIDI` symptom occurred with both physical-controller CC
and Bitwig's own upstream MIDI CC device. Parser tests cannot prove that a host
will deliver events to a device. This update retains the existing
`audio-effect + instrument + stereo` candidate and explicit MIDI/CLAP input port;
it does not introduce another speculative role/MPE switch or claim that the
Bitwig routing question has been settled.

For the Windows release, remove stale duplicate copies, replace GlitchDeck.clap,
rescan/restart Bitwig and insert a fresh instance. First exercise mouse/keys, then
send upstream CC20 on channel16 at 127/0. Note whether the footer remains
`NO HOST MIDI`, reports `IN` without `MATCH`, or reports a matching press/release.
Test Learn, saving/reloading, Reverse/Pitch Rise against alpha-6, full-wet Dropout,
125%/150% display scaling, and closing the editor while a pad is held.

Native Linux builds do not yet provide an embedded editor: the shipped GUI
delegate remains Win32-only. Linux CI rendering tests the JUCE component under
Xvfb, not Linux host-window integration. No interactive DAW result is implied by
CI or clap-validator success.
