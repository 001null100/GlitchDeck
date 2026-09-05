#pragma once

#include "GlitchDeckPlugin.hpp"
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <functional>
#include <memory>

class GlitchDeckEditor final : public juce::Component, private juce::Timer
{
public:
    explicit GlitchDeckEditor(GlitchDeckPlugin& plugin);
    ~GlitchDeckEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override;
    void focusLost(FocusChangeType) override;
    void focusOfChildComponentChanged(FocusChangeType) override;
    bool keyPressed(const juce::KeyPress&) override;
    bool keyStateChanged(bool) override;

private:
    class TriggerPad final : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        TriggerPad(GlitchDeckPlugin& plugin, int slotIndex);
        void paint(juce::Graphics&) override;
        void mouseDown(const juce::MouseEvent&) override;
        void mouseUp(const juce::MouseEvent&) override;
        void setSelected(bool selected);
        void release();
        std::function<void(int)> onSelected;
    private:
        GlitchDeckPlugin& plugin_;
        int slot_ = 0;
        bool selected_ = false, mouseHeld_ = false;
    };
    void timerCallback() override;
    void selectSlot(int slot);
    void syncControlsFromPlugin();
    void refreshMidiLearnButton();
    void updateKeyboardTriggers();
    void releaseKeyboardTriggers();
    void releaseHeldInputs();
    void panic();
    void configureKnob(juce::Slider& slider);
    void configureCombo(juce::ComboBox& box);
    void bindSlider(juce::Slider& slider, std::function<clap_id()> parameter);
    void setOneShot(clap_id id, double value);

    GlitchDeckPlugin& plugin_;
    std::array<std::unique_ptr<TriggerPad>, GlitchDeckPlugin::numSlots> pads_;
    std::array<bool, GlitchDeckPlugin::numSlots> keyboardDown_ {};
    bool keyboardSuppressed_ = false, syncing_ = false;
    int selectedSlot_ = 0;
    juce::Rectangle<int> editPanel_;
    juce::TooltipWindow tooltips_ { this, 650 };
    juce::Label titleLabel_, subtitleLabel_, selectedLabel_, statusLabel_, helpLabel_;
    juce::Slider globalMix_;
    juce::Label globalMixLabel_;
    juce::ComboBox effectBox_, quantizeBox_, stereoBox_, midiTypeBox_, midiChannelBox_;
    juce::Slider midiNumberSlider_;
    juce::ToggleButton latchButton_ { "LATCH" };
    juce::TextButton midiLearnButton_, panicButton_ { "PANIC / ESC" };
    juce::Slider intensitySlider_, lengthSlider_, attackSlider_, releaseSlider_, shapeSlider_;
    juce::Label effectLabel_, quantizeLabel_, stereoLabel_, midiLabel_, midiNumberLabel_, midiChannelLabel_;
    juce::Label intensityLabel_, lengthLabel_, attackLabel_, releaseLabel_, shapeLabel_;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GlitchDeckEditor)
};
