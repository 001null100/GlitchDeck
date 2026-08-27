#include "GlitchEngine.h"

#include <algorithm>
#include <cmath>

void GlitchEngine::prepare(double newSampleRate, int, int channels)
{
    sampleRate = std::max(1.0, newSampleRate);
    numChannels = juce::jlimit(1, 2, channels);

    historySize = std::max(4096, static_cast<int>(std::ceil(sampleRate * 8.0)));
    historyLeft.assign(static_cast<size_t>(historySize), 0.0f);
    historyRight.assign(static_cast<size_t>(historySize), 0.0f);

    for (auto& slot : slots)
    {
        slot.envelope.reset(sampleRate, 0.002);
        slot.envelope.setCurrentAndTargetValue(0.0f);
        slot.active = false;
        slot.activeSamples = 0;
    }

    reset();
}

void GlitchEngine::reset()
{
    std::fill(historyLeft.begin(), historyLeft.end(), 0.0f);
    std::fill(historyRight.begin(), historyRight.end(), 0.0f);
    writePosition = 0;
    readPosition = 0.0;
    loopStart = 0;
    loopLength = 1;
    transportEngaged = false;
    streamingTransport = false;
    crushPhase = 0;
    heldCrushLeft = 0.0f;
    heldCrushRight = 0.0f;

    for (auto& slot : slots)
    {
        slot.active = false;
        slot.activeSamples = 0;
        slot.envelope.setCurrentAndTargetValue(0.0f);
    }
}

void GlitchEngine::setSlotConfig(int slot, const SlotConfig& config)
{
    if (juce::isPositiveAndBelow(slot, numSlots))
        slots[static_cast<size_t>(slot)].config = config;
}

void GlitchEngine::trigger(int slotIndex, bool down)
{
    if (! juce::isPositiveAndBelow(slotIndex, numSlots))
        return;

    auto& slot = slots[static_cast<size_t>(slotIndex)];
    if (slot.active == down)
        return;

    slot.active = down;

    if (down)
    {
        slot.activeSamples = 0;
        slot.envelope.reset(sampleRate, std::max(0.0001, static_cast<double>(slot.config.attackMs) / 1000.0));
        slot.envelope.setTargetValue(1.0f);

        if (isTransportEffect(slot.config.effect))
        {
            bool anotherTransportIsActive = false;
            for (int i = 0; i < numSlots; ++i)
            {
                if (i == slotIndex)
                    continue;

                const auto& other = slots[static_cast<size_t>(i)];
                anotherTransportIsActive |= other.active && isTransportEffect(other.config.effect);
            }

            if (slot.config.effect == EffectType::tapeStop && ! anotherTransportIsActive)
                startStreamingTransport();
            else if (isLoopDefiningEffect(slot.config.effect))
                captureLoopForSlot(slotIndex);
            else
                transportEngaged = true;
        }
    }
    else
    {
        slot.envelope.reset(sampleRate, std::max(0.0001, static_cast<double>(slot.config.releaseMs) / 1000.0));
        slot.envelope.setTargetValue(0.0f);
        refreshTransportAfterRelease();
    }
}

bool GlitchEngine::isSlotActive(int slot) const noexcept
{
    return juce::isPositiveAndBelow(slot, numSlots) && slots[static_cast<size_t>(slot)].active;
}

bool GlitchEngine::isTransportEffect(EffectType type) noexcept
{
    return type == EffectType::stutter
        || type == EffectType::microloop
        || type == EffectType::reverse
        || type == EffectType::tapeStop
        || type == EffectType::pitchDive
        || type == EffectType::pitchRise;
}

bool GlitchEngine::isLoopDefiningEffect(EffectType type) noexcept
{
    return type == EffectType::stutter
        || type == EffectType::microloop
        || type == EffectType::reverse
        || type == EffectType::pitchDive
        || type == EffectType::pitchRise;
}

int GlitchEngine::wrapIndex(int index) const noexcept
{
    if (historySize <= 0)
        return 0;

    index %= historySize;
    if (index < 0)
        index += historySize;
    return index;
}

double GlitchEngine::wrapPosition(double position) const noexcept
{
    if (historySize <= 0)
        return 0.0;

    position = std::fmod(position, static_cast<double>(historySize));
    if (position < 0.0)
        position += static_cast<double>(historySize);
    return position;
}

float GlitchEngine::readHistory(int channel, double position) const noexcept
{
    if (historySize <= 1)
        return 0.0f;

    position = wrapPosition(position);
    const auto indexA = static_cast<int>(position);
    const auto indexB = wrapIndex(indexA + 1);
    const auto fraction = static_cast<float>(position - static_cast<double>(indexA));

    const auto& history = (channel == 0 || numChannels == 1) ? historyLeft : historyRight;
    return juce::jmap(fraction, history[static_cast<size_t>(indexA)], history[static_cast<size_t>(indexB)]);
}

int GlitchEngine::millisecondsToSamples(float milliseconds) const noexcept
{
    return std::max(1, static_cast<int>(std::round(static_cast<double>(milliseconds) * sampleRate / 1000.0)));
}

void GlitchEngine::captureLoopForSlot(int slotIndex)
{
    const auto& config = slots[static_cast<size_t>(slotIndex)].config;
    auto desiredLength = millisecondsToSamples(config.lengthMs);

    if (config.effect == EffectType::microloop)
        desiredLength = juce::jlimit(millisecondsToSamples(2.0f), millisecondsToSamples(50.0f), desiredLength);

    desiredLength = juce::jlimit(2, std::max(2, historySize - 8), desiredLength);

    loopLength = desiredLength;
    loopStart = wrapIndex(writePosition - loopLength - 2);
    streamingTransport = false;
    transportEngaged = true;

    bool reverseIsActive = false;
    for (const auto& slot : slots)
        reverseIsActive |= slot.active && slot.config.effect == EffectType::reverse;

    readPosition = reverseIsActive
        ? static_cast<double>(wrapIndex(loopStart + loopLength - 1))
        : static_cast<double>(loopStart);
}

void GlitchEngine::startStreamingTransport()
{
    streamingTransport = true;
    transportEngaged = true;
    readPosition = static_cast<double>(wrapIndex(writePosition - 2));
}

void GlitchEngine::refreshTransportAfterRelease()
{
    bool anyLoop = false;
    bool anyTape = false;

    for (const auto& slot : slots)
    {
        if (! slot.active)
            continue;

        anyLoop |= isLoopDefiningEffect(slot.config.effect);
        anyTape |= slot.config.effect == EffectType::tapeStop;
    }

    if (! anyLoop && anyTape)
        streamingTransport = true;
}

void GlitchEngine::process(juce::AudioBuffer<float>& buffer, int startSample, int numSamplesToProcess, float globalMix)
{
    if (historySize <= 1 || numSamplesToProcess <= 0)
        return;

    const auto channelsInBuffer = buffer.getNumChannels();
    if (channelsInBuffer == 0)
        return;

    auto* left = buffer.getWritePointer(0);
    auto* right = channelsInBuffer > 1 ? buffer.getWritePointer(1) : nullptr;
    globalMix = juce::jlimit(0.0f, 1.0f, globalMix);

    for (int sample = startSample; sample < startSample + numSamplesToProcess; ++sample)
    {
        const auto dryLeft = left[sample];
        const auto dryRight = right != nullptr ? right[sample] : dryLeft;

        historyLeft[static_cast<size_t>(writePosition)] = dryLeft;
        historyRight[static_cast<size_t>(writePosition)] = dryRight;
        writePosition = wrapIndex(writePosition + 1);

        std::array<float, numSlots> envelopeValues {};
        float transportWet = 0.0f;
        float crushAmount = 0.0f;
        float dropoutAmount = 0.0f;
        bool reverse = false;
        bool anyTransportActive = false;
        double playbackRate = 1.0;
        int dominantTransportSlot = -1;
        float dominantTransportWeight = -1.0f;

        for (int i = 0; i < numSlots; ++i)
        {
            auto& slot = slots[static_cast<size_t>(i)];
            const auto envelope = slot.envelope.getNextValue();
            envelopeValues[static_cast<size_t>(i)] = envelope;

            if (slot.active)
                ++slot.activeSamples;

            const auto weight = envelope * juce::jlimit(0.0f, 1.0f, slot.config.intensity);

            if (isTransportEffect(slot.config.effect))
            {
                transportWet = std::max(transportWet, weight);
                anyTransportActive |= slot.active;

                if (weight > dominantTransportWeight)
                {
                    dominantTransportWeight = weight;
                    dominantTransportSlot = i;
                }
            }

            if (slot.config.effect == EffectType::reverse && envelope > 0.001f)
                reverse = slot.active || envelope > 0.05f;

            if ((slot.config.effect == EffectType::pitchDive || slot.config.effect == EffectType::pitchRise)
                && envelope > 0.0001f)
            {
                const auto duration = std::max(1, millisecondsToSamples(slot.config.lengthMs));
                const auto progress = juce::jlimit(0.0, 1.0, static_cast<double>(slot.activeSamples) / static_cast<double>(duration));
                const auto exponent = 0.35 + static_cast<double>(slot.config.shape) * 2.65;
                const auto shaped = std::pow(progress, exponent);
                const auto direction = slot.config.effect == EffectType::pitchRise ? 1.0 : -1.0;
                const auto semitones = direction * 24.0 * static_cast<double>(slot.config.intensity) * shaped * static_cast<double>(envelope);
                playbackRate *= std::pow(2.0, semitones / 12.0);
            }

            if (slot.config.effect == EffectType::tapeStop && envelope > 0.0001f)
            {
                const auto duration = std::max(1, millisecondsToSamples(slot.config.lengthMs));
                const auto progress = juce::jlimit(0.0, 1.0, static_cast<double>(slot.activeSamples) / static_cast<double>(duration));
                const auto exponent = 0.25 + static_cast<double>(slot.config.shape) * 2.75;
                const auto stoppedRate = std::pow(std::max(0.0, 1.0 - progress), exponent);
                const auto depth = static_cast<double>(slot.config.intensity) * static_cast<double>(envelope);
                playbackRate *= 1.0 + depth * (stoppedRate - 1.0);
            }

            if (slot.config.effect == EffectType::bitcrush)
                crushAmount = std::max(crushAmount, weight);
            else if (slot.config.effect == EffectType::dropout)
                dropoutAmount = std::max(dropoutAmount, weight);
        }

        float processedLeft = dryLeft;
        float processedRight = dryRight;

        if (transportEngaged && transportWet > 0.000001f)
        {
            auto leftReadPosition = readPosition;
            auto rightReadPosition = readPosition;

            GlitchEngine::StereoMode stereoMode = StereoMode::linked;
            float stereoIntensity = 0.0f;
            if (juce::isPositiveAndBelow(dominantTransportSlot, numSlots))
            {
                const auto& config = slots[static_cast<size_t>(dominantTransportSlot)].config;
                stereoMode = config.stereo;
                stereoIntensity = config.intensity;
            }

            if (stereoMode == StereoMode::spread)
            {
                const auto spreadSamples = streamingTransport
                    ? static_cast<double>(millisecondsToSamples(6.0f + stereoIntensity * 18.0f))
                    : std::max(1.0, static_cast<double>(loopLength) * (0.03 + static_cast<double>(stereoIntensity) * 0.17));

                if (streamingTransport)
                {
                    rightReadPosition = wrapPosition(readPosition - spreadSamples);
                }
                else
                {
                    auto offset = readPosition - static_cast<double>(loopStart);
                    if (offset < 0.0)
                        offset += static_cast<double>(historySize);
                    offset = std::fmod(offset + spreadSamples, static_cast<double>(loopLength));
                    rightReadPosition = wrapPosition(static_cast<double>(loopStart) + offset);
                }
            }

            auto wetLeft = readHistory(0, leftReadPosition);
            auto wetRight = readHistory(1, rightReadPosition);

            if (stereoMode == StereoMode::swap)
                std::swap(wetLeft, wetRight);
            else if (stereoMode == StereoMode::mono)
                wetLeft = wetRight = 0.5f * (wetLeft + wetRight);

            processedLeft = juce::jmap(transportWet, dryLeft, wetLeft);
            processedRight = juce::jmap(transportWet, dryRight, wetRight);

            const auto signedRate = reverse ? -playbackRate : playbackRate;

            if (streamingTransport)
            {
                readPosition = wrapPosition(readPosition + signedRate);
            }
            else
            {
                auto offset = readPosition - static_cast<double>(loopStart);
                if (offset < 0.0)
                    offset += static_cast<double>(historySize);

                offset += signedRate;
                const auto loopLengthDouble = static_cast<double>(std::max(1, loopLength));
                offset = std::fmod(offset, loopLengthDouble);
                if (offset < 0.0)
                    offset += loopLengthDouble;

                readPosition = wrapPosition(static_cast<double>(loopStart) + offset);
            }
        }

        if (crushAmount > 0.000001f)
        {
            const auto holdPeriod = 1 + static_cast<int>(std::round(crushAmount * crushAmount * 31.0f));
            if (crushPhase <= 0)
            {
                const auto bits = juce::jlimit(3, 16, 16 - static_cast<int>(std::round(crushAmount * 13.0f)));
                const auto scale = static_cast<float>(1 << (bits - 1));
                heldCrushLeft = std::round(processedLeft * scale) / scale;
                heldCrushRight = std::round(processedRight * scale) / scale;
                crushPhase = holdPeriod;
            }

            --crushPhase;
            processedLeft = juce::jmap(crushAmount, processedLeft, heldCrushLeft);
            processedRight = juce::jmap(crushAmount, processedRight, heldCrushRight);
        }
        else
        {
            crushPhase = 0;
        }

        if (dropoutAmount > 0.000001f)
        {
            const auto gain = 1.0f - juce::jlimit(0.0f, 1.0f, dropoutAmount);
            processedLeft *= gain;
            processedRight *= gain;
        }

        left[sample] = dryLeft + globalMix * (processedLeft - dryLeft);
        if (right != nullptr)
            right[sample] = dryRight + globalMix * (processedRight - dryRight);

        if (! anyTransportActive && transportWet <= 0.00001f)
        {
            transportEngaged = false;
            streamingTransport = false;
        }
    }
}
