#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace
{
constexpr std::array<float, GlitchDeckAudioProcessor::numSlots> defaultLengths {
    125.0f, 12.0f, 250.0f, 600.0f, 450.0f, 350.0f, 180.0f, 120.0f
};
}

GlitchDeckAudioProcessor::GlitchDeckAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "GlitchDeckState", createParameterLayout())
{
    for (auto& active : visibleActive)
        active.store(false, std::memory_order_relaxed);
}

void GlitchDeckAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = std::max(1.0, sampleRate);
    streamSampleCounter = 0;
    pendingTriggers = {};
    lastAutomationDown = {};

    engine.prepare(currentSampleRate, samplesPerBlock, getTotalNumOutputChannels());
    updateEngineConfigs();

    for (auto& active : visibleActive)
        active.store(false, std::memory_order_relaxed);
}

void GlitchDeckAudioProcessor::releaseResources()
{
    engine.reset();
}

bool GlitchDeckAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto output = layouts.getMainOutputChannelSet();
    const auto input = layouts.getMainInputChannelSet();

    if (output != juce::AudioChannelSet::mono() && output != juce::AudioChannelSet::stereo())
        return false;

    return input == output;
}

void GlitchDeckAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    for (auto channel = getTotalNumInputChannels(); channel < getTotalNumOutputChannels(); ++channel)
        buffer.clear(channel, 0, buffer.getNumSamples());

    updateEngineConfigs();

    juce::Optional<juce::AudioPlayHead::PositionInfo> position;
    if (auto* audioPlayHead = getPlayHead())
        position = audioPlayHead->getPosition();

    scanAutomationTriggers(position);
    scanMidiTriggers(midi, position);

    const auto mix = parameters.getRawParameterValue("mix")->load();

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        executePendingTriggersAt(streamSampleCounter + sample);
        engine.process(buffer, sample, 1, mix);
    }

    streamSampleCounter += buffer.getNumSamples();

    for (int i = 0; i < numSlots; ++i)
        visibleActive[static_cast<size_t>(i)].store(engine.isSlotActive(i), std::memory_order_relaxed);
}

juce::AudioProcessorEditor* GlitchDeckAudioProcessor::createEditor()
{
    return new GlitchDeckAudioProcessorEditor(*this);
}

void GlitchDeckAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto xml = parameters.copyState().createXml())
        copyXmlToBinary(*xml, destData);
}

void GlitchDeckAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
    {
        if (xml->hasTagName(parameters.state.getType()))
            parameters.replaceState(juce::ValueTree::fromXml(*xml));
    }

    pendingTriggers = {};
    lastAutomationDown = {};
    engine.reset();
    updateEngineConfigs();
}

juce::String GlitchDeckAudioProcessor::slotParameterId(int slot, const juce::String& suffix)
{
    return "slot" + juce::String(slot + 1) + "." + suffix;
}

juce::StringArray GlitchDeckAudioProcessor::effectNames()
{
    return { "Stutter", "Microloop", "Reverse", "Tape Stop", "Pitch Dive", "Pitch Rise", "Bitcrush", "Dropout" };
}

juce::StringArray GlitchDeckAudioProcessor::quantizeNames()
{
    return { "Free", "1/32", "1/16", "1/8", "1/4", "1 Bar" };
}

juce::StringArray GlitchDeckAudioProcessor::stereoNames()
{
    return { "Linked", "Spread", "Swap", "Mono" };
}

juce::String GlitchDeckAudioProcessor::midiNoteName(int midiNote)
{
    return juce::MidiMessage::getMidiNoteName(juce::jlimit(0, 127, midiNote), true, true, 3);
}

juce::AudioProcessorValueTreeState::ParameterLayout GlitchDeckAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> result;

    result.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "mix", 1 }, "Global Mix",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f }, 1.0f));

    const auto effects = effectNames();
    const auto quantize = quantizeNames();
    const auto stereo = stereoNames();

    for (int slot = 0; slot < numSlots; ++slot)
    {
        const auto prefix = "Trigger " + juce::String(slot + 1) + " ";

        result.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID { slotParameterId(slot, "trigger"), 1 }, prefix + "Trigger", false));

        result.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID { slotParameterId(slot, "effect"), 1 }, prefix + "Effect", effects, slot));

        result.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID { slotParameterId(slot, "latch"), 1 }, prefix + "Latch", false));

        result.push_back(std::make_unique<juce::AudioParameterInt>(
            juce::ParameterID { slotParameterId(slot, "midi"), 1 }, prefix + "MIDI Note", 0, 127, 36 + slot));

        result.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID { slotParameterId(slot, "quantize"), 1 }, prefix + "Quantize", quantize, 0));

        result.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID { slotParameterId(slot, "stereo"), 1 }, prefix + "Stereo", stereo, 0));

        result.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { slotParameterId(slot, "intensity"), 1 }, prefix + "Intensity",
            juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f }, slot == 7 ? 1.0f : 0.82f));

        result.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { slotParameterId(slot, "length"), 1 }, prefix + "Length",
            juce::NormalisableRange<float> { 2.0f, 1500.0f, 0.01f, 0.35f }, defaultLengths[static_cast<size_t>(slot)]));

        result.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { slotParameterId(slot, "attack"), 1 }, prefix + "Attack",
            juce::NormalisableRange<float> { 0.1f, 100.0f, 0.01f, 0.4f }, 2.0f));

        result.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { slotParameterId(slot, "release"), 1 }, prefix + "Release",
            juce::NormalisableRange<float> { 0.1f, 300.0f, 0.01f, 0.4f }, 20.0f));

        result.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { slotParameterId(slot, "shape"), 1 }, prefix + "Shape",
            juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f }, 0.5f));
    }

    return { result.begin(), result.end() };
}

float GlitchDeckAudioProcessor::parameterValue(int slot, const char* suffix) const noexcept
{
    if (auto* value = parameters.getRawParameterValue(slotParameterId(slot, suffix)))
        return value->load();
    return 0.0f;
}

int GlitchDeckAudioProcessor::parameterIntValue(int slot, const char* suffix) const noexcept
{
    return static_cast<int>(std::round(parameterValue(slot, suffix)));
}

bool GlitchDeckAudioProcessor::parameterBoolValue(int slot, const char* suffix) const noexcept
{
    return parameterValue(slot, suffix) >= 0.5f;
}

void GlitchDeckAudioProcessor::updateEngineConfigs()
{
    for (int slot = 0; slot < numSlots; ++slot)
    {
        GlitchEngine::SlotConfig config;
        config.effect = static_cast<GlitchEngine::EffectType>(juce::jlimit(0, 7, parameterIntValue(slot, "effect")));
        config.stereo = static_cast<GlitchEngine::StereoMode>(juce::jlimit(0, 3, parameterIntValue(slot, "stereo")));
        config.intensity = parameterValue(slot, "intensity");
        config.lengthMs = parameterValue(slot, "length");
        config.attackMs = parameterValue(slot, "attack");
        config.releaseMs = parameterValue(slot, "release");
        config.shape = parameterValue(slot, "shape");
        engine.setSlotConfig(slot, config);
    }
}

void GlitchDeckAudioProcessor::scanAutomationTriggers(const juce::Optional<juce::AudioPlayHead::PositionInfo>& position)
{
    for (int slot = 0; slot < numSlots; ++slot)
    {
        const auto down = parameterBoolValue(slot, "trigger");
        if (down != lastAutomationDown[static_cast<size_t>(slot)])
        {
            lastAutomationDown[static_cast<size_t>(slot)] = down;
            scheduleTrigger(slot, down, 0, position);
        }
    }
}

void GlitchDeckAudioProcessor::scanMidiTriggers(const juce::MidiBuffer& midi,
                                                const juce::Optional<juce::AudioPlayHead::PositionInfo>& position)
{
    for (const auto metadata : midi)
    {
        const auto message = metadata.getMessage();
        if (! message.isNoteOnOrOff())
            continue;

        const auto note = message.getNoteNumber();
        for (int slot = 0; slot < numSlots; ++slot)
        {
            if (note != getMidiNoteForSlot(slot))
                continue;

            scheduleTrigger(slot, message.isNoteOn(), metadata.samplePosition, position);
            break;
        }
    }
}

void GlitchDeckAudioProcessor::scheduleTrigger(int slot, bool down, int sampleOffset,
                                               const juce::Optional<juce::AudioPlayHead::PositionInfo>& position)
{
    if (! juce::isPositiveAndBelow(slot, numSlots))
        return;

    const auto latch = parameterBoolValue(slot, "latch");
    if (! down && latch)
        return;

    if (! down)
        cancelPendingOnset(slot);

    const auto target = down ? quantizedTargetSample(slot, sampleOffset, position)
                             : streamSampleCounter + sampleOffset;

    for (auto& pending : pendingTriggers)
    {
        if (! pending.used)
        {
            pending.used = true;
            pending.slot = slot;
            pending.down = down;
            pending.targetSample = target;
            return;
        }
    }

    applyTriggerNow(slot, down);
}

void GlitchDeckAudioProcessor::cancelPendingOnset(int slot)
{
    for (auto& pending : pendingTriggers)
        if (pending.used && pending.slot == slot && pending.down)
            pending.used = false;
}

void GlitchDeckAudioProcessor::executePendingTriggersAt(std::int64_t absoluteSample)
{
    for (auto& pending : pendingTriggers)
    {
        if (pending.used && pending.targetSample <= absoluteSample)
        {
            applyTriggerNow(pending.slot, pending.down);
            pending.used = false;
        }
    }
}

void GlitchDeckAudioProcessor::applyTriggerNow(int slot, bool down)
{
    if (parameterBoolValue(slot, "latch"))
    {
        if (! down)
            return;
        engine.trigger(slot, ! engine.isSlotActive(slot));
    }
    else
    {
        engine.trigger(slot, down);
    }

    visibleActive[static_cast<size_t>(slot)].store(engine.isSlotActive(slot), std::memory_order_relaxed);
}

std::int64_t GlitchDeckAudioProcessor::quantizedTargetSample(
    int slot, int sampleOffset, const juce::Optional<juce::AudioPlayHead::PositionInfo>& position) const
{
    const auto immediate = streamSampleCounter + sampleOffset;
    const auto quantizeIndex = juce::jlimit(0, 5, parameterIntValue(slot, "quantize"));
    if (quantizeIndex == 0 || ! position || ! position->getIsPlaying())
        return immediate;

    const auto bpm = position->getBpm();
    const auto ppq = position->getPpqPosition();
    if (! bpm || ! ppq || *bpm <= 0.0)
        return immediate;

    constexpr std::array<double, 6> gridPpq { 0.0, 0.125, 0.25, 0.5, 1.0, 4.0 };
    const auto grid = gridPpq[static_cast<size_t>(quantizeIndex)];
    const auto ppqPerSample = *bpm / (60.0 * currentSampleRate);
    const auto eventPpq = *ppq + static_cast<double>(sampleOffset) * ppqPerSample;
    const auto targetPpq = std::ceil((eventPpq - 1.0e-9) / grid) * grid;
    const auto deltaSamples = static_cast<std::int64_t>(std::llround((targetPpq - eventPpq) / ppqPerSample));
    return std::max(immediate, immediate + deltaSamples);
}

void GlitchDeckAudioProcessor::setTriggerParameterFromUI(int slot, bool down)
{
    if (! juce::isPositiveAndBelow(slot, numSlots))
        return;

    if (auto* parameter = parameters.getParameter(slotParameterId(slot, "trigger")))
    {
        if (down)
            parameter->beginChangeGesture();

        parameter->setValueNotifyingHost(down ? 1.0f : 0.0f);

        if (! down)
            parameter->endChangeGesture();
    }
}

bool GlitchDeckAudioProcessor::isSlotActive(int slot) const noexcept
{
    return juce::isPositiveAndBelow(slot, numSlots)
        && visibleActive[static_cast<size_t>(slot)].load(std::memory_order_relaxed);
}

int GlitchDeckAudioProcessor::getMidiNoteForSlot(int slot) const noexcept
{
    if (! juce::isPositiveAndBelow(slot, numSlots))
        return 36;
    return juce::jlimit(0, 127, parameterIntValue(slot, "midi"));
}

int GlitchDeckAudioProcessor::getEffectIndexForSlot(int slot) const noexcept
{
    if (! juce::isPositiveAndBelow(slot, numSlots))
        return 0;
    return juce::jlimit(0, 7, parameterIntValue(slot, "effect"));
}

juce::String GlitchDeckAudioProcessor::getEffectNameForSlot(int slot) const
{
    return effectNames()[getEffectIndexForSlot(slot)];
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GlitchDeckAudioProcessor();
}
