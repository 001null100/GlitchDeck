#pragma once

#include "GlitchEngine.h"
#include "ParameterIds.hpp"

#include <nullclap/Plugin.hpp>

#include <array>
#include <atomic>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

class GlitchDeckPlugin final : public nullclap::Plugin
{
public:
    static constexpr int numSlots = GlitchEngine::numSlots;

    enum class MidiBindingType
    {
        note = 0,
        cc = 1,
    };

    static const clap_plugin_descriptor_t& descriptor() noexcept;
    explicit GlitchDeckPlugin(const clap_host_t* host);
    ~GlitchDeckPlugin() override = default;

    const glitchdeck::ids::Slot& slotIds(int slot) const noexcept;
    double parameterValue(clap_id id) const noexcept;
    int parameterInt(clap_id id) const noexcept;
    bool parameterBool(clap_id id) const noexcept;

    void setTriggerFromUi(int slot, bool down) noexcept;
    bool isSlotActive(int slot) const noexcept;

    void toggleMidiLearn(int slot) noexcept;
    bool isMidiLearning(int slot) const noexcept;
    void applyPendingMidiLearnFromUi() noexcept;

    int effectIndex(int slot) const noexcept;
    std::string effectName(int slot) const;
    std::string midiBindingText(int slot) const;

    static const std::array<const char*, 8>& effectNames() noexcept;
    static const std::array<const char*, 6>& quantizeNames() noexcept;
    static const std::array<const char*, 4>& stereoNames() noexcept;
    static const std::array<const char*, 2>& midiTypeNames() noexcept;

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

    static constexpr std::size_t uiQueueCapacity = 64;

    bool onActivate(double sampleRate, std::uint32_t minFrames, std::uint32_t maxFrames) noexcept override;
    void onReset() noexcept override;
    void processAudio(const clap_process_t& process,
                      std::uint32_t startFrame,
                      std::uint32_t endFrame) noexcept override;
    void onEvent(const clap_event_header_t& event) noexcept override;
    bool loadExtraState(std::span<const std::byte> bytes) override;

    void registerParameters();
    void registerPorts();
    void registerRemoteControls();
    void updateEngineConfigs() noexcept;

    void drainUiTriggers(std::uint32_t eventTime) noexcept;
    bool tryCaptureMidiLearn(std::uint8_t status, std::uint8_t data1, std::uint8_t data2) noexcept;
    void handleMidiEvent(const clap_event_midi_t& event) noexcept;
    void handleParameterEvent(const clap_event_param_value_t& event) noexcept;
    void handleTransportEvent(const clap_event_transport_t& event) noexcept;

    void scheduleTrigger(int slot, bool down, std::uint32_t eventTime) noexcept;
    void cancelPendingOnset(int slot) noexcept;
    void executePendingTriggersAt(std::int64_t absoluteSample) noexcept;
    void applyTriggerNow(int slot, bool down) noexcept;
    std::int64_t quantizedTargetSample(int slot, std::uint32_t eventTime) const noexcept;

    bool pushUiTrigger(const UiTriggerEvent& event) noexcept;
    bool popUiTrigger(UiTriggerEvent& event) noexcept;

    std::array<glitchdeck::ids::Slot, numSlots> ids_ { glitchdeck::ids::makeSlots() };
    GlitchEngine engine_;

    std::array<bool, numSlots> lastAutomationDown_ {};
    std::array<bool, numSlots> lastCcDown_ {};
    std::array<std::atomic<bool>, numSlots> visibleActive_ {};
    std::array<PendingTrigger, 64> pendingTriggers_ {};

    std::array<UiTriggerEvent, uiQueueCapacity> uiTriggerQueue_ {};
    std::atomic<std::size_t> uiQueueWrite_ { 0 };
    std::atomic<std::size_t> uiQueueRead_ { 0 };

    std::atomic<int> midiLearnSlot_ { -1 };
    std::atomic<int> learnedSlot_ { -1 };
    std::atomic<int> learnedType_ { 0 };
    std::atomic<int> learnedNumber_ { 0 };
    std::atomic<int> learnedChannel_ { 0 };

    clap_event_transport_t transport_ {};
    bool hasTransport_ = false;
    std::int64_t transportAnchorSample_ = 0;
    std::int64_t transportBlockStartSample_ = -1;

    std::int64_t streamSampleCounter_ = 0;
    double sampleRate_ = 44100.0;
};
