#include "GlitchDeckPlugin.hpp"
#ifndef GLITCHDECK_HEADLESS_TEST
#include "JuceGuiDelegate.hpp"
#endif

#include <clap/events.h>
#include <clap/fixedpoint.h>
#include <clap/plugin-features.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <memory>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace
{
constexpr std::array<double, GlitchDeckPlugin::numSlots> defaultLengths {
    125.0, 12.0, 250.0, 600.0, 450.0, 350.0, 180.0, 120.0
};

constexpr std::uint32_t steppedAutomatable = CLAP_PARAM_IS_AUTOMATABLE | CLAP_PARAM_IS_STEPPED;
constexpr std::uint32_t triggerFlags = steppedAutomatable | CLAP_PARAM_REQUIRES_PROCESS;

constexpr std::uint32_t activityValid = 1u << 31;
constexpr std::uint32_t activityCc = 1u << 30;
constexpr std::uint32_t activityDown = 1u << 29;
constexpr std::uint32_t activityChannelShift = 24;
constexpr std::uint32_t activityNumberShift = 16;
constexpr std::uint32_t activityValueShift = 8;

std::string slotModule(int slot)
{
    return "Trigger " + std::to_string(slot + 1);
}

std::string midiNoteName(int note)
{
    static constexpr std::array<const char*, 12> names {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };
    note = std::clamp(note, 0, 127);
    return std::string(names[static_cast<std::size_t>(note % 12)]) + std::to_string(note / 12 - 2);
}
} // namespace

const clap_plugin_descriptor_t& GlitchDeckPlugin::descriptor() noexcept
{
    static const char* const features[] {
        CLAP_PLUGIN_FEATURE_AUDIO_EFFECT,
        CLAP_PLUGIN_FEATURE_INSTRUMENT,
        CLAP_PLUGIN_FEATURE_STEREO,
        nullptr,
    };

    static const clap_plugin_descriptor_t descriptor {
        CLAP_VERSION,
        "dev.nullexo.glitchdeck",
        "GlitchDeck",
        "Null Exo",
        "https://github.com/001null100/GlitchDeck",
        "",
        "",
        "0.2.1",
        "Playable performance-first glitch processor",
        features,
    };
    return descriptor;
}

GlitchDeckPlugin::GlitchDeckPlugin(const clap_host_t* host)
    : Plugin(&descriptor(), host)
{
    for (auto& active : visibleActive_)
        active.store(false, std::memory_order_relaxed);

    registerParameters();
    registerPorts();
    registerRemoteControls();
#ifndef GLITCHDECK_HEADLESS_TEST
    setGuiDelegate(std::make_unique<JuceGuiDelegate>(*this));
#endif
}

const std::array<const char*, 8>& GlitchDeckPlugin::effectNames() noexcept
{
    static const std::array<const char*, 8> names {
        "Stutter", "Microloop", "Reverse", "Tape Stop",
        "Pitch Dive", "Pitch Rise", "Bitcrush", "Dropout"
    };
    return names;
}

const std::array<const char*, 6>& GlitchDeckPlugin::quantizeNames() noexcept
{
    static const std::array<const char*, 6> names { "Free", "1/32", "1/16", "1/8", "1/4", "1 Bar" };
    return names;
}

const std::array<const char*, 4>& GlitchDeckPlugin::stereoNames() noexcept
{
    static const std::array<const char*, 4> names { "Linked", "Spread", "Swap", "Mono" };
    return names;
}

const std::array<const char*, 2>& GlitchDeckPlugin::midiTypeNames() noexcept
{
    static const std::array<const char*, 2> names { "Note", "CC" };
    return names;
}

const glitchdeck::ids::Slot& GlitchDeckPlugin::slotIds(int slot) const noexcept
{
    return ids_[static_cast<std::size_t>(std::clamp(slot, 0, numSlots - 1))];
}

double GlitchDeckPlugin::parameterValue(clap_id id) const noexcept
{
    return parameters().value(id);
}

int GlitchDeckPlugin::parameterInt(clap_id id) const noexcept
{
    return static_cast<int>(std::llround(parameters().value(id)));
}

bool GlitchDeckPlugin::parameterBool(clap_id id) const noexcept
{
    return parameters().value(id) >= 0.5;
}

void GlitchDeckPlugin::registerParameters()
{
    auto mix = nullclap::ParameterSpec::continuous(glitchdeck::ids::mix, "Global Mix", "Global", 0.0, 1.0, 1.0);
    mix.unit = "";
    mix.displayPrecision = 3;
    parameters().add(std::move(mix));

    const auto effects = std::vector<std::string>(effectNames().begin(), effectNames().end());
    const auto quantize = std::vector<std::string>(quantizeNames().begin(), quantizeNames().end());
    const auto stereo = std::vector<std::string>(stereoNames().begin(), stereoNames().end());
    const auto midiTypes = std::vector<std::string>(midiTypeNames().begin(), midiTypeNames().end());

    for (int slot = 0; slot < numSlots; ++slot)
    {
        const auto& id = ids_[static_cast<std::size_t>(slot)];
        const auto module = slotModule(slot);

        auto trigger = nullclap::ParameterSpec::continuous(id.trigger, "Trigger", module, 0.0, 1.0, 0.0, triggerFlags);
        trigger.displayPrecision = 0;
        parameters().add(std::move(trigger));

        parameters().add(nullclap::ParameterSpec::choice(id.effect, "Effect", module, effects, static_cast<std::size_t>(slot)));

        auto latch = nullclap::ParameterSpec::continuous(id.latch, "Latch", module, 0.0, 1.0, 0.0, steppedAutomatable);
        latch.displayPrecision = 0;
        parameters().add(std::move(latch));

        parameters().add(nullclap::ParameterSpec::choice(id.midiType, "MIDI Type", module, midiTypes, 1));

        auto midiNumber = nullclap::ParameterSpec::continuous(
            id.midiNumber, "MIDI Number", module, 0.0, 127.0, 20.0 + slot, steppedAutomatable);
        midiNumber.displayPrecision = 0;
        parameters().add(std::move(midiNumber));

        auto midiChannel = nullclap::ParameterSpec::continuous(
            id.midiChannel, "MIDI Channel", module, 0.0, 16.0, 16.0, steppedAutomatable);
        midiChannel.displayPrecision = 0;
        parameters().add(std::move(midiChannel));

        parameters().add(nullclap::ParameterSpec::choice(id.quantize, "Quantize", module, quantize, 0));
        parameters().add(nullclap::ParameterSpec::choice(id.stereo, "Stereo", module, stereo, 0));

        parameters().add(nullclap::ParameterSpec::continuous(
            id.intensity, "Intensity", module, 0.0, 1.0, slot == 7 ? 1.0 : 0.82));

        auto length = nullclap::ParameterSpec::continuous(
            id.length, "Length", module, 2.0, 1500.0, defaultLengths[static_cast<std::size_t>(slot)]);
        length.unit = "ms";
        length.displayPrecision = 2;
        parameters().add(std::move(length));

        auto attack = nullclap::ParameterSpec::continuous(id.attack, "Attack", module, 0.1, 100.0, 2.0);
        attack.unit = "ms";
        attack.displayPrecision = 2;
        parameters().add(std::move(attack));

        auto release = nullclap::ParameterSpec::continuous(id.release, "Release", module, 0.1, 300.0, 20.0);
        release.unit = "ms";
        release.displayPrecision = 2;
        parameters().add(std::move(release));

        parameters().add(nullclap::ParameterSpec::continuous(id.shape, "Shape", module, 0.0, 1.0, 0.5));
    }
}

void GlitchDeckPlugin::registerPorts()
{
    auto input = nullclap::AudioPortSpec::stereo(glitchdeck::ids::audioInput, "Stereo Input", true);
    auto output = nullclap::AudioPortSpec::stereo(glitchdeck::ids::audioOutput, "Stereo Output", true);
    input.inPlacePair = output.id;
    output.inPlacePair = input.id;
    audioPorts().addInput(std::move(input));
    audioPorts().addOutput(std::move(output));

    // Prefer raw MIDI so CC20-27 can reach the performance layer, but also accept
    // native CLAP note events. Bitwig may choose either representation for notes;
    // CC messages still require the raw-MIDI dialect.
    notePorts().addInput(nullclap::NotePortSpec::dialects(
        glitchdeck::ids::midiInput,
        "Performance MIDI",
        CLAP_NOTE_DIALECT_MIDI | CLAP_NOTE_DIALECT_CLAP,
        CLAP_NOTE_DIALECT_MIDI));
}

void GlitchDeckPlugin::registerRemoteControls()
{
    nullclap::RemoteControlPage triggers;
    triggers.id = glitchdeck::ids::performanceRemote;
    triggers.section = "GlitchDeck";
    triggers.name = "Triggers";
    for (int slot = 0; slot < numSlots; ++slot)
        triggers.parameters[static_cast<std::size_t>(slot)] = ids_[static_cast<std::size_t>(slot)].trigger;
    remoteControls().add(std::move(triggers));

    for (int slot = 0; slot < numSlots; ++slot)
    {
        const auto& id = ids_[static_cast<std::size_t>(slot)];
        nullclap::RemoteControlPage page;
        page.id = nullclap::stableId("glitchdeck.remote.slot" + std::to_string(slot + 1));
        page.section = "GlitchDeck";
        page.name = slotModule(slot);
        page.parameters = {
            id.effect, id.intensity, id.length, id.shape,
            id.attack, id.release, id.latch, id.trigger
        };
        remoteControls().add(std::move(page));
    }
}

bool GlitchDeckPlugin::onActivate(double sampleRate, std::uint32_t, std::uint32_t) noexcept
{
    if (!std::isfinite(sampleRate) || sampleRate < 1.0 || sampleRate > 768000.0)
        return false;
    try
    {
        sampleRate_ = sampleRate;
        engine_.prepare(sampleRate_, 2);
        streamSampleCounter_ = 0;
        resetPerformance();
        configsInitialized_ = false;
        blockPrepared_ = false;
        midiActivity_.store(0, std::memory_order_relaxed);
        midiReceived_.store(0, std::memory_order_relaxed);
        midiMatched_.store(0, std::memory_order_relaxed);
        updateEngineConfigs();
        return true;
    }
    catch (...) { return false; }
}

void GlitchDeckPlugin::onReset() noexcept
{
    resetPerformance();
    engine_.reset();
    // Only the consumer advances the read index. Never reset the producer index
    // from the audio thread while the editor might be publishing an edge.
    uiQueueRead_.store(uiQueueWrite_.load(std::memory_order_acquire), std::memory_order_release);
    for (auto& overflow : uiOverflow_) overflow.exchange(0, std::memory_order_acq_rel);
}

bool GlitchDeckPlugin::loadExtraState(std::span<const std::byte> bytes)
{
    if (!bytes.empty()) return false; // v1 has no opaque application payload.
    ++learnGeneration_;
    midiLearnArm_.store(0, std::memory_order_release);
    learnedBinding_.store(0, std::memory_order_release);
    // State loading is a main-thread callback and may overlap DSP. Do not clear
    // history/envelopes here; consume this request at the next block boundary.
    stateResetRequested_.store(true, std::memory_order_release);
    _host.requestProcess();
    return true;
}

void GlitchDeckPlugin::updateEngineConfigs() noexcept
{
    for (int slot = 0; slot < numSlots; ++slot)
    {
        const auto& id = ids_[static_cast<std::size_t>(slot)];
        const auto index = static_cast<std::size_t>(slot);
        const auto binding = static_cast<std::uint32_t>(parameterInt(id.midiType))
            | (static_cast<std::uint32_t>(parameterInt(id.midiNumber)) << 8)
            | (static_cast<std::uint32_t>(parameterInt(id.midiChannel)) << 16);
        const auto effect = parameterInt(id.effect);
        const bool latch = parameterBool(id.latch);
        if (configsInitialized_ && (binding != previousBinding_[index] || effect != previousEffect_[index]
            || latch != previousLatch_[index]))
            stopSlot(slot);
        previousBinding_[index] = binding;
        previousEffect_[index] = effect;
        previousLatch_[index] = latch;
        GlitchEngine::SlotConfig config;
        config.effect = static_cast<GlitchEngine::EffectType>(std::clamp(parameterInt(id.effect), 0, 7));
        config.stereo = static_cast<GlitchEngine::StereoMode>(std::clamp(parameterInt(id.stereo), 0, 3));
        config.intensity = static_cast<float>(parameters().effectiveValue(id.intensity));
        config.lengthMs = static_cast<float>(parameters().effectiveValue(id.length));
        config.attackMs = static_cast<float>(parameters().effectiveValue(id.attack));
        config.releaseMs = static_cast<float>(parameters().effectiveValue(id.release));
        config.shape = static_cast<float>(parameters().effectiveValue(id.shape));
        engine_.setSlotConfig(slot, config);
    }
    configsInitialized_ = true;
}

void GlitchDeckPlugin::processAudio(const clap_process_t& process,
                                    std::uint32_t startFrame,
                                    std::uint32_t endFrame) noexcept
{
    if (startFrame >= endFrame)
        return;

    prepareBlock();
    updateEngineConfigs();

    const auto mix = static_cast<float>(parameters().effectiveValue(glitchdeck::ids::mix));
    const auto* input = process.audio_inputs_count > 0 && process.audio_inputs != nullptr ? &process.audio_inputs[0] : nullptr;
    auto* output = process.audio_outputs_count > 0 && process.audio_outputs != nullptr ? &process.audio_outputs[0] : nullptr;

    if (output != nullptr) output->constant_mask = 0;
    for (std::uint32_t frame = startFrame; frame < endFrame; ++frame)
    {
        executePendingTriggersAt(streamSampleCounter_);

        float left = 0.0f;
        float right = 0.0f;
        if (input != nullptr && input->channel_count > 0)
        {
            if (input->data32 != nullptr)
            {
                left = input->data32[0] != nullptr ? input->data32[0][frame] : 0.0f;
                right = input->channel_count > 1 && input->data32[1] != nullptr ? input->data32[1][frame] : left;
            }
            else if (input->data64 != nullptr)
            {
                left = input->data64[0] != nullptr ? static_cast<float>(input->data64[0][frame]) : 0.0f;
                right = input->channel_count > 1 && input->data64[1] != nullptr
                    ? static_cast<float>(input->data64[1][frame]) : left;
            }
        }

        const auto processed = engine_.processSample(left, right, mix);

        if (output != nullptr && output->channel_count > 0)
        {
            if (output->data32 != nullptr)
            {
                if (output->data32[0] != nullptr)
                    output->data32[0][frame] = processed.left;
                if (output->channel_count > 1 && output->data32[1] != nullptr)
                    output->data32[1][frame] = processed.right;
            }
            else if (output->data64 != nullptr)
            {
                if (output->data64[0] != nullptr)
                    output->data64[0][frame] = static_cast<double>(processed.left);
                if (output->channel_count > 1 && output->data64[1] != nullptr)
                    output->data64[1][frame] = static_cast<double>(processed.right);
            }
        }

        ++streamSampleCounter_;
    }

    for (int slot = 0; slot < numSlots; ++slot)
        visibleActive_[static_cast<std::size_t>(slot)].store(engine_.isSlotActive(slot), std::memory_order_relaxed);
}

void GlitchDeckPlugin::onEvent(const clap_event_header_t& event) noexcept
{
    prepareBlock();
    if (event.space_id != CLAP_CORE_EVENT_SPACE_ID)
        return;

    switch (event.type)
    {
        case CLAP_EVENT_MIDI:
            if (event.size >= sizeof(clap_event_midi_t))
                handleMidiEvent(reinterpret_cast<const clap_event_midi_t&>(event));
            break;
        case CLAP_EVENT_NOTE_ON:
            if (event.size >= sizeof(clap_event_note_t))
                handleNoteEvent(reinterpret_cast<const clap_event_note_t&>(event), true);
            break;
        case CLAP_EVENT_NOTE_OFF:
        case CLAP_EVENT_NOTE_CHOKE:
            if (event.size >= sizeof(clap_event_note_t))
                handleNoteEvent(reinterpret_cast<const clap_event_note_t&>(event), false);
            break;
        case CLAP_EVENT_PARAM_VALUE:
            if (event.size >= sizeof(clap_event_param_value_t))
                handleParameterEvent(reinterpret_cast<const clap_event_param_value_t&>(event));
            break;
        case CLAP_EVENT_TRANSPORT:
            if (event.size >= sizeof(clap_event_transport_t))
                handleTransportEvent(reinterpret_cast<const clap_event_transport_t&>(event));
            break;
        default:
            break;
    }
}

void GlitchDeckPlugin::handleTransportEvent(const clap_event_transport_t& event) noexcept
{
    constexpr auto timingFlags = CLAP_TRANSPORT_IS_PLAYING | CLAP_TRANSPORT_HAS_TEMPO | CLAP_TRANSPORT_HAS_BEATS_TIMELINE;
    bool cancel = (event.flags & timingFlags) != timingFlags || !std::isfinite(event.tempo)
        || !std::isfinite(event.tempo_inc) || event.tempo <= 0.0;
    if (!cancel && hasTransport_ && (transport_.flags & timingFlags) == timingFlags)
    {
        const double elapsed = static_cast<double>(streamSampleCounter_ - transportAnchorSample_);
        const double expected = static_cast<double>(transport_.song_pos_beats) / CLAP_BEATTIME_FACTOR
            + (elapsed * transport_.tempo + 0.5 * elapsed * (elapsed - 1.0) * transport_.tempo_inc) / (60.0 * sampleRate_);
        const double actual = static_cast<double>(event.song_pos_beats) / CLAP_BEATTIME_FACTOR;
        const double tolerance = std::max(1.0e-6, 4.0 * event.tempo / (60.0 * sampleRate_));
        cancel = !std::isfinite(expected) || std::abs(actual - expected) > tolerance;
    }
    transport_ = event;
    hasTransport_ = true;
    transportAnchorSample_ = streamSampleCounter_;
    transportBlockStartSample_ = streamSampleCounter_ - static_cast<std::int64_t>(event.header.time);
    for (int slot = 0; slot < numSlots; ++slot)
    {
        auto& pending = pendingTriggers_[static_cast<std::size_t>(slot)];
        if (!pending.used || !pending.quantized) continue;
        if (cancel) stopSlot(slot);
        else pending.targetSample = quantizedTargetSample(slot, event.header.time, nullptr, &pending.targetBeat);
    }
}

void GlitchDeckPlugin::recordMidiActivity(MidiBindingType type,
                                          int number,
                                          int channel,
                                          int value,
                                          bool down) noexcept
{
    number = std::clamp(number, 0, 127);
    channel = std::clamp(channel, 1, 16);
    value = std::clamp(value, 0, 127);

    std::uint32_t packed = activityValid;
    if (type == MidiBindingType::cc)
        packed |= activityCc;
    if (down)
        packed |= activityDown;
    packed |= static_cast<std::uint32_t>(channel - 1) << activityChannelShift;
    packed |= static_cast<std::uint32_t>(number) << activityNumberShift;
    packed |= static_cast<std::uint32_t>(value) << activityValueShift;
    midiActivity_.store(packed, std::memory_order_release);
}

bool GlitchDeckPlugin::captureMidiLearn(MidiBindingType type, int number, int channel) noexcept
{
    auto arm = midiLearnArm_.load(std::memory_order_acquire);
    const auto slot = static_cast<int>(arm & 0xffu) - 1;
    if (slot < 0 || slot >= numSlots || number < 0 || number > 127 || channel < 1 || channel > 16)
        return false;
    if (type == MidiBindingType::cc && number >= 120) return false; // Channel-mode messages are not pads.
    if (!midiLearnArm_.compare_exchange_strong(arm, 0, std::memory_order_acq_rel)) return false;
    const auto payload = (arm & 0xffffffff00000000ULL)
        | (static_cast<std::uint64_t>(slot + 1) << 24)
        | (static_cast<std::uint64_t>(type) << 23)
        | (static_cast<std::uint64_t>(channel) << 8) | static_cast<unsigned>(number);
    learnedBinding_.store(payload, std::memory_order_release);
    requestMainService();
    return true;
}

void GlitchDeckPlugin::handleMidiEvent(const clap_event_midi_t& event) noexcept
{
    if (event.port_index != 0 || event.data[1] > 127 || event.data[2] > 127) return;
    const auto command = event.data[0] & 0xf0u;
    const int channel = (event.data[0] & 0x0fu) + 1;
    const int number = event.data[1], value = event.data[2];
    if (command != 0xb0u && command != 0x90u && command != 0x80u) return;
    const bool cc = command == 0xb0u;
    const bool down = cc ? value >= 64 : command == 0x90u && value > 0;
    recordMidiActivity(cc ? MidiBindingType::cc : MidiBindingType::note, number, channel, value, down);
    midiReceived_.fetch_add(1, std::memory_order_relaxed);
    if (cc && (number == 120 || number == 123 || number == 121))
    {
        releaseMidiChannel(channel, number == 121);
        return;
    }
    if (cc && number >= 120) return;
    if (down && captureMidiLearn(cc ? MidiBindingType::cc : MidiBindingType::note, number, channel)) return;
    if (!cc)
    {
        updateNoteInput(true, channel, number, -1, down, false, event.header.time);
        return;
    }
    for (int slot = 0; slot < numSlots; ++slot)
    {
        const auto& id = ids_[static_cast<std::size_t>(slot)];
        const int configured = parameterInt(id.midiChannel);
        if (parameterInt(id.midiType) != 1 || parameterInt(id.midiNumber) != number
            || (configured != 0 && configured != channel)) continue;
        midiMatched_.fetch_add(1, std::memory_order_relaxed);
        const auto bit = static_cast<std::uint16_t>(1u << (channel - 1));
        auto& held = heldCc_[static_cast<std::size_t>(slot)];
        held = down ? static_cast<std::uint16_t>(held | bit) : static_cast<std::uint16_t>(held & ~bit);
        updateInputGate(slot, event.header.time);
    }
}

void GlitchDeckPlugin::handleNoteEvent(const clap_event_note_t& event, bool down) noexcept
{
    if (event.port_index < -1 || event.port_index > 0 || event.key < -1 || event.key > 127
        || event.channel < -1 || event.channel > 15 || event.note_id < -1) return;
    if (down && (event.port_index != 0 || event.key < 0 || event.channel < 0
        || !std::isfinite(event.velocity) || event.velocity < 0.0 || event.velocity > 1.0)) return;
    const int channel = event.channel < 0 ? -1 : event.channel + 1;
    const int velocity = down ? static_cast<int>(std::lround(event.velocity * 127.0)) : 0;
    recordMidiActivity(MidiBindingType::note, event.key, channel, velocity, down);
    midiReceived_.fetch_add(1, std::memory_order_relaxed);
    // Native CLAP note-on velocity zero is still a note-on, unlike raw MIDI.
    if (down && captureMidiLearn(MidiBindingType::note, event.key, channel)) return;
    updateNoteInput(false, channel, event.key, event.note_id, down,
                    event.header.type == CLAP_EVENT_NOTE_CHOKE, event.header.time);
}

void GlitchDeckPlugin::handleParameterEvent(const clap_event_param_value_t& event) noexcept
{
    if (event.note_id != -1 || event.port_index != -1 || event.channel != -1 || event.key != -1
        || !std::isfinite(event.value)) return;
    updateEngineConfigs();
    for (int slot = 0; slot < numSlots; ++slot)
    {
        if (event.param_id != ids_[static_cast<std::size_t>(slot)].trigger) continue;
        lastAutomationDown_[static_cast<std::size_t>(slot)] = parameters().value(event.param_id) >= 0.5;
        updateInputGate(slot, event.header.time);
        break;
    }
}

void GlitchDeckPlugin::scheduleTrigger(int slot, bool down, std::uint32_t eventTime) noexcept
{
    if (slot < 0 || slot >= numSlots) return;
    const auto i = static_cast<std::size_t>(slot);
    const bool latch = parameterBool(ids_[i].latch);
    if (latch && !down) return;
    desiredGate_[i] = latch ? !desiredGate_[i] : down;
    if (!desiredGate_[i])
    {
        pendingTriggers_[i].used = false;
        visiblePending_[i].store(false, std::memory_order_relaxed);
        engine_.trigger(slot, false);
        visibleActive_[i].store(false, std::memory_order_relaxed);
        return;
    }
    double targetBeat = 0.0;
    const auto target = quantizedTargetSample(slot, eventTime, &targetBeat);
    pendingTriggers_[i] = { true, target, target > streamSampleCounter_, targetBeat };
    visiblePending_[i].store(pendingTriggers_[i].quantized, std::memory_order_relaxed);
}

/* Each slot owns exactly one pending onset; release cancels it directly. */

void GlitchDeckPlugin::executePendingTriggersAt(std::int64_t absoluteSample) noexcept
{
    for (int slot = 0; slot < numSlots; ++slot)
    {
        auto& pending = pendingTriggers_[static_cast<std::size_t>(slot)];
        if (!pending.used || pending.targetSample > absoluteSample) continue;
        pending.used = false;
        engine_.trigger(slot, true);
        visiblePending_[static_cast<std::size_t>(slot)].store(false, std::memory_order_relaxed);
    }
}

/* Gate-on is committed immediately before its first audio sample. */

std::int64_t GlitchDeckPlugin::quantizedTargetSample(int slot, std::uint32_t eventTime,
    double* targetBeatOut, const double* requestedBeat) const noexcept
{
    const auto quantizeIndex = std::clamp(parameterInt(ids_[static_cast<std::size_t>(slot)].quantize), 0, 5);
    if (quantizeIndex == 0)
        return streamSampleCounter_;

    const auto blockStartSample = streamSampleCounter_ - static_cast<std::int64_t>(eventTime);
    const clap_event_transport_t* source = nullptr;
    std::int64_t anchorSample = 0;

    if (hasTransport_ && transportBlockStartSample_ == blockStartSample)
    {
        source = &transport_;
        anchorSample = transportAnchorSample_;
    }
    else if (const auto* process = currentProcess(); process != nullptr && process->transport != nullptr)
    {
        source = process->transport;
        anchorSample = blockStartSample;
    }
    else if (hasTransport_)
    {
        source = &transport_;
        anchorSample = transportAnchorSample_;
    }

    if (source == nullptr
        || (source->flags & CLAP_TRANSPORT_IS_PLAYING) == 0
        || (source->flags & CLAP_TRANSPORT_HAS_TEMPO) == 0
        || (source->flags & CLAP_TRANSPORT_HAS_BEATS_TIMELINE) == 0
        || !std::isfinite(source->tempo) || !std::isfinite(source->tempo_inc)
        || source->tempo <= 0.0)
        return streamSampleCounter_;

    const auto elapsedSamples = static_cast<double>(std::max<std::int64_t>(0, streamSampleCounter_ - anchorSample));
    const auto beatAtAnchor = static_cast<double>(source->song_pos_beats) / static_cast<double>(CLAP_BEATTIME_FACTOR);
    const auto beatDelta = (source->tempo * elapsedSamples
        + 0.5 * source->tempo_inc * elapsedSamples * (elapsedSamples - 1.0)) / (60.0 * sampleRate_);
    const auto eventBeat = beatAtAnchor + beatDelta;

    double targetBeat = eventBeat;
    if (requestedBeat != nullptr)
        targetBeat = *requestedBeat; // Keep the onset chosen at the press, even if this block crossed its grid.
    else if (quantizeIndex == 5)
    {
        double barLengthBeats = 4.0;
        double barStartBeat = 0.0;
        if ((source->flags & CLAP_TRANSPORT_HAS_TIME_SIGNATURE) != 0
            && source->tsig_num > 0
            && source->tsig_denom > 0)
        {
            barLengthBeats = static_cast<double>(source->tsig_num) * 4.0
                / static_cast<double>(source->tsig_denom);
            barStartBeat = static_cast<double>(source->bar_start) / static_cast<double>(CLAP_BEATTIME_FACTOR);
        }
        barLengthBeats = std::max(1.0e-6, barLengthBeats);
        targetBeat = barStartBeat
            + std::ceil((eventBeat - barStartBeat - 1.0e-9) / barLengthBeats) * barLengthBeats;
    }
    else
    {
        constexpr std::array<double, 5> grids { 0.0, 0.125, 0.25, 0.5, 1.0 };
        const auto grid = grids[static_cast<std::size_t>(quantizeIndex)];
        targetBeat = std::ceil((eventBeat - 1.0e-9) / grid) * grid;
    }

    if (targetBeatOut != nullptr) *targetBeatOut = targetBeat;
    const auto tempoNow = source->tempo + source->tempo_inc * elapsedSamples;
    const auto distance = std::max(0.0, targetBeat - eventBeat) * 60.0 * sampleRate_;
    if (!std::isfinite(distance) || !std::isfinite(tempoNow) || tempoNow <= 0.0)
        return streamSampleCounter_;
    // Solve the discrete tempo ramp: n*T + n*(n-1)*inc/2 = beat distance.
    // The stable root avoids cancellation for small tempo increments.
    const auto b = tempoNow - 0.5 * source->tempo_inc;
    const auto discriminant = b * b + 2.0 * source->tempo_inc * distance;
    double samples = distance / tempoNow;
    if (source->tempo_inc != 0.0 && discriminant >= 0.0 && b + std::sqrt(discriminant) > 0.0)
        samples = 2.0 * distance / (b + std::sqrt(discriminant));
    if (!std::isfinite(samples) || samples < 0.0 || samples > sampleRate_ * 60.0)
        return streamSampleCounter_;
    // Never fire before the grid due to rounding a fractional sample down.
    return streamSampleCounter_ + static_cast<std::int64_t>(std::ceil(samples - 1.0e-8));
}

bool GlitchDeckPlugin::pushUiTrigger(const UiTriggerEvent& event) noexcept
{
    const auto write = uiQueueWrite_.load(std::memory_order_relaxed);
    const auto next = (write + 1) % uiQueueCapacity;
    if (next == uiQueueRead_.load(std::memory_order_acquire))
        return false;
    uiTriggerQueue_[write] = event;
    uiQueueWrite_.store(next, std::memory_order_release);
    return true;
}

bool GlitchDeckPlugin::popUiTrigger(UiTriggerEvent& event) noexcept
{
    const auto read = uiQueueRead_.load(std::memory_order_relaxed);
    if (read == uiQueueWrite_.load(std::memory_order_acquire))
        return false;
    event = uiTriggerQueue_[read];
    uiQueueRead_.store((read + 1) % uiQueueCapacity, std::memory_order_release);
    return true;
}

void GlitchDeckPlugin::drainUiTriggers(std::uint32_t eventTime) noexcept
{
    const auto read = uiQueueRead_.load(std::memory_order_relaxed);
    const auto write = uiQueueWrite_.load(std::memory_order_acquire);
    const auto count = (write + uiQueueCapacity - read) % uiQueueCapacity;
    UiTriggerEvent event;
    for (std::size_t n = 0; n < count && popUiTrigger(event); ++n) applyUiTrigger(event, eventTime);
    for (int slot = 0; slot < numSlots; ++slot)
    {
        const auto packed = uiOverflow_[static_cast<std::size_t>(slot)].exchange(0, std::memory_order_acq_rel);
        if (packed != 0) applyUiTrigger({ slot, (packed & 1u) != 0, packed >> 1 }, eventTime);
    }
}

void GlitchDeckPlugin::setTriggerFromUi(int slot, bool down, UiSource source) noexcept
{
    if (slot < 0 || slot >= numSlots) return;
    auto& sources = uiSources_[static_cast<std::size_t>(slot)];
    const bool previous = sources != 0;
    if (source != UiSource::mouse && source != UiSource::keyboard) return;
    const auto bit = static_cast<std::uint8_t>(1u << static_cast<unsigned>(source));
    sources = down ? static_cast<std::uint8_t>(sources | bit) : static_cast<std::uint8_t>(sources & ~bit);
    const bool held = sources != 0;
    if (held == previous) return;
    const auto sequence = ++uiSequence_;
    if (!pushUiTrigger({ slot, held, sequence }))
    {
        uiOverflow_[static_cast<std::size_t>(slot)].store((sequence << 1) | (held ? 1u : 0u), std::memory_order_release);
        overflowCount_.fetch_add(1, std::memory_order_relaxed);
    }
    const auto id = ids_[static_cast<std::size_t>(slot)].trigger;
    if (held) beginUiEdit(id);
    setUiValue(id, held ? 1.0 : 0.0);
    if (!held) endUiEdit(id);
    _host.requestProcess();
}

bool GlitchDeckPlugin::isSlotActive(int slot) const noexcept
{
    return slot >= 0 && slot < numSlots
        && visibleActive_[static_cast<std::size_t>(slot)].load(std::memory_order_relaxed);
}

void GlitchDeckPlugin::toggleMidiLearn(int slot) noexcept
{
    if (slot < 0 || slot >= numSlots) return;
    const bool cancel = isMidiLearning(slot);
    ++learnGeneration_;
    learnedBinding_.store(0, std::memory_order_release);
    const auto arm = cancel ? 0 : (static_cast<std::uint64_t>(learnGeneration_) << 32) | static_cast<unsigned>(slot + 1);
    midiLearnArm_.store(arm, std::memory_order_release);
}

bool GlitchDeckPlugin::isMidiLearning(int slot) const noexcept
{
    return slot >= 0 && slot < numSlots
        && static_cast<int>(midiLearnArm_.load(std::memory_order_acquire) & 0xffu) == slot + 1;
}

void GlitchDeckPlugin::applyPendingMidiLearnFromUi() noexcept
{
    const auto binding = learnedBinding_.exchange(0, std::memory_order_acq_rel);
    const int slot = static_cast<int>((binding >> 24) & 0xffu) - 1;
    if (slot >= 0 && slot < numSlots && static_cast<std::uint32_t>(binding >> 32) == learnGeneration_)
    {
        const auto& id = ids_[static_cast<std::size_t>(slot)];
        setUiValueOnce(id.midiType, static_cast<double>((binding >> 23) & 1u));
        setUiValueOnce(id.midiNumber, static_cast<double>(binding & 0x7fu));
        setUiValueOnce(id.midiChannel, static_cast<double>((binding >> 8) & 0x1fu));
    }
    flushUiEdits();
}

int GlitchDeckPlugin::effectIndex(int slot) const noexcept
{
    if (slot < 0 || slot >= numSlots)
        return 0;
    return std::clamp(parameterInt(ids_[static_cast<std::size_t>(slot)].effect), 0, 7);
}

std::string GlitchDeckPlugin::effectName(int slot) const
{
    return effectNames()[static_cast<std::size_t>(effectIndex(slot))];
}

std::string GlitchDeckPlugin::midiBindingText(int slot) const
{
    if (slot < 0 || slot >= numSlots)
        return "UNBOUND";

    const auto& id = ids_[static_cast<std::size_t>(slot)];
    const auto type = std::clamp(parameterInt(id.midiType), 0, 1);
    const auto number = std::clamp(parameterInt(id.midiNumber), 0, 127);
    const auto channel = std::clamp(parameterInt(id.midiChannel), 0, 16);

    std::string result = type == static_cast<int>(MidiBindingType::cc)
        ? "CC " + std::to_string(number)
        : midiNoteName(number) + " (" + std::to_string(number) + ")";
    result += channel == 0 ? "  ·  OMNI" : "  ·  CH " + std::to_string(channel);
    return result;
}

std::string GlitchDeckPlugin::midiActivityText() const
{
    const auto packed = midiActivity_.load(std::memory_order_acquire);
    if ((packed & activityValid) == 0)
        return "NO HOST MIDI | Use the upstream MIDI CC device to test delivery";

    const auto isCc = (packed & activityCc) != 0;
    const auto down = (packed & activityDown) != 0;
    const auto channel = static_cast<int>((packed >> activityChannelShift) & 0x0Fu) + 1;
    const auto number = static_cast<int>((packed >> activityNumberShift) & 0x7Fu);
    const auto value = static_cast<int>((packed >> activityValueShift) & 0x7Fu);

    const auto counts = " | IN " + std::to_string(midiReceived_.load(std::memory_order_relaxed))
        + " / MATCH " + std::to_string(midiMatched_.load(std::memory_order_relaxed));
    if (isCc)
        return "CC" + std::to_string(number) + " CH" + std::to_string(channel) + " " + std::to_string(value) + counts;

    return midiNoteName(number) + " CH" + std::to_string(channel) + (down ? " ON" : " OFF") + counts;
}

void GlitchDeckPlugin::resetPerformance() noexcept
{
    heldNotes_ = {};
    for (int slot = 0; slot < numSlots; ++slot) stopSlot(slot);
    hasTransport_ = false;
    transportAnchorSample_ = streamSampleCounter_;
    transportBlockStartSample_ = -1;
}

void GlitchDeckPlugin::stopSlot(int slot) noexcept
{
    const auto i = static_cast<std::size_t>(slot);
    heldUi_[i] = lastAutomationDown_[i] = inputDown_[i] = desiredGate_[i] = false;
    heldCc_[i] = 0;
    for (auto& note : heldNotes_)
    {
        note.slots &= static_cast<std::uint8_t>(~(1u << slot));
        if (note.slots == 0) note.used = false;
    }
    pendingTriggers_[i].used = false;
    engine_.trigger(slot, false);
    visibleActive_[i].store(false, std::memory_order_relaxed);
    visiblePending_[i].store(false, std::memory_order_relaxed);
}

void GlitchDeckPlugin::prepareBlock() noexcept
{
    const auto* process = currentProcess();
    if (process == nullptr || blockPrepared_) return;
    blockPrepared_ = true;
    if (stateResetRequested_.exchange(false, std::memory_order_acq_rel)) onReset();
    if (panicRequested_.exchange(false, std::memory_order_acq_rel))
    {
        resetPerformance(); // Release envelopes, rather than clearing audio abruptly.
        uiQueueRead_.store(uiQueueWrite_.load(std::memory_order_acquire), std::memory_order_release);
        for (auto& overflow : uiOverflow_) overflow.exchange(0, std::memory_order_acq_rel);
    }
    if (process->transport != nullptr) handleTransportEvent(*process->transport);
    else
    {
        hasTransport_ = false;
        for (int slot = 0; slot < numSlots; ++slot)
            if (pendingTriggers_[static_cast<std::size_t>(slot)].used
                && pendingTriggers_[static_cast<std::size_t>(slot)].quantized) stopSlot(slot);
    }
    updateEngineConfigs();
    drainUiTriggers(0);
}

clap_process_status GlitchDeckPlugin::processFinished() noexcept
{
    blockPrepared_ = false;
    return CLAP_PROCESS_CONTINUE;
}

void GlitchDeckPlugin::updateInputGate(int slot, std::uint32_t eventTime) noexcept
{
    const auto i = static_cast<std::size_t>(slot);
    bool held = heldUi_[i] || lastAutomationDown_[i] || heldCc_[i] != 0;
    if (!held)
        for (const auto& note : heldNotes_)
            held |= note.used && (note.slots & (1u << slot)) != 0;
    if (held == inputDown_[i]) return;
    inputDown_[i] = held;
    scheduleTrigger(slot, held, eventTime);
}

void GlitchDeckPlugin::updateNoteInput(bool raw, int channel, int key, std::int32_t noteId,
                                     bool down, bool choke, std::uint32_t eventTime) noexcept
{
    std::uint8_t affected = 0;
    if (down)
    {
        for (int slot = 0; slot < numSlots; ++slot)
        {
            const auto& id = ids_[static_cast<std::size_t>(slot)];
            const int configured = parameterInt(id.midiChannel);
            if (parameterInt(id.midiType) == 0 && parameterInt(id.midiNumber) == key
                && (configured == 0 || configured == channel)) affected |= static_cast<std::uint8_t>(1u << slot);
        }
        if (affected == 0) return;
        for (const auto& note : heldNotes_)
            if (note.used && note.rawMidi == raw && note.channel == channel && note.key == key && note.noteId == noteId)
                return; // Duplicate held notes do not toggle a latch repeatedly.
        auto free = std::find_if(heldNotes_.begin(), heldNotes_.end(), [](const auto& note) { return !note.used; });
        if (free == heldNotes_.end()) return; // Bounded voice tracking; never evict a release owner.
        *free = { true, raw, channel, key, noteId, affected };
    }
    else
    {
        for (auto& note : heldNotes_)
        {
            if (!note.used || note.rawMidi != raw || (channel != -1 && note.channel != channel)
                || (key != -1 && note.key != key) || (noteId != -1 && note.noteId != noteId)) continue;
            affected |= note.slots;
            note.used = false;
        }
        // A wildcard choke is also the host's emergency stop for latched notes
        // whose key was already released. Unlike note-off, it cancels the latch.
        if (choke && noteId == -1)
        {
            for (int slot = 0; slot < numSlots; ++slot)
            {
                const auto& id = ids_[static_cast<std::size_t>(slot)];
                const int configured = parameterInt(id.midiChannel);
                if ((key == -1 || (parameterInt(id.midiType) == 0 && parameterInt(id.midiNumber) == key))
                    && (channel == -1 || configured == 0 || configured == channel))
                    affected |= static_cast<std::uint8_t>(1u << slot);
            }
        }
    }
    if (affected != 0) midiMatched_.fetch_add(1, std::memory_order_relaxed);
    for (int slot = 0; slot < numSlots; ++slot)
    {
        if ((affected & (1u << slot)) == 0) continue;
        bool remainingNote = false;
        for (const auto& note : heldNotes_) remainingNote |= note.used && (note.slots & (1u << slot)) != 0;
        const auto i = static_cast<std::size_t>(slot);
        if (choke && ((key == -1 && channel == -1 && noteId == -1)
            || (!remainingNote && heldCc_[i] == 0 && !heldUi_[i] && !lastAutomationDown_[i]))) stopSlot(slot);
        else updateInputGate(slot, eventTime);
    }
}

void GlitchDeckPlugin::releaseMidiChannel(int channel, bool controllersOnly) noexcept
{
    if (!controllersOnly)
        for (auto& note : heldNotes_)
            if (note.used && note.channel == channel) note.used = false;
    for (int slot = 0; slot < numSlots; ++slot)
    {
        const auto i = static_cast<std::size_t>(slot);
        heldCc_[i] &= static_cast<std::uint16_t>(~(1u << (channel - 1)));
        bool midiHeld = heldCc_[i] != 0;
        for (const auto& note : heldNotes_) midiHeld |= note.used && (note.slots & (1u << slot)) != 0;
        const int configured = parameterInt(ids_[i].midiChannel);
        if (!midiHeld && !heldUi_[i] && !lastAutomationDown_[i]
            && (configured == 0 || configured == channel)
            && (!controllersOnly || parameterInt(ids_[i].midiType) == 1)) stopSlot(slot);
        else updateInputGate(slot, 0);
    }
}

void GlitchDeckPlugin::applyUiTrigger(const UiTriggerEvent& event, std::uint32_t eventTime) noexcept
{
    if (event.slot < 0 || event.slot >= numSlots) return;
    const auto i = static_cast<std::size_t>(event.slot);
    if (event.sequence <= lastUiSequence_[i]) return;
    lastUiSequence_[i] = event.sequence;
    heldUi_[i] = event.down;
    updateInputGate(event.slot, eventTime);
}

void GlitchDeckPlugin::releaseUiTriggers() noexcept
{
    for (int slot = 0; slot < numSlots; ++slot)
    {
        setTriggerFromUi(slot, false, UiSource::mouse);
        setTriggerFromUi(slot, false, UiSource::keyboard);
    }
}

void GlitchDeckPlugin::panicFromUi() noexcept
{
    releaseUiTriggers();
    for (const auto& id : ids_) setUiValueOnce(id.trigger, 0.0);
    ++learnGeneration_;
    midiLearnArm_.store(0, std::memory_order_release);
    learnedBinding_.store(0, std::memory_order_release);
    panicRequested_.store(true, std::memory_order_release);
    _host.requestProcess();
}

bool GlitchDeckPlugin::isSlotPending(int slot) const noexcept
{
    return slot >= 0 && slot < numSlots && visiblePending_[static_cast<std::size_t>(slot)].load(std::memory_order_relaxed);
}

GlitchDeckPlugin::UiEdit* GlitchDeckPlugin::uiEdit(clap_id id) noexcept
{
    if (!parameters().contains(id) || parameters().isReadOnly(id)) return nullptr;
    for (auto& edit : uiEdits_) if (edit.id == id) return &edit;
    for (auto& edit : uiEdits_)
        if (edit.id == CLAP_INVALID_ID) { edit.id = id; return &edit; }
    return nullptr;
}

void GlitchDeckPlugin::beginUiEdit(clap_id id) noexcept
{
    if (auto* edit = uiEdit(id)) { edit->active = true; edit->end = false; }
    flushUiEdits();
}

void GlitchDeckPlugin::setUiValue(clap_id id, double value) noexcept
{
    if (!std::isfinite(value)) return;
    if (auto* edit = uiEdit(id))
    {
        edit->active = edit->hasValue = true;
        edit->value = value;
    }
    flushUiEdits();
}

void GlitchDeckPlugin::endUiEdit(clap_id id) noexcept
{
    if (auto* edit = uiEdit(id)) edit->end = true;
    flushUiEdits();
}

void GlitchDeckPlugin::setUiValueOnce(clap_id id, double value) noexcept
{
    beginUiEdit(id);
    setUiValue(id, value);
    endUiEdit(id);
}

void GlitchDeckPlugin::flushUiEdits() noexcept
{
    for (auto& edit : uiEdits_)
    {
        if (!edit.active) continue;
        if (!edit.began)
        {
            if (!beginParameterGesture(edit.id)) { requestMainService(); return; }
            edit.began = true;
        }
        if (edit.hasValue)
        {
            if (!setParameterFromGui(edit.id, edit.value)) { requestMainService(); return; }
            edit.hasValue = false;
            markStateDirty();
        }
        if (edit.end)
        {
            if (!endParameterGesture(edit.id)) { requestMainService(); return; }
            edit.active = edit.began = false;
        }
    }
}

void GlitchDeckPlugin::requestMainService() noexcept
{
    if (!mainServiceRequested_.exchange(true, std::memory_order_acq_rel)) _host.requestCallback();
}

void GlitchDeckPlugin::onMainThreadCallback() noexcept
{
    mainServiceRequested_.store(false, std::memory_order_release);
    applyPendingMidiLearnFromUi();
}
