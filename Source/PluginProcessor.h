#pragma once

#include <JuceHeader.h>
#include "GlitchEngine.h"

#include <array>
#include <atomic>
#include <cstdint>

class GlitchDeckAudioProcessor : public juce::AudioProcessor
{
public:
    static constexpr int numSlots = GlitchEngine::numSlots;

    GlitchDeckAudioProcessor();
    ~GlitchDeckAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getValueTreeState() noexcept { return parameters; }
    const juce::AudioProcessorValueTreeState& getValueTreeState() const noexcept { return parameters; }

    void setTriggerParameterFromUI(int slot, bool down);
    bool isSlotActive(int slot) const noexcept;
    int getMidiNoteForSlot(int slot) const noexcept;
    int getEffectIndexForSlot(int slot) const noexcept;
    juce::String getEffectNameForSlot(int slot) const;

    static juce::String slotParameterId(int slot, const juce::String& suffix);
    static juce::String midiNoteName(int midiNote);
    static juce::StringArray effectNames();
    static juce::StringArray quantizeNames();
    static juce::StringArray stereoNames();

private:
    struct PendingTrigger
    {
        bool used = false;
        int slot = 0;
        bool down = false;
        std::int64_t targetSample = 0;
    };

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void updateEngineConfigs();
    void scanAutomationTriggers(const juce::Optional<juce::AudioPlayHead::PositionInfo>& position);
    void scanMidiTriggers(const juce::MidiBuffer& midi,
                          const juce::Optional<juce::AudioPlayHead::PositionInfo>& position);
    void scheduleTrigger(int slot, bool down, int sampleOffset,
                         const juce::Optional<juce::AudioPlayHead::PositionInfo>& position);
    void cancelPendingOnset(int slot);
    void executePendingTriggersAt(std::int64_t absoluteSample);
    void applyTriggerNow(int slot, bool down);
    std::int64_t quantizedTargetSample(int slot, int sampleOffset,
                                      const juce::Optional<juce::AudioPlayHead::PositionInfo>& position) const;

    float parameterValue(int slot, const char* suffix) const noexcept;
    int parameterIntValue(int slot, const char* suffix) const noexcept;
    bool parameterBoolValue(int slot, const char* suffix) const noexcept;

    juce::AudioProcessorValueTreeState parameters;
    GlitchEngine engine;

    std::array<bool, numSlots> lastAutomationDown {};
    std::array<std::atomic<bool>, numSlots> visibleActive {};
    std::array<PendingTrigger, 64> pendingTriggers {};

    std::int64_t streamSampleCounter = 0;
    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GlitchDeckAudioProcessor)
};
