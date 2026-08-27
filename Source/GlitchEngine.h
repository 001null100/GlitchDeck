#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

class GlitchEngine
{
public:
    static constexpr int numSlots = 8;

    enum class EffectType
    {
        stutter = 0,
        microloop,
        reverse,
        tapeStop,
        pitchDive,
        pitchRise,
        bitcrush,
        dropout
    };

    enum class StereoMode
    {
        linked = 0,
        spread,
        swap,
        mono
    };

    struct SlotConfig
    {
        EffectType effect = EffectType::stutter;
        StereoMode stereo = StereoMode::linked;
        float intensity = 0.8f;
        float lengthMs = 125.0f;
        float attackMs = 2.0f;
        float releaseMs = 20.0f;
        float shape = 0.5f;
    };

    void prepare(double newSampleRate, int maximumBlockSize, int channels);
    void reset();

    void setSlotConfig(int slot, const SlotConfig& config);
    void trigger(int slot, bool down);
    bool isSlotActive(int slot) const noexcept;

    void process(juce::AudioBuffer<float>& buffer, int startSample, int numSamples, float globalMix);

private:
    struct SlotRuntime
    {
        SlotConfig config;
        bool active = false;
        std::int64_t activeSamples = 0;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> envelope;
    };

    static bool isTransportEffect(EffectType type) noexcept;
    static bool isLoopDefiningEffect(EffectType type) noexcept;

    int wrapIndex(int index) const noexcept;
    double wrapPosition(double position) const noexcept;
    float readHistory(int channel, double position) const noexcept;
    int millisecondsToSamples(float milliseconds) const noexcept;

    void captureLoopForSlot(int slot);
    void startStreamingTransport();
    void refreshTransportAfterRelease();

    std::array<SlotRuntime, numSlots> slots;

    std::vector<float> historyLeft;
    std::vector<float> historyRight;
    int historySize = 0;
    int writePosition = 0;

    double sampleRate = 44100.0;
    int numChannels = 2;

    bool transportEngaged = false;
    bool streamingTransport = false;
    double readPosition = 0.0;
    int loopStart = 0;
    int loopLength = 1;

    int crushPhase = 0;
    float heldCrushLeft = 0.0f;
    float heldCrushRight = 0.0f;
};
