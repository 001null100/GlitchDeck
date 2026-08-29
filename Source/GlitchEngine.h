#pragma once

#include <array>
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

    struct StereoSample
    {
        float left = 0.0f;
        float right = 0.0f;
    };

    void prepare(double newSampleRate, int channels = 2);
    void reset() noexcept;

    void setSlotConfig(int slot, const SlotConfig& config) noexcept;
    void trigger(int slot, bool down) noexcept;
    bool isSlotActive(int slot) const noexcept;

    StereoSample processSample(float left, float right, float globalMix) noexcept;

private:
    struct LinearEnvelope
    {
        void reset(float value = 0.0f) noexcept;
        void setTarget(float value, int samples) noexcept;
        float next() noexcept;

        float current = 0.0f;
        float target = 0.0f;
        float step = 0.0f;
        int remaining = 0;
    };

    struct SlotRuntime
    {
        SlotConfig config;
        bool active = false;
        std::int64_t activeSamples = 0;
        LinearEnvelope envelope;
    };

    static bool isTransportEffect(EffectType type) noexcept;
    static bool isLoopDefiningEffect(EffectType type) noexcept;
    static bool requiresCapturedTransport(EffectType type) noexcept;
    static float lerp(float amount, float a, float b) noexcept;

    int wrapIndex(int index) const noexcept;
    double wrapPosition(double position) const noexcept;
    float readHistory(int channel, double position) const noexcept;
    float readCapturedHistory(int channel, double position) const noexcept;
    int millisecondsToSamples(float milliseconds) const noexcept;

    void captureLoopForSlot(int slot, bool looping) noexcept;
    void startStreamingTransport() noexcept;
    void refreshTransportAfterRelease() noexcept;

    std::array<SlotRuntime, numSlots> slots {};

    std::vector<float> historyLeft;
    std::vector<float> historyRight;
    int historySize = 0;
    int historySamplesAvailable = 0;
    int writePosition = 0;

    double sampleRate = 44100.0;
    int numChannels = 2;

    bool transportEngaged = false;
    bool streamingTransport = false;
    bool capturedTransportLoops = true;
    double readPosition = 0.0;
    int loopStart = 0;
    int loopLength = 1;

    int crushPhase = 0;
    float heldCrushLeft = 0.0f;
    float heldCrushRight = 0.0f;
};
