#pragma once

#include "GlitchEngine.h"
#include "ParameterIds.hpp"
#include <nullclap/Plugin.hpp>
#include <array>
#include <atomic>
#include <cstdint>
#include <span>
#include <string>

class GlitchDeckPlugin final : public nullclap::Plugin
{
public:
    static constexpr int numSlots = GlitchEngine::numSlots;
    enum class MidiBindingType { note = 0, cc = 1 };
    enum class UiSource { mouse = 0, keyboard = 1 };

    static const clap_plugin_descriptor_t& descriptor() noexcept;
    explicit GlitchDeckPlugin(const clap_host_t* host);
    ~GlitchDeckPlugin() override = default;
    const glitchdeck::ids::Slot& slotIds(int slot) const noexcept;
    double parameterValue(clap_id id) const noexcept;
    int parameterInt(clap_id id) const noexcept;
    bool parameterBool(clap_id id) const noexcept;

    // Main-thread edits retain unsent values and gesture ends under backpressure.
    void beginUiEdit(clap_id id) noexcept;
    void setUiValue(clap_id id, double value) noexcept;
    void endUiEdit(clap_id id) noexcept;
    void setUiValueOnce(clap_id id, double value) noexcept;
    void setTriggerFromUi(int slot, bool down, UiSource source = UiSource::mouse) noexcept;
    void releaseUiTriggers() noexcept;
    void panicFromUi() noexcept;
    bool isSlotActive(int slot) const noexcept;
    bool isSlotPending(int slot) const noexcept;

    void toggleMidiLearn(int slot) noexcept;
    bool isMidiLearning(int slot) const noexcept;
    void applyPendingMidiLearnFromUi() noexcept;
    int effectIndex(int slot) const noexcept;
    std::string effectName(int slot) const;
    std::string midiBindingText(int slot) const;
    std::string midiActivityText() const;
    static const std::array<const char*, 8>& effectNames() noexcept;
    static const std::array<const char*, 6>& quantizeNames() noexcept;
    static const std::array<const char*, 4>& stereoNames() noexcept;
    static const std::array<const char*, 2>& midiTypeNames() noexcept;

private:
    struct PendingTrigger { bool used = false; std::int64_t targetSample = 0; bool quantized = false; double targetBeat = 0.0; };
    struct UiTriggerEvent { int slot = 0; bool down = false; std::uint64_t sequence = 0; };
    struct HeldNote
    {
        bool used = false, rawMidi = false;
        int channel = 0, key = 0;
        std::int32_t noteId = -1;
        std::uint8_t slots = 0;
    };
    struct UiEdit
    {
        clap_id id = CLAP_INVALID_ID;
        bool active = false, began = false, hasValue = false, end = true;
        double value = 0.0;
    };
    static constexpr std::size_t uiQueueCapacity = 64;

    bool onActivate(double, std::uint32_t, std::uint32_t) noexcept override;
    void onReset() noexcept override;
    void onMainThreadCallback() noexcept override;
    void processAudio(const clap_process_t&, std::uint32_t, std::uint32_t) noexcept override;
    clap_process_status processFinished() noexcept override;
    void onEvent(const clap_event_header_t&) noexcept override;
    bool loadExtraState(std::span<const std::byte>) override;
    void registerParameters();
    void registerPorts();
    void registerRemoteControls();
    void updateEngineConfigs() noexcept;
    void prepareBlock() noexcept;
    void resetPerformance() noexcept;
    void stopSlot(int slot) noexcept;
    void updateInputGate(int slot, std::uint32_t eventTime) noexcept;
    void releaseMidiChannel(int channel, bool controllersOnly) noexcept;
    void updateNoteInput(bool raw, int channel, int key, std::int32_t noteId,
                         bool down, bool choke, std::uint32_t eventTime) noexcept;
    void drainUiTriggers(std::uint32_t eventTime) noexcept;
    bool captureMidiLearn(MidiBindingType, int number, int channel) noexcept;
    void recordMidiActivity(MidiBindingType, int number, int channel, int value, bool down) noexcept;
    void handleMidiEvent(const clap_event_midi_t&) noexcept;
    void handleNoteEvent(const clap_event_note_t&, bool down) noexcept;
    void handleParameterEvent(const clap_event_param_value_t&) noexcept;
    void handleTransportEvent(const clap_event_transport_t&) noexcept;
    void scheduleTrigger(int slot, bool down, std::uint32_t eventTime) noexcept;
    void executePendingTriggersAt(std::int64_t absoluteSample) noexcept;
    std::int64_t quantizedTargetSample(int slot, std::uint32_t eventTime,
        double* targetBeatOut = nullptr, const double* requestedBeat = nullptr) const noexcept;
    void applyUiTrigger(const UiTriggerEvent&, std::uint32_t eventTime) noexcept;
    bool pushUiTrigger(const UiTriggerEvent&) noexcept;
    bool popUiTrigger(UiTriggerEvent&) noexcept;
    UiEdit* uiEdit(clap_id id) noexcept;
    void flushUiEdits() noexcept;
    void requestMainService() noexcept;

    std::array<glitchdeck::ids::Slot, numSlots> ids_ { glitchdeck::ids::makeSlots() };
    GlitchEngine engine_;
    std::array<bool, numSlots> lastAutomationDown_ {}, heldUi_ {}, inputDown_ {}, desiredGate_ {};
    std::array<std::uint16_t, numSlots> heldCc_ {};
    std::array<HeldNote, 128> heldNotes_ {};
    std::array<std::atomic<bool>, numSlots> visibleActive_ {}, visiblePending_ {};
    std::array<PendingTrigger, numSlots> pendingTriggers_ {};
    std::array<std::uint32_t, numSlots> previousBinding_ {};
    std::array<int, numSlots> previousEffect_ {};
    std::array<bool, numSlots> previousLatch_ {};
    bool configsInitialized_ = false;

    // The queue preserves normal taps. On saturation, per-slot sequenced
    // mailboxes retain the newest edge, especially releases, without blocking.
    std::array<UiTriggerEvent, uiQueueCapacity> uiTriggerQueue_ {};
    std::atomic<std::size_t> uiQueueWrite_ { 0 }, uiQueueRead_ { 0 };
    std::array<std::atomic<std::uint64_t>, numSlots> uiOverflow_ {};
    std::array<std::uint64_t, numSlots> lastUiSequence_ {};
    std::array<std::uint8_t, numSlots> uiSources_ {}; // Main thread only.
    std::uint64_t uiSequence_ = 0;                 // Main thread only.
    std::array<UiEdit, 128> uiEdits_ {};            // Main thread only.
    std::atomic<bool> mainServiceRequested_ { false }, panicRequested_ { false }, stateResetRequested_ { false };

    // Generation and payload are published together, so cancel/re-arm cannot
    // accidentally apply an older learn result or mix its number and channel.
    std::uint32_t learnGeneration_ = 0; // Main thread only.
    std::atomic<std::uint64_t> midiLearnArm_ { 0 }, learnedBinding_ { 0 };
    std::atomic<std::uint32_t> midiActivity_ { 0 }, midiReceived_ { 0 }, midiMatched_ { 0 }, overflowCount_ { 0 };
    clap_event_transport_t transport_ {};
    bool hasTransport_ = false, blockPrepared_ = false;
    std::int64_t transportAnchorSample_ = 0, transportBlockStartSample_ = -1;
    std::int64_t streamSampleCounter_ = 0;
    double sampleRate_ = 44100.0;
};
