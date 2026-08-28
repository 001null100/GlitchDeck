#include "GlitchDeckPlugin.hpp"
#include "JuceGuiDelegate.hpp"

#include <clap/events.h>
#include <clap/fixedpoint.h>
#include <clap/plugin-features.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <memory>
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
        "0.2.0",
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
    setGuiDelegate(std::make_unique<JuceGuiDelegate>(*this));
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
    mix.unit = "%";
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

    notePorts().addInput(nullclap::NotePortSpec::midi(glitchdeck::ids::midiInput, "Performance MIDI"));
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
    sampleRate_ = std::max(1.0, sampleRate);
    streamSampleCounter_ = 0;
    pendingTriggers_ = {};
    lastAutomationDown_ = {};
    lastCcDown_ = {};
    uiQueueRead_.store(0, std::memory_order_relaxed);
    uiQueueWrite_.store(0, std::memory_order_relaxed);
    hasTransport_ = false;
    transportAnchorSample_ = 0;
    transportBlockStartSample_ = -1;
    engine_.prepare(sampleRate_, 2);
    updateEngineConfigs();
    for (auto& active : visibleActive_)
        active.store(false, std::memory_order_relaxed);
    return true;
}

void GlitchDeckPlugin::onReset() noexcept
{
    pendingTriggers_ = {};
    lastAutomationDown_ = {};
    lastCcDown_ = {};
    engine_.reset();
    for (auto& active : visibleActive_)
        active.store(false, std::memory_order_relaxed);
}

bool GlitchDeckPlugin::loadExtraState(std::span<const std::byte>)
{
    onReset();
    midiLearnSlot_.store(-1, std::memory_order_relaxed);
    learnedSlot_.store(-1, std::memory_order_relaxed);
    updateEngineConfigs();
    return true;
}

void GlitchDeckPlugin::updateEngineConfigs() noexcept
{
    for (int slot = 0; slot < numSlots; ++slot)
    {
        const auto& id = ids_[static_cast<std::size_t>(slot)];
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
}

void GlitchDeckPlugin::processAudio(const clap_process_t& process,
                                    std::uint32_t startFrame,
                                    std::uint32_t endFrame) noexcept
{
    if (startFrame >= endFrame)
        return;

    const auto blockStartSample = streamSampleCounter_ - static_cast<std::int64_t>(startFrame);
    if (startFrame == 0 && process.transport != nullptr && transportBlockStartSample_ != blockStartSample)
    {
        transport_ = *process.transport;
        hasTransport_ = true;
        transportAnchorSample_ = blockStartSample;
        transportBlockStartSample_ = blockStartSample;
    }

    updateEngineConfigs();
    drainUiTriggers();

    const auto mix = static_cast<float>(parameters().effectiveValue(glitchdeck::ids::mix));
    const auto* input = process.audio_inputs_count > 0 ? &process.audio_inputs[0] : nullptr;
    auto* output = process.audio_outputs_count > 0 ? &process.audio_outputs[0] : nullptr;

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
    if (event.space_id != CLAP_CORE_EVENT_SPACE_ID)
        return;

    switch (event.type)
    {
        case CLAP_EVENT_MIDI:
            if (event.size >= sizeof(clap_event_midi_t))
                handleMidiEvent(reinterpret_cast<const clap_event_midi_t&>(event));
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
    transport_ = event;
    hasTransport_ = true;
    transportAnchorSample_ = streamSampleCounter_;
    transportBlockStartSample_ = streamSampleCounter_ - static_cast<std::int64_t>(event.header.time);
}

bool GlitchDeckPlugin::tryCaptureMidiLearn(std::uint8_t status, std::uint8_t data1, std::uint8_t data2) noexcept
{
    const auto slot = midiLearnSlot_.load(std::memory_order_acquire);
    if (slot < 0 || slot >= numSlots)
        return false;

    const auto command = static_cast<std::uint8_t>(status & 0xF0u);
    int type = -1;
    int number = 0;

    if (command == 0x90u && data2 > 0)
    {
        type = static_cast<int>(MidiBindingType::note);
        number = data1;
    }
    else if (command == 0xB0u && data2 >= 64)
    {
        type = static_cast<int>(MidiBindingType::cc);
        number = data1;
    }

    if (type < 0)
        return false;

    learnedType_.store(type, std::memory_order_relaxed);
    learnedNumber_.store(number, std::memory_order_relaxed);
    learnedChannel_.store(static_cast<int>(status & 0x0Fu) + 1, std::memory_order_relaxed);
    learnedSlot_.store(slot, std::memory_order_release);
    midiLearnSlot_.store(-1, std::memory_order_release);
    return true;
}

void GlitchDeckPlugin::handleMidiEvent(const clap_event_midi_t& event) noexcept
{
    const auto status = event.data[0];
    const auto data1 = event.data[1];
    const auto data2 = event.data[2];
    if (tryCaptureMidiLearn(status, data1, data2))
        return;

    const auto command = static_cast<std::uint8_t>(status & 0xF0u);
    const auto channel = static_cast<int>(status & 0x0Fu) + 1;

    for (int slot = 0; slot < numSlots; ++slot)
    {
        const auto& id = ids_[static_cast<std::size_t>(slot)];
        const auto configuredChannel = std::clamp(parameterInt(id.midiChannel), 0, 16);
        if (configuredChannel != 0 && configuredChannel != channel)
            continue;

        const auto bindingType = std::clamp(parameterInt(id.midiType), 0, 1);
        const auto number = std::clamp(parameterInt(id.midiNumber), 0, 127);

        if (bindingType == static_cast<int>(MidiBindingType::note))
        {
            if (data1 != number)
                continue;
            if (command == 0x90u)
                scheduleTrigger(slot, data2 > 0, event.header.time);
            else if (command == 0x80u)
                scheduleTrigger(slot, false, event.header.time);
        }
        else if (command == 0xB0u && data1 == number)
        {
            const auto down = data2 >= 64;
            auto& previous = lastCcDown_[static_cast<std::size_t>(slot)];
            if (down != previous)
            {
                previous = down;
                scheduleTrigger(slot, down, event.header.time);
            }
        }
    }
}

void GlitchDeckPlugin::handleParameterEvent(const clap_event_param_value_t& event) noexcept
{
    for (int slot = 0; slot < numSlots; ++slot)
    {
        if (event.param_id != ids_[static_cast<std::size_t>(slot)].trigger)
            continue;

        const auto down = event.value >= 0.5;
        auto& previous = lastAutomationDown_[static_cast<std::size_t>(slot)];
        if (down != previous)
        {
            previous = down;
            scheduleTrigger(slot, down, event.header.time);
        }
        break;
    }
}

void GlitchDeckPlugin::scheduleTrigger(int slot, bool down, std::uint32_t eventTime) noexcept
{
    if (slot < 0 || slot >= numSlots)
        return;

    if (!down && parameterBool(ids_[static_cast<std::size_t>(slot)].latch))
        return;

    if (!down)
        cancelPendingOnset(slot);

    const auto target = down ? quantizedTargetSample(slot, eventTime) : streamSampleCounter_;
    for (auto& pending : pendingTriggers_)
    {
        if (!pending.used)
        {
            pending = { true, slot, down, target };
            return;
        }
    }

    applyTriggerNow(slot, down);
}

void GlitchDeckPlugin::cancelPendingOnset(int slot) noexcept
{
    for (auto& pending : pendingTriggers_)
        if (pending.used && pending.slot == slot && pending.down)
            pending.used = false;
}

void GlitchDeckPlugin::executePendingTriggersAt(std::int64_t absoluteSample) noexcept
{
    for (auto& pending : pendingTriggers_)
    {
        if (pending.used && pending.targetSample <= absoluteSample)
        {
            applyTriggerNow(pending.slot, pending.down);
            pending.used = false;
        }
    }
}

void GlitchDeckPlugin::applyTriggerNow(int slot, bool down) noexcept
{
    if (parameterBool(ids_[static_cast<std::size_t>(slot)].latch))
    {
        if (!down)
            return;
        engine_.trigger(slot, !engine_.isSlotActive(slot));
    }
    else
    {
        engine_.trigger(slot, down);
    }
    visibleActive_[static_cast<std::size_t>(slot)].store(engine_.isSlotActive(slot), std::memory_order_relaxed);
}

std::int64_t GlitchDeckPlugin::quantizedTargetSample(int slot, std::uint32_t eventTime) const noexcept
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
        || source->tempo <= 0.0)
        return streamSampleCounter_;

    const auto elapsedSamples = static_cast<double>(std::max<std::int64_t>(0, streamSampleCounter_ - anchorSample));
    const auto beatAtAnchor = static_cast<double>(source->song_pos_beats) / static_cast<double>(CLAP_BEATTIME_FACTOR);
    const auto beatDelta = (source->tempo * elapsedSamples
        + 0.5 * source->tempo_inc * elapsedSamples * elapsedSamples) / (60.0 * sampleRate_);
    const auto eventBeat = beatAtAnchor + beatDelta;

    double targetBeat = eventBeat;
    if (quantizeIndex == 5)
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

    const auto tempoNow = std::max(1.0e-6, source->tempo + source->tempo_inc * elapsedSamples);
    const auto deltaSamples = static_cast<std::int64_t>(
        std::llround((targetBeat - eventBeat) * 60.0 * sampleRate_ / tempoNow));
    return std::max(streamSampleCounter_, streamSampleCounter_ + deltaSamples);
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

void GlitchDeckPlugin::drainUiTriggers() noexcept
{
    UiTriggerEvent event;
    while (popUiTrigger(event))
    {
        if (event.slot < 0 || event.slot >= numSlots)
            continue;
        lastAutomationDown_[static_cast<std::size_t>(event.slot)] = event.down;
        scheduleTrigger(event.slot, event.down, 0);
    }
}

void GlitchDeckPlugin::setTriggerFromUi(int slot, bool down) noexcept
{
    if (slot < 0 || slot >= numSlots)
        return;

    pushUiTrigger({ slot, down });
    const auto id = ids_[static_cast<std::size_t>(slot)].trigger;
    if (down)
        beginParameterGesture(id);
    setParameterFromGui(id, down ? 1.0 : 0.0);
    if (!down)
        endParameterGesture(id);
}

bool GlitchDeckPlugin::isSlotActive(int slot) const noexcept
{
    return slot >= 0 && slot < numSlots
        && visibleActive_[static_cast<std::size_t>(slot)].load(std::memory_order_relaxed);
}

void GlitchDeckPlugin::toggleMidiLearn(int slot) noexcept
{
    if (slot < 0 || slot >= numSlots)
        return;
    const auto current = midiLearnSlot_.load(std::memory_order_relaxed);
    midiLearnSlot_.store(current == slot ? -1 : slot, std::memory_order_release);
}

bool GlitchDeckPlugin::isMidiLearning(int slot) const noexcept
{
    return midiLearnSlot_.load(std::memory_order_acquire) == slot;
}

void GlitchDeckPlugin::applyPendingMidiLearnFromUi() noexcept
{
    const auto slot = learnedSlot_.exchange(-1, std::memory_order_acq_rel);
    if (slot < 0 || slot >= numSlots)
        return;

    const auto& id = ids_[static_cast<std::size_t>(slot)];
    const std::array<std::pair<clap_id, double>, 3> changes {{
        { id.midiType, static_cast<double>(learnedType_.load(std::memory_order_relaxed)) },
        { id.midiNumber, static_cast<double>(learnedNumber_.load(std::memory_order_relaxed)) },
        { id.midiChannel, static_cast<double>(learnedChannel_.load(std::memory_order_relaxed)) },
    }};

    for (const auto& [parameter, value] : changes)
    {
        beginParameterGesture(parameter);
        setParameterFromGui(parameter, value);
        endParameterGesture(parameter);
    }
    markStateDirty();
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
