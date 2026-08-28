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

    enum class MidiBindingType
    {
        Note = 0,
        CC = 1
    };

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
    int getEffectIndexForSlot(int slot) const noexcept;
    juce::String getEffectNameForSlot(int slot) const;

    void toggleMidiLearn(int slot) noexcept;
    bool isMidiLearning(int slot) const noexcept;
    void applyPendingMidiLearn();
    juce::String getMidiBindingTextForSlot(int slot) const;

    static juce::String slotParameterId(int slot, const juce::String& suffix);
    static juce::String midiNoteName(int midiNote);
    static juce::StringArray effectNames();
    static juce::StringArray quantizeNames();
    static juce::StringArray stereoNames();
    static juce::StringArray midiBindingTypeNames();

private:
    struct PendingTrigger
    {
        bool used = false;
        int slot = 0;
        bool down = false;
        std::int64_t targetSample = 0;
    };

    struct UiTriggerEvent
    {
        int slot = 0;
        bool down = false;
    };

    static constexpr unsigned int uiQueueCapacity = 64;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void updateEngineConfigs();
    void drainUiTriggers(const juce::Optional<juce::AudioPlayHead::PositionInfo>& position);
    void scanAutomationTriggers(const juce::Optional<juce::AudioPlayHead::PositionInfo>& position);
    void scanMidiTriggers(const juce::MidiBuffer& midi,
                          const juce::Optional<juce::AudioPlayHead::PositionInfo>& position);
    bool tryCaptureMidiLearn(const juce::MidiMessage& message) noexcept;
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
    void setParameterPlainFromMessageThread(int slot, const char* suffix, float plainValue);

    juce::AudioProcessorValueTreeState parameters;
    GlitchEngine engine;

    std::array<bool, numSlots> lastAutomationDown {};
    std::array<bool, numSlots> lastCcDown {};
    std::array<std::atomic<bool>, numSlots> visibleActive {};
    std::array<PendingTrigger, 64> pendingTriggers {};

    // Single producer (JUCE message thread) / single consumer (audio thread).
    std::array<UiTriggerEvent, uiQueueCapacity> uiTriggerQueue {};
    std::atomic<unsigned int> uiQueueWrite { 0 };
    std::atomic<unsigned int> uiQueueRead { 0 };

    // MIDI learn is requested by the message thread and captured by the audio
    // thread. The learned mapping is committed to APVTS later on the message
    // thread so the audio callback never notifies the host or mutates ValueTrees.
    std::atomic<int> midiLearnSlot { -1 };
    std::atomic<int> learnedSlot { -1 };
    std::atomic<int> learnedType { 0 };
    std::atomic<int> learnedNumber { 0 };
    std::atomic<int> learnedChannel { 0 };

    std::int64_t streamSampleCounter = 0;
    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GlitchDeckAudioProcessor)
};
