#pragma once

#include "GlitchDeckPlugin.hpp"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <functional>
#include <memory>

class GlitchDeckEditor final : public juce::Component,
                               private juce::Timer
{
public:
    explicit GlitchDeckEditor(GlitchDeckPlugin& plugin);
    ~GlitchDeckEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    class TriggerPad final : public juce::Component
    {
    public:
        TriggerPad(GlitchDeckPlugin& plugin, int slotIndex);
        void paint(juce::Graphics&) override;
        void mouseDown(const juce::MouseEvent&) override;
        void mouseUp(const juce::MouseEvent&) override;
        void setSelected(bool selected);

        std::function<void(int)> onSelected;

    private:
        GlitchDeckPlugin& plugin_;
        int slot_ = 0;
        bool selected_ = false;
        bool mouseHeld_ = false;
    };

    void timerCallback() override;
    void selectSlot(int slot);
    void syncControlsFromPlugin();
    void refreshMidiLearnButton();
    void releaseKeyboardTriggers();
    void configureKnob(juce::Slider& slider);
    void configureCombo(juce::ComboBox& box);
    void setOneShot(clap_id id, double value);

    GlitchDeckPlugin& plugin_;
    std::array<std::unique_ptr<TriggerPad>, GlitchDeckPlugin::numSlots> pads_;
    std::array<bool, GlitchDeckPlugin::numSlots> keyboardDown_ {};
    int selectedSlot_ = 0;
    bool syncing_ = false;

    juce::Label titleLabel_;
    juce::Label subtitleLabel_;
    juce::Label selectedLabel_;

    juce::Slider globalMix_;
    juce::Label globalMixLabel_;

    juce::ComboBox effectBox_;
    juce::ComboBox quantizeBox_;
    juce::ComboBox stereoBox_;
    juce::ToggleButton latchButton_ { "LATCH" };
    juce::TextButton midiLearnButton_;

    juce::Slider intensitySlider_;
    juce::Slider lengthSlider_;
    juce::Slider attackSlider_;
    juce::Slider releaseSlider_;
    juce::Slider shapeSlider_;

    juce::Label effectLabel_;
    juce::Label quantizeLabel_;
    juce::Label stereoLabel_;
    juce::Label midiLabel_;
    juce::Label intensityLabel_;
    juce::Label lengthLabel_;
    juce::Label attackLabel_;
    juce::Label releaseLabel_;
    juce::Label shapeLabel_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GlitchDeckEditor)
};
