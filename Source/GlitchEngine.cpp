#include "GlitchEngine.h"

#include <algorithm>
#include <cmath>

void GlitchEngine::LinearEnvelope::reset(float value) noexcept
{
    current = target = value;
    step = 0.0f;
    remaining = 0;
}

void GlitchEngine::LinearEnvelope::setTarget(float value, int samples) noexcept
{
    target = value;
    remaining = std::max(1, samples);
    step = (target - current) / static_cast<float>(remaining);
}

float GlitchEngine::LinearEnvelope::next() noexcept
{
    if (remaining > 0)
    {
        current += step;
        if (--remaining == 0)
            current = target;
    }
    return current;
}

float GlitchEngine::lerp(float amount, float a, float b) noexcept
{
    return a + amount * (b - a);
}

void GlitchEngine::prepare(double newSampleRate, int channels)
{
    sampleRate = std::max(1.0, newSampleRate);
    numChannels = std::clamp(channels, 1, 2);

    historySize = std::max(4096, static_cast<int>(std::ceil(sampleRate * 8.0)));
    historyLeft.assign(static_cast<std::size_t>(historySize), 0.0f);
    historyRight.assign(static_cast<std::size_t>(historySize), 0.0f);

    for (auto& slot : slots)
    {
        slot.envelope.reset();
        slot.active = false;
        slot.activeSamples = 0;
    }

    reset();
}

void GlitchEngine::reset() noexcept
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
        slot.envelope.reset();
    }
}

void GlitchEngine::setSlotConfig(int slot, const SlotConfig& config) noexcept
{
    if (slot >= 0 && slot < numSlots)
        slots[static_cast<std::size_t>(slot)].config = config;
}

void GlitchEngine::trigger(int slotIndex, bool down) noexcept
{
    if (slotIndex < 0 || slotIndex >= numSlots)
        return;

    auto& slot = slots[static_cast<std::size_t>(slotIndex)];
    if (slot.active == down)
        return;

    slot.active = down;

    if (down)
    {
        slot.activeSamples = 0;
        slot.envelope.setTarget(1.0f, millisecondsToSamples(std::max(0.1f, slot.config.attackMs)));

        if (isTransportEffect(slot.config.effect))
        {
            bool anotherTransportIsActive = false;
            for (int i = 0; i < numSlots; ++i)
            {
                if (i == slotIndex)
                    continue;
                const auto& other = slots[static_cast<std::size_t>(i)];
                anotherTransportIsActive |= other.active && isTransportEffect(other.config.effect);
            }

            if (isLoopDefiningEffect(slot.config.effect))
            {
                captureLoopForSlot(slotIndex);
            }
            else if (!anotherTransportIsActive)
            {
                // Reverse and Pitch Rise cannot run indefinitely against a live
                // write head with a single read pointer. Reverse walks away from
                // the present forever, while Pitch Rise (>1x) overtakes unwritten
                // history. Give those standalone gestures a bounded capture. When
                // they are added to an existing Stutter/Microloop they remain true
                // modifiers and do not recapture the shared region.
                if (requiresCapturedTransport(slot.config.effect))
                    captureLoopForSlot(slotIndex);
                else
                    startStreamingTransport();
            }
            else
            {
                transportEngaged = true;
            }
        }
    }
    else
    {
        slot.envelope.setTarget(0.0f, millisecondsToSamples(std::max(0.1f, slot.config.releaseMs)));
        refreshTransportAfterRelease();
    }
}

bool GlitchEngine::isSlotActive(int slot) const noexcept
{
    return slot >= 0 && slot < numSlots && slots[static_cast<std::size_t>(slot)].active;
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
        || type == EffectType::microloop;
}

bool GlitchEngine::requiresCapturedTransport(EffectType type) noexcept
{
    return type == EffectType::reverse
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
    return lerp(fraction, history[static_cast<std::size_t>(indexA)], history[static_cast<std::size_t>(indexB)]);
}

float GlitchEngine::readCapturedHistory(int channel, double position) const noexcept
{
    if (historySize <= 1 || loopLength <= 1)
        return readHistory(channel, position);

    // Convert the absolute circular-history position to a fractional offset inside
    // the active capture, then wrap BOTH interpolation taps inside that region.
    // Without this, a fractional head near the final captured sample blends with
    // the next global-history sample, leaking live/unwritten audio into Pitch Rise.
    auto offset = wrapPosition(position) - static_cast<double>(loopStart);
    if (offset < 0.0)
        offset += static_cast<double>(historySize);

    const auto length = static_cast<double>(loopLength);
    offset = std::fmod(offset, length);
    if (offset < 0.0)
        offset += length;

    const auto offsetA = static_cast<int>(std::floor(offset));
    const auto offsetB = (offsetA + 1) % loopLength;
    const auto fraction = static_cast<float>(offset - static_cast<double>(offsetA));
    const auto indexA = wrapIndex(loopStart + offsetA);
    const auto indexB = wrapIndex(loopStart + offsetB);
    const auto& history = (channel == 0 || numChannels == 1) ? historyLeft : historyRight;
    return lerp(fraction, history[static_cast<std::size_t>(indexA)], history[static_cast<std::size_t>(indexB)]);
}

int GlitchEngine::millisecondsToSamples(float milliseconds) const noexcept
{
    return std::max(1, static_cast<int>(std::round(static_cast<double>(milliseconds) * sampleRate / 1000.0)));
}

void GlitchEngine::captureLoopForSlot(int slotIndex) noexcept
{
    const auto& config = slots[static_cast<std::size_t>(slotIndex)].config;
    auto desiredLength = millisecondsToSamples(config.lengthMs);

    if (config.effect == EffectType::microloop)
        desiredLength = std::clamp(desiredLength, millisecondsToSamples(2.0f), millisecondsToSamples(50.0f));

    desiredLength = std::clamp(desiredLength, 2, std::max(2, historySize - 8));
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

void GlitchEngine::startStreamingTransport() noexcept
{
    streamingTransport = true;
    transportEngaged = true;
    readPosition = static_cast<double>(wrapIndex(writePosition - 2));
}

void GlitchEngine::refreshTransportAfterRelease() noexcept
{
    bool anyLoopSource = false;
    bool anyCapturedModifier = false;
    bool anyStreamingModifier = false;

    for (const auto& slot : slots)
    {
        if (!slot.active || !isTransportEffect(slot.config.effect))
            continue;

        anyLoopSource |= isLoopDefiningEffect(slot.config.effect);
        anyCapturedModifier |= requiresCapturedTransport(slot.config.effect);
        anyStreamingModifier |= !isLoopDefiningEffect(slot.config.effect)
            && !requiresCapturedTransport(slot.config.effect);
    }

    if (anyLoopSource || anyCapturedModifier)
    {
        // Preserve the existing captured region. In particular, releasing a
        // Stutter while Reverse remains held should keep reversing that fragment
        // instead of suddenly wandering backward through the entire history.
        streamingTransport = false;
        transportEngaged = true;
    }
    else if (anyStreamingModifier)
    {
        // Pitch Dive and Tape Stop are valid against a recent-history streaming
        // head. Re-anchor to recent audio when a loop source disappears.
        startStreamingTransport();
    }
}

GlitchEngine::StereoSample GlitchEngine::processSample(float dryLeft, float dryRight, float globalMix) noexcept
{
    if (historySize <= 1)
        return { dryLeft, dryRight };

    historyLeft[static_cast<std::size_t>(writePosition)] = dryLeft;
    historyRight[static_cast<std::size_t>(writePosition)] = dryRight;
    writePosition = wrapIndex(writePosition + 1);

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
        auto& slot = slots[static_cast<std::size_t>(i)];
        const auto envelope = slot.envelope.next();
        if (slot.active)
            ++slot.activeSamples;

        const auto weight = envelope * std::clamp(slot.config.intensity, 0.0f, 1.0f);

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
            const auto progress = std::clamp(static_cast<double>(slot.activeSamples) / static_cast<double>(duration), 0.0, 1.0);
            const auto exponent = 0.35 + static_cast<double>(slot.config.shape) * 2.65;
            const auto shaped = std::pow(progress, exponent);
            const auto direction = slot.config.effect == EffectType::pitchRise ? 1.0 : -1.0;
            const auto semitones = direction * 24.0 * static_cast<double>(slot.config.intensity)
                * shaped * static_cast<double>(envelope);
            playbackRate *= std::pow(2.0, semitones / 12.0);
        }

        if (slot.config.effect == EffectType::tapeStop && envelope > 0.0001f)
        {
            const auto duration = std::max(1, millisecondsToSamples(slot.config.lengthMs));
            const auto progress = std::clamp(static_cast<double>(slot.activeSamples) / static_cast<double>(duration), 0.0, 1.0);
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

        StereoMode stereoMode = StereoMode::linked;
        float stereoIntensity = 0.0f;
        if (dominantTransportSlot >= 0 && dominantTransportSlot < numSlots)
        {
            const auto& config = slots[static_cast<std::size_t>(dominantTransportSlot)].config;
            stereoMode = config.stereo;
            stereoIntensity = config.intensity;
        }

        if (stereoMode == StereoMode::spread)
        {
            const auto spreadSamples = streamingTransport
                ? static_cast<double>(millisecondsToSamples(6.0f + stereoIntensity * 18.0f))
                : std::max(1.0, static_cast<double>(loopLength) * (0.03 + static_cast<double>(stereoIntensity) * 0.17));

            if (streamingTransport)
                rightReadPosition = wrapPosition(readPosition - spreadSamples);
            else
            {
                auto offset = readPosition - static_cast<double>(loopStart);
                if (offset < 0.0)
                    offset += static_cast<double>(historySize);
                offset = std::fmod(offset + spreadSamples, static_cast<double>(loopLength));
                rightReadPosition = wrapPosition(static_cast<double>(loopStart) + offset);
            }
        }

        auto wetLeft = streamingTransport
            ? readHistory(0, leftReadPosition)
            : readCapturedHistory(0, leftReadPosition);
        auto wetRight = streamingTransport
            ? readHistory(1, rightReadPosition)
            : readCapturedHistory(1, rightReadPosition);

        if (stereoMode == StereoMode::swap)
            std::swap(wetLeft, wetRight);
        else if (stereoMode == StereoMode::mono)
            wetLeft = wetRight = 0.5f * (wetLeft + wetRight);

        processedLeft = lerp(transportWet, dryLeft, wetLeft);
        processedRight = lerp(transportWet, dryRight, wetRight);

        const auto signedRate = reverse ? -playbackRate : playbackRate;
        if (streamingTransport)
            readPosition = wrapPosition(readPosition + signedRate);
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
            const auto bits = std::clamp(16 - static_cast<int>(std::round(crushAmount * 13.0f)), 3, 16);
            const auto scale = static_cast<float>(1 << (bits - 1));
            heldCrushLeft = std::round(processedLeft * scale) / scale;
            heldCrushRight = std::round(processedRight * scale) / scale;
            crushPhase = holdPeriod;
        }
        --crushPhase;
        processedLeft = lerp(crushAmount, processedLeft, heldCrushLeft);
        processedRight = lerp(crushAmount, processedRight, heldCrushRight);
    }
    else
    {
        crushPhase = 0;
    }

    if (dropoutAmount > 0.000001f)
    {
        const auto gain = 1.0f - std::clamp(dropoutAmount, 0.0f, 1.0f);
        processedLeft *= gain;
        processedRight *= gain;
    }

    globalMix = std::clamp(globalMix, 0.0f, 1.0f);
    if (!anyTransportActive && transportWet <= 0.00001f)
    {
        transportEngaged = false;
        streamingTransport = false;
    }

    return {
        dryLeft + globalMix * (processedLeft - dryLeft),
        dryRight + globalMix * (processedRight - dryRight)
    };
}
