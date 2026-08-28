#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

#include <array>
#include <functional>
#include <memory>

class GlitchDeckAudioProcessorEditor : public juce::AudioProcessorEditor,
                                       private juce::Timer
{
public:
    explicit GlitchDeckAudioProcessorEditor(GlitchDeckAudioProcessor&);
    ~GlitchDeckAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    class TriggerPad : public juce::Component
    {
    public:
        TriggerPad(GlitchDeckAudioProcessor&, int slotIndex);
        void paint(juce::Graphics&) override;
        void mouseDown(const juce::MouseEvent&) override;
        void mouseUp(const juce::MouseEvent&) override;
        void mouseExit(const juce::MouseEvent&) override;

        void setSelected(bool shouldBeSelected);
        std::function<void(int)> onSelected;

    private:
        GlitchDeckAudioProcessor& processor;
        int slot = 0;
        bool selected = false;
        bool mouseHeld = false;
    };

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void timerCallback() override;
    void selectSlot(int slot);
    void bindSelectedSlot();
    void configureKnob(juce::Slider& slider, const juce::String& name);
    void configureCombo(juce::ComboBox& box, const juce::String& name);
    void releaseKeyboardTriggers();
    void refreshMidiLearnButton();

    GlitchDeckAudioProcessor& processor;
    juce::AudioProcessorValueTreeState& parameters;

    std::array<std::unique_ptr<TriggerPad>, GlitchDeckAudioProcessor::numSlots> pads;
    std::array<bool, GlitchDeckAudioProcessor::numSlots> keyboardDown {};
    int selectedSlot = 0;

    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::Label selectedLabel;

    juce::Slider globalMix;
    juce::Label globalMixLabel;
    std::unique_ptr<SliderAttachment> globalMixAttachment;

    juce::ComboBox effectBox;
    juce::ComboBox quantizeBox;
    juce::ComboBox stereoBox;
    juce::ToggleButton latchButton { "LATCH" };
    juce::TextButton midiLearnButton;

    juce::Slider intensitySlider;
    juce::Slider lengthSlider;
    juce::Slider attackSlider;
    juce::Slider releaseSlider;
    juce::Slider shapeSlider;

    juce::Label effectLabel;
    juce::Label quantizeLabel;
    juce::Label stereoLabel;
    juce::Label midiLabel;
    juce::Label intensityLabel;
    juce::Label lengthLabel;
    juce::Label attackLabel;
    juce::Label releaseLabel;
    juce::Label shapeLabel;

    std::unique_ptr<ComboAttachment> effectAttachment;
    std::unique_ptr<ComboAttachment> quantizeAttachment;
    std::unique_ptr<ComboAttachment> stereoAttachment;
    std::unique_ptr<ButtonAttachment> latchAttachment;
    std::unique_ptr<SliderAttachment> intensityAttachment;
    std::unique_ptr<SliderAttachment> lengthAttachment;
    std::unique_ptr<SliderAttachment> attackAttachment;
    std::unique_ptr<SliderAttachment> releaseAttachment;
    std::unique_ptr<SliderAttachment> shapeAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GlitchDeckAudioProcessorEditor)
};
