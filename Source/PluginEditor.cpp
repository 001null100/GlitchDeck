#include "PluginEditor.h"

#include <algorithm>
#include <cmath>

namespace
{
const auto background = juce::Colour(0xff0d1016);
const auto panel = juce::Colour(0xff151a23);
const auto panelRaised = juce::Colour(0xff1d2430);
const auto text = juce::Colour(0xffeef3f8);
const auto muted = juce::Colour(0xff8290a3);
const auto accent = juce::Colour(0xff55d9ff);
const auto hot = juce::Colour(0xffff4fc8);
const auto activeFill = juce::Colour(0xff26384a);

void styleLabel(juce::Label& label, const juce::String& caption)
{
    label.setText(caption, juce::dontSendNotification);
    label.setColour(juce::Label::textColourId, muted);
    label.setJustificationType(juce::Justification::centredLeft);
    label.setFont(juce::Font(12.0f, juce::Font::bold));
}
}

GlitchDeckAudioProcessorEditor::TriggerPad::TriggerPad(GlitchDeckAudioProcessor& p, int slotIndex)
    : processor(p), slot(slotIndex)
{
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

void GlitchDeckAudioProcessorEditor::TriggerPad::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced(1.0f);
    const auto isActive = processor.isSlotActive(slot);

    g.setColour(isActive ? activeFill : panelRaised);
    g.fillRoundedRectangle(bounds, 10.0f);

    g.setColour(isActive ? hot : (selected ? accent : juce::Colour(0xff2d3745)));
    g.drawRoundedRectangle(bounds, 10.0f, selected ? 2.0f : 1.0f);

    auto content = getLocalBounds().reduced(15);
    auto top = content.removeFromTop(24);

    g.setColour(isActive ? hot : muted);
    g.setFont(juce::Font(12.0f, juce::Font::bold));
    g.drawText(juce::String(slot + 1), top.removeFromLeft(32), juce::Justification::centredLeft);

    const auto midi = processor.getMidiNoteForSlot(slot);
    g.drawText(GlitchDeckAudioProcessor::midiNoteName(midi), top, juce::Justification::centredRight);

    content.removeFromTop(10);
    g.setColour(text);
    g.setFont(juce::Font(21.0f, juce::Font::bold));
    g.drawText(processor.getEffectNameForSlot(slot).toUpperCase(), content.removeFromTop(32),
               juce::Justification::centredLeft, true);

    auto& state = processor.getValueTreeState();
    const auto quantizeIndex = static_cast<int>(std::round(
        state.getRawParameterValue(GlitchDeckAudioProcessor::slotParameterId(slot, "quantize"))->load()));
    const auto latch = state.getRawParameterValue(GlitchDeckAudioProcessor::slotParameterId(slot, "latch"))->load() >= 0.5f;
    const auto length = state.getRawParameterValue(GlitchDeckAudioProcessor::slotParameterId(slot, "length"))->load();

    g.setColour(muted);
    g.setFont(juce::Font(12.0f));
    auto details = GlitchDeckAudioProcessor::quantizeNames()[juce::jlimit(0, 5, quantizeIndex)];
    details += "  |  " + juce::String(length, length < 100.0f ? 1 : 0) + " ms";
    if (latch)
        details += "  |  LATCH";
    g.drawText(details, content.removeFromTop(24), juce::Justification::centredLeft, true);

    if (isActive)
    {
        auto light = juce::Rectangle<float>(bounds.getRight() - 20.0f, bounds.getBottom() - 20.0f, 7.0f, 7.0f);
        g.setColour(hot);
        g.fillEllipse(light);
    }
}

void GlitchDeckAudioProcessorEditor::TriggerPad::mouseDown(const juce::MouseEvent&)
{
    if (onSelected)
        onSelected(slot);

    mouseHeld = true;
    processor.setTriggerParameterFromUI(slot, true);
    repaint();
}

void GlitchDeckAudioProcessorEditor::TriggerPad::mouseUp(const juce::MouseEvent&)
{
    if (! mouseHeld)
        return;

    mouseHeld = false;
    processor.setTriggerParameterFromUI(slot, false);
    repaint();
}

void GlitchDeckAudioProcessorEditor::TriggerPad::mouseExit(const juce::MouseEvent&)
{
    // JUCE keeps mouse capture for a pressed component, so mouseUp still arrives
    // after dragging outside. Do not synthesize a fake MouseEvent here.
}

void GlitchDeckAudioProcessorEditor::TriggerPad::setSelected(bool shouldBeSelected)
{
    if (selected != shouldBeSelected)
    {
        selected = shouldBeSelected;
        repaint();
    }
}

GlitchDeckAudioProcessorEditor::GlitchDeckAudioProcessorEditor(GlitchDeckAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p), parameters(p.getValueTreeState())
{
    setSize(980, 660);
    setResizable(true, true);
    setResizeLimits(820, 570, 1500, 1000);
    setWantsKeyboardFocus(true);
    setOpaque(true);

    titleLabel.setText("GLITCHDECK", juce::dontSendNotification);
    titleLabel.setColour(juce::Label::textColourId, text);
    titleLabel.setFont(juce::Font(27.0f, juce::Font::bold));
    addAndMakeVisible(titleLabel);

    subtitleLabel.setText("PLAY THE DAMAGE", juce::dontSendNotification);
    subtitleLabel.setColour(juce::Label::textColourId, muted);
    subtitleLabel.setFont(juce::Font(11.0f, juce::Font::bold));
    addAndMakeVisible(subtitleLabel);

    styleLabel(globalMixLabel, "GLOBAL MIX");
    addAndMakeVisible(globalMixLabel);
    globalMix.setSliderStyle(juce::Slider::LinearHorizontal);
    globalMix.setTextBoxStyle(juce::Slider::TextBoxRight, false, 58, 22);
    globalMix.setColour(juce::Slider::trackColourId, accent);
    globalMix.setColour(juce::Slider::thumbColourId, text);
    globalMix.textFromValueFunction = [](double value) { return juce::String(std::round(value * 100.0)) + "%"; };
    addAndMakeVisible(globalMix);
    globalMixAttachment = std::make_unique<SliderAttachment>(parameters, "mix", globalMix);

    for (int i = 0; i < GlitchDeckAudioProcessor::numSlots; ++i)
    {
        pads[static_cast<size_t>(i)] = std::make_unique<TriggerPad>(processor, i);
        pads[static_cast<size_t>(i)]->onSelected = [this](int slot) { selectSlot(slot); };
        addAndMakeVisible(*pads[static_cast<size_t>(i)]);
    }

    selectedLabel.setColour(juce::Label::textColourId, text);
    selectedLabel.setFont(juce::Font(15.0f, juce::Font::bold));
    addAndMakeVisible(selectedLabel);

    configureCombo(effectBox, "EFFECT");
    configureCombo(quantizeBox, "QUANTIZE");
    configureCombo(stereoBox, "STEREO");
    effectBox.addItemList(GlitchDeckAudioProcessor::effectNames(), 1);
    quantizeBox.addItemList(GlitchDeckAudioProcessor::quantizeNames(), 1);
    stereoBox.addItemList(GlitchDeckAudioProcessor::stereoNames(), 1);

    styleLabel(effectLabel, "EFFECT");
    styleLabel(quantizeLabel, "QUANTIZE");
    styleLabel(stereoLabel, "STEREO");
    styleLabel(midiLabel, "MIDI NOTE");
    styleLabel(intensityLabel, "INTENSITY");
    styleLabel(lengthLabel, "LENGTH");
    styleLabel(attackLabel, "ATTACK");
    styleLabel(releaseLabel, "RELEASE");
    styleLabel(shapeLabel, "SHAPE");

    for (auto* label : { &effectLabel, &quantizeLabel, &stereoLabel, &midiLabel,
                         &intensityLabel, &lengthLabel, &attackLabel, &releaseLabel, &shapeLabel })
        addAndMakeVisible(*label);

    latchButton.setColour(juce::ToggleButton::textColourId, text);
    latchButton.setColour(juce::ToggleButton::tickColourId, hot);
    latchButton.setColour(juce::ToggleButton::tickDisabledColourId, muted);
    addAndMakeVisible(latchButton);

    midiNoteSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    midiNoteSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 86, 22);
    midiNoteSlider.setColour(juce::Slider::trackColourId, accent);
    midiNoteSlider.setColour(juce::Slider::thumbColourId, text);
    midiNoteSlider.textFromValueFunction = [](double value)
    {
        const auto note = juce::jlimit(0, 127, static_cast<int>(std::round(value)));
        return GlitchDeckAudioProcessor::midiNoteName(note) + "  " + juce::String(note);
    };
    addAndMakeVisible(midiNoteSlider);

    configureKnob(intensitySlider, "INTENSITY");
    configureKnob(lengthSlider, "LENGTH");
    configureKnob(attackSlider, "ATTACK");
    configureKnob(releaseSlider, "RELEASE");
    configureKnob(shapeSlider, "SHAPE");

    intensitySlider.textFromValueFunction = [](double value) { return juce::String(std::round(value * 100.0)) + "%"; };
    shapeSlider.textFromValueFunction = [](double value) { return juce::String(std::round(value * 100.0)) + "%"; };
    lengthSlider.textFromValueFunction = [](double value) { return juce::String(value, value < 100.0 ? 1 : 0) + " ms"; };
    attackSlider.textFromValueFunction = [](double value) { return juce::String(value, value < 10.0 ? 1 : 0) + " ms"; };
    releaseSlider.textFromValueFunction = [](double value) { return juce::String(value, value < 10.0 ? 1 : 0) + " ms"; };

    selectSlot(0);
    startTimerHz(30);
    grabKeyboardFocus();
}

GlitchDeckAudioProcessorEditor::~GlitchDeckAudioProcessorEditor()
{
    stopTimer();
    releaseKeyboardTriggers();
}

void GlitchDeckAudioProcessorEditor::configureKnob(juce::Slider& slider, const juce::String&)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 76, 22);
    slider.setColour(juce::Slider::rotarySliderFillColourId, accent);
    slider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff293442));
    slider.setColour(juce::Slider::thumbColourId, text);
    slider.setColour(juce::Slider::textBoxTextColourId, text);
    slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible(slider);
}

void GlitchDeckAudioProcessorEditor::configureCombo(juce::ComboBox& box, const juce::String&)
{
    box.setColour(juce::ComboBox::backgroundColourId, panelRaised);
    box.setColour(juce::ComboBox::textColourId, text);
    box.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff34404f));
    box.setColour(juce::ComboBox::arrowColourId, accent);
    addAndMakeVisible(box);
}

void GlitchDeckAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(background);

    auto deckPanel = getLocalBounds().toFloat().reduced(18.0f);
    deckPanel.removeFromTop(62.0f);
    auto editPanel = deckPanel.removeFromBottom(272.0f);

    g.setColour(panel);
    g.fillRoundedRectangle(editPanel, 12.0f);
    g.setColour(juce::Colour(0xff252f3c));
    g.drawRoundedRectangle(editPanel, 12.0f, 1.0f);

    g.setColour(selectedSlot % 2 == 0 ? accent.withAlpha(0.8f) : hot.withAlpha(0.75f));
    g.fillRoundedRectangle(editPanel.getX(), editPanel.getY(), 5.0f, editPanel.getHeight(), 2.5f);
}

void GlitchDeckAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(20);
    auto header = area.removeFromTop(58);

    auto titleArea = header.removeFromLeft(300);
    titleLabel.setBounds(titleArea.removeFromTop(35));
    subtitleLabel.setBounds(titleArea.removeFromTop(18));

    auto mixArea = header.removeFromRight(270);
    globalMixLabel.setBounds(mixArea.removeFromTop(18));
    globalMix.setBounds(mixArea.removeFromTop(32));

    area.removeFromTop(6);
    auto padArea = area.removeFromTop(std::max(220, static_cast<int>(area.getHeight() * 0.48f)));
    constexpr int columns = 4;
    constexpr int rows = 2;
    constexpr int gap = 10;
    const auto padWidth = (padArea.getWidth() - gap * (columns - 1)) / columns;
    const auto padHeight = (padArea.getHeight() - gap * (rows - 1)) / rows;

    for (int i = 0; i < GlitchDeckAudioProcessor::numSlots; ++i)
    {
        const auto column = i % columns;
        const auto row = i / columns;
        pads[static_cast<size_t>(i)]->setBounds(
            padArea.getX() + column * (padWidth + gap),
            padArea.getY() + row * (padHeight + gap),
            padWidth, padHeight);
    }

    area.removeFromTop(12);
    auto editor = area;
    editor.removeFromLeft(14);
    editor.removeFromRight(14);
    editor.removeFromTop(8);
    selectedLabel.setBounds(editor.removeFromTop(28));

    auto strip = editor.removeFromTop(72);
    const auto stripGap = 10;
    const auto firstWidth = std::max(125, (strip.getWidth() - stripGap * 4) / 5);

    auto placeField = [&](juce::Label& label, juce::Component& component, int width)
    {
        auto cell = strip.removeFromLeft(width);
        label.setBounds(cell.removeFromTop(18));
        component.setBounds(cell.removeFromTop(34));
        strip.removeFromLeft(stripGap);
    };

    placeField(effectLabel, effectBox, firstWidth);
    placeField(quantizeLabel, quantizeBox, firstWidth);
    placeField(stereoLabel, stereoBox, firstWidth);
    placeField(midiLabel, midiNoteSlider, firstWidth + 20);

    auto latchCell = strip;
    latchCell.removeFromTop(17);
    latchButton.setBounds(latchCell.removeFromTop(35));

    editor.removeFromTop(6);
    auto knobs = editor;
    const auto knobGap = 10;
    const auto knobWidth = (knobs.getWidth() - knobGap * 4) / 5;

    auto placeKnob = [&](juce::Label& label, juce::Slider& slider)
    {
        auto cell = knobs.removeFromLeft(knobWidth);
        label.setBounds(cell.removeFromTop(18));
        slider.setBounds(cell);
        knobs.removeFromLeft(knobGap);
    };

    placeKnob(intensityLabel, intensitySlider);
    placeKnob(lengthLabel, lengthSlider);
    placeKnob(attackLabel, attackSlider);
    placeKnob(releaseLabel, releaseSlider);
    placeKnob(shapeLabel, shapeSlider);
}

void GlitchDeckAudioProcessorEditor::selectSlot(int slot)
{
    selectedSlot = juce::jlimit(0, GlitchDeckAudioProcessor::numSlots - 1, slot);
    for (int i = 0; i < GlitchDeckAudioProcessor::numSlots; ++i)
        pads[static_cast<size_t>(i)]->setSelected(i == selectedSlot);

    bindSelectedSlot();
    repaint();
}

void GlitchDeckAudioProcessorEditor::bindSelectedSlot()
{
    effectAttachment.reset();
    quantizeAttachment.reset();
    stereoAttachment.reset();
    latchAttachment.reset();
    midiAttachment.reset();
    intensityAttachment.reset();
    lengthAttachment.reset();
    attackAttachment.reset();
    releaseAttachment.reset();
    shapeAttachment.reset();

    selectedLabel.setText("TRIGGER " + juce::String(selectedSlot + 1) + "  /  "
                              + processor.getEffectNameForSlot(selectedSlot).toUpperCase(),
                          juce::dontSendNotification);

    effectAttachment = std::make_unique<ComboAttachment>(parameters,
        GlitchDeckAudioProcessor::slotParameterId(selectedSlot, "effect"), effectBox);
    quantizeAttachment = std::make_unique<ComboAttachment>(parameters,
        GlitchDeckAudioProcessor::slotParameterId(selectedSlot, "quantize"), quantizeBox);
    stereoAttachment = std::make_unique<ComboAttachment>(parameters,
        GlitchDeckAudioProcessor::slotParameterId(selectedSlot, "stereo"), stereoBox);
    latchAttachment = std::make_unique<ButtonAttachment>(parameters,
        GlitchDeckAudioProcessor::slotParameterId(selectedSlot, "latch"), latchButton);
    midiAttachment = std::make_unique<SliderAttachment>(parameters,
        GlitchDeckAudioProcessor::slotParameterId(selectedSlot, "midi"), midiNoteSlider);
    intensityAttachment = std::make_unique<SliderAttachment>(parameters,
        GlitchDeckAudioProcessor::slotParameterId(selectedSlot, "intensity"), intensitySlider);
    lengthAttachment = std::make_unique<SliderAttachment>(parameters,
        GlitchDeckAudioProcessor::slotParameterId(selectedSlot, "length"), lengthSlider);
    attackAttachment = std::make_unique<SliderAttachment>(parameters,
        GlitchDeckAudioProcessor::slotParameterId(selectedSlot, "attack"), attackSlider);
    releaseAttachment = std::make_unique<SliderAttachment>(parameters,
        GlitchDeckAudioProcessor::slotParameterId(selectedSlot, "release"), releaseSlider);
    shapeAttachment = std::make_unique<SliderAttachment>(parameters,
        GlitchDeckAudioProcessor::slotParameterId(selectedSlot, "shape"), shapeSlider);
}

void GlitchDeckAudioProcessorEditor::timerCallback()
{
    for (auto& pad : pads)
        pad->repaint();

    selectedLabel.setText("TRIGGER " + juce::String(selectedSlot + 1) + "  /  "
                              + processor.getEffectNameForSlot(selectedSlot).toUpperCase(),
                          juce::dontSendNotification);

    bool textEditorHasFocus = false;
    if (auto* focused = juce::Component::getCurrentlyFocusedComponent())
    {
        for (auto* component = focused; component != nullptr; component = component->getParentComponent())
        {
            if (dynamic_cast<juce::TextEditor*>(component) != nullptr)
            {
                textEditorHasFocus = true;
                break;
            }
        }
    }

    if (! hasKeyboardFocus(true) || textEditorHasFocus)
    {
        releaseKeyboardTriggers();
        return;
    }

    for (int i = 0; i < GlitchDeckAudioProcessor::numSlots; ++i)
    {
        const auto down = juce::KeyPress::isKeyCurrentlyDown('1' + i);
        if (down != keyboardDown[static_cast<size_t>(i)])
        {
            keyboardDown[static_cast<size_t>(i)] = down;
            processor.setTriggerParameterFromUI(i, down);
        }
    }
}

void GlitchDeckAudioProcessorEditor::releaseKeyboardTriggers()
{
    for (int i = 0; i < GlitchDeckAudioProcessor::numSlots; ++i)
    {
        if (keyboardDown[static_cast<size_t>(i)])
        {
            keyboardDown[static_cast<size_t>(i)] = false;
            processor.setTriggerParameterFromUI(i, false);
        }
    }
}
