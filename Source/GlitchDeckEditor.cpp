#include "GlitchDeckEditor.hpp"

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

juce::String toJuce(const std::string& value)
{
    return juce::String::fromUTF8(value.c_str());
}
}

GlitchDeckEditor::TriggerPad::TriggerPad(GlitchDeckPlugin& plugin, int slotIndex)
    : plugin_(plugin), slot_(slotIndex)
{
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

void GlitchDeckEditor::TriggerPad::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced(1.0f);
    const auto active = plugin_.isSlotActive(slot_);

    g.setColour(active ? activeFill : panelRaised);
    g.fillRoundedRectangle(bounds, 10.0f);
    g.setColour(active ? hot : (selected_ ? accent : juce::Colour(0xff2d3745)));
    g.drawRoundedRectangle(bounds, 10.0f, selected_ ? 2.0f : 1.0f);

    auto content = getLocalBounds().reduced(15);
    auto top = content.removeFromTop(24);
    g.setColour(active ? hot : muted);
    g.setFont(juce::Font(12.0f, juce::Font::bold));
    g.drawText(juce::String(slot_ + 1), top.removeFromLeft(32), juce::Justification::centredLeft);
    g.drawText(toJuce(plugin_.midiBindingText(slot_)), top, juce::Justification::centredRight, true);

    content.removeFromTop(10);
    g.setColour(text);
    g.setFont(juce::Font(21.0f, juce::Font::bold));
    g.drawText(toJuce(plugin_.effectName(slot_)).toUpperCase(), content.removeFromTop(32),
               juce::Justification::centredLeft, true);

    const auto& ids = plugin_.slotIds(slot_);
    const auto quantize = std::clamp(plugin_.parameterInt(ids.quantize), 0, 5);
    const auto latch = plugin_.parameterBool(ids.latch);
    const auto length = plugin_.parameterValue(ids.length);

    g.setColour(muted);
    g.setFont(juce::Font(12.0f));
    juce::String details(GlitchDeckPlugin::quantizeNames()[static_cast<std::size_t>(quantize)]);
    details += "  |  " + juce::String(length, length < 100.0 ? 1 : 0) + " ms";
    if (latch)
        details += "  |  LATCH";
    g.drawText(details, content.removeFromTop(24), juce::Justification::centredLeft, true);

    if (active)
    {
        const auto light = juce::Rectangle<float>(bounds.getRight() - 20.0f, bounds.getBottom() - 20.0f, 7.0f, 7.0f);
        g.setColour(hot);
        g.fillEllipse(light);
    }
}

void GlitchDeckEditor::TriggerPad::mouseDown(const juce::MouseEvent& event)
{
    if (onSelected)
        onSelected(slot_);

    // Right-click is a safe selection gesture for editing during playback.
    if (!event.mods.isLeftButtonDown())
        return;

    mouseHeld_ = true;
    plugin_.setTriggerFromUi(slot_, true);
    repaint();
}

void GlitchDeckEditor::TriggerPad::mouseUp(const juce::MouseEvent&)
{
    if (!mouseHeld_)
        return;
    mouseHeld_ = false;
    plugin_.setTriggerFromUi(slot_, false);
    repaint();
}

void GlitchDeckEditor::TriggerPad::setSelected(bool selected)
{
    if (selected_ == selected)
        return;
    selected_ = selected;
    repaint();
}

GlitchDeckEditor::GlitchDeckEditor(GlitchDeckPlugin& plugin)
    : plugin_(plugin)
{
    setWantsKeyboardFocus(true);
    setOpaque(true);

    titleLabel_.setText("GLITCHDECK", juce::dontSendNotification);
    titleLabel_.setColour(juce::Label::textColourId, text);
    titleLabel_.setFont(juce::Font(27.0f, juce::Font::bold));
    addAndMakeVisible(titleLabel_);

    subtitleLabel_.setText("PLAY THE DAMAGE", juce::dontSendNotification);
    subtitleLabel_.setColour(juce::Label::textColourId, muted);
    subtitleLabel_.setFont(juce::Font(11.0f, juce::Font::bold));
    addAndMakeVisible(subtitleLabel_);

    styleLabel(globalMixLabel_, "GLOBAL MIX");
    addAndMakeVisible(globalMixLabel_);
    globalMix_.setSliderStyle(juce::Slider::LinearHorizontal);
    globalMix_.setRange(0.0, 1.0, 0.001);
    globalMix_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 58, 22);
    globalMix_.setColour(juce::Slider::trackColourId, accent);
    globalMix_.setColour(juce::Slider::thumbColourId, text);
    globalMix_.textFromValueFunction = [](double value) { return juce::String(std::round(value * 100.0)) + "%"; };
    globalMix_.onDragStart = [this] { plugin_.beginParameterGesture(glitchdeck::ids::mix); };
    globalMix_.onValueChange = [this]
    {
        if (!syncing_)
            plugin_.setParameterFromGui(glitchdeck::ids::mix, globalMix_.getValue());
    };
    globalMix_.onDragEnd = [this] { plugin_.endParameterGesture(glitchdeck::ids::mix); };
    addAndMakeVisible(globalMix_);

    for (int i = 0; i < GlitchDeckPlugin::numSlots; ++i)
    {
        pads_[static_cast<std::size_t>(i)] = std::make_unique<TriggerPad>(plugin_, i);
        pads_[static_cast<std::size_t>(i)]->onSelected = [this](int slot) { selectSlot(slot); };
        addAndMakeVisible(*pads_[static_cast<std::size_t>(i)]);
    }

    selectedLabel_.setColour(juce::Label::textColourId, text);
    selectedLabel_.setFont(juce::Font(15.0f, juce::Font::bold));
    addAndMakeVisible(selectedLabel_);

    configureCombo(effectBox_);
    configureCombo(quantizeBox_);
    configureCombo(stereoBox_);
    for (int i = 0; i < static_cast<int>(GlitchDeckPlugin::effectNames().size()); ++i)
        effectBox_.addItem(GlitchDeckPlugin::effectNames()[static_cast<std::size_t>(i)], i + 1);
    for (int i = 0; i < static_cast<int>(GlitchDeckPlugin::quantizeNames().size()); ++i)
        quantizeBox_.addItem(GlitchDeckPlugin::quantizeNames()[static_cast<std::size_t>(i)], i + 1);
    for (int i = 0; i < static_cast<int>(GlitchDeckPlugin::stereoNames().size()); ++i)
        stereoBox_.addItem(GlitchDeckPlugin::stereoNames()[static_cast<std::size_t>(i)], i + 1);

    effectBox_.onChange = [this]
    {
        if (!syncing_ && effectBox_.getSelectedId() > 0)
            setOneShot(plugin_.slotIds(selectedSlot_).effect, effectBox_.getSelectedId() - 1);
    };
    quantizeBox_.onChange = [this]
    {
        if (!syncing_ && quantizeBox_.getSelectedId() > 0)
            setOneShot(plugin_.slotIds(selectedSlot_).quantize, quantizeBox_.getSelectedId() - 1);
    };
    stereoBox_.onChange = [this]
    {
        if (!syncing_ && stereoBox_.getSelectedId() > 0)
            setOneShot(plugin_.slotIds(selectedSlot_).stereo, stereoBox_.getSelectedId() - 1);
    };

    styleLabel(effectLabel_, "EFFECT");
    styleLabel(quantizeLabel_, "QUANTIZE");
    styleLabel(stereoLabel_, "STEREO");
    styleLabel(midiLabel_, "MIDI IN");
    styleLabel(intensityLabel_, "INTENSITY");
    styleLabel(lengthLabel_, "LENGTH");
    styleLabel(attackLabel_, "ATTACK");
    styleLabel(releaseLabel_, "RELEASE");
    styleLabel(shapeLabel_, "SHAPE");
    for (auto* label : { &effectLabel_, &quantizeLabel_, &stereoLabel_, &midiLabel_, &intensityLabel_,
                         &lengthLabel_, &attackLabel_, &releaseLabel_, &shapeLabel_ })
        addAndMakeVisible(*label);

    latchButton_.setColour(juce::ToggleButton::textColourId, text);
    latchButton_.setColour(juce::ToggleButton::tickColourId, hot);
    latchButton_.onClick = [this]
    {
        if (!syncing_)
            setOneShot(plugin_.slotIds(selectedSlot_).latch, latchButton_.getToggleState() ? 1.0 : 0.0);
    };
    addAndMakeVisible(latchButton_);

    midiLearnButton_.setColour(juce::TextButton::buttonColourId, panelRaised);
    midiLearnButton_.setColour(juce::TextButton::buttonOnColourId, hot.withAlpha(0.25f));
    midiLearnButton_.setColour(juce::TextButton::textColourOffId, text);
    midiLearnButton_.setColour(juce::TextButton::textColourOnId, hot);
    midiLearnButton_.setWantsKeyboardFocus(false);
    midiLearnButton_.onClick = [this]
    {
        plugin_.toggleMidiLearn(selectedSlot_);
        refreshMidiLearnButton();
    };
    addAndMakeVisible(midiLearnButton_);

    for (auto* slider : { &intensitySlider_, &lengthSlider_, &attackSlider_, &releaseSlider_, &shapeSlider_ })
        configureKnob(*slider);

    intensitySlider_.setRange(0.0, 1.0, 0.001);
    lengthSlider_.setRange(2.0, 1500.0, 0.01);
    lengthSlider_.setSkewFactor(0.35);
    attackSlider_.setRange(0.1, 100.0, 0.01);
    attackSlider_.setSkewFactor(0.4);
    releaseSlider_.setRange(0.1, 300.0, 0.01);
    releaseSlider_.setSkewFactor(0.4);
    shapeSlider_.setRange(0.0, 1.0, 0.001);

    intensitySlider_.textFromValueFunction = [](double value) { return juce::String(std::round(value * 100.0)) + "%"; };
    shapeSlider_.textFromValueFunction = [](double value) { return juce::String(std::round(value * 100.0)) + "%"; };
    lengthSlider_.textFromValueFunction = [](double value) { return juce::String(value, value < 100.0 ? 1 : 0) + " ms"; };
    attackSlider_.textFromValueFunction = [](double value) { return juce::String(value, value < 10.0 ? 1 : 0) + " ms"; };
    releaseSlider_.textFromValueFunction = [](double value) { return juce::String(value, value < 10.0 ? 1 : 0) + " ms"; };

    auto bindSlotSlider = [this](juce::Slider& slider, auto idForSlot)
    {
        slider.onDragStart = [this, idForSlot]
        {
            plugin_.beginParameterGesture(idForSlot(plugin_.slotIds(selectedSlot_)));
        };
        slider.onValueChange = [this, &slider, idForSlot]
        {
            if (!syncing_)
                plugin_.setParameterFromGui(idForSlot(plugin_.slotIds(selectedSlot_)), slider.getValue());
        };
        slider.onDragEnd = [this, idForSlot]
        {
            plugin_.endParameterGesture(idForSlot(plugin_.slotIds(selectedSlot_)));
        };
    };

    bindSlotSlider(intensitySlider_, [](const auto& ids) { return ids.intensity; });
    bindSlotSlider(lengthSlider_, [](const auto& ids) { return ids.length; });
    bindSlotSlider(attackSlider_, [](const auto& ids) { return ids.attack; });
    bindSlotSlider(releaseSlider_, [](const auto& ids) { return ids.release; });
    bindSlotSlider(shapeSlider_, [](const auto& ids) { return ids.shape; });

    selectSlot(0);
    setSize(980, 660);
    startTimerHz(30);
}

GlitchDeckEditor::~GlitchDeckEditor()
{
    stopTimer();
    releaseKeyboardTriggers();
}

void GlitchDeckEditor::configureKnob(juce::Slider& slider)
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

void GlitchDeckEditor::configureCombo(juce::ComboBox& box)
{
    box.setColour(juce::ComboBox::backgroundColourId, panelRaised);
    box.setColour(juce::ComboBox::textColourId, text);
    box.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff34404f));
    box.setColour(juce::ComboBox::arrowColourId, accent);
    addAndMakeVisible(box);
}

void GlitchDeckEditor::setOneShot(clap_id id, double value)
{
    plugin_.beginParameterGesture(id);
    plugin_.setParameterFromGui(id, value);
    plugin_.endParameterGesture(id);
}

void GlitchDeckEditor::paint(juce::Graphics& g)
{
    g.fillAll(background);
    auto deckPanel = getLocalBounds().toFloat().reduced(18.0f);
    deckPanel.removeFromTop(62.0f);
    auto editPanel = deckPanel.removeFromBottom(272.0f);
    g.setColour(panel);
    g.fillRoundedRectangle(editPanel, 12.0f);
    g.setColour(juce::Colour(0xff252f3c));
    g.drawRoundedRectangle(editPanel, 12.0f, 1.0f);
    g.setColour(selectedSlot_ % 2 == 0 ? accent.withAlpha(0.8f) : hot.withAlpha(0.75f));
    g.fillRoundedRectangle(editPanel.getX(), editPanel.getY(), 5.0f, editPanel.getHeight(), 2.5f);
}

void GlitchDeckEditor::resized()
{
    auto area = getLocalBounds().reduced(20);
    auto header = area.removeFromTop(58);
    auto titleArea = header.removeFromLeft(300);
    titleLabel_.setBounds(titleArea.removeFromTop(35));
    subtitleLabel_.setBounds(titleArea.removeFromTop(18));

    auto mixArea = header.removeFromRight(270);
    globalMixLabel_.setBounds(mixArea.removeFromTop(18));
    globalMix_.setBounds(mixArea.removeFromTop(32));

    area.removeFromTop(6);
    auto padArea = area.removeFromTop(std::max(220, static_cast<int>(area.getHeight() * 0.48f)));
    constexpr int columns = 4;
    constexpr int gap = 10;
    const auto padWidth = (padArea.getWidth() - gap * (columns - 1)) / columns;
    const auto padHeight = (padArea.getHeight() - gap) / 2;

    for (int i = 0; i < GlitchDeckPlugin::numSlots; ++i)
    {
        if (pads_[static_cast<std::size_t>(i)] == nullptr)
            continue;
        const auto column = i % columns;
        const auto row = i / columns;
        pads_[static_cast<std::size_t>(i)]->setBounds(
            padArea.getX() + column * (padWidth + gap),
            padArea.getY() + row * (padHeight + gap), padWidth, padHeight);
    }

    area.removeFromTop(12);
    auto editor = area;
    editor.removeFromLeft(14);
    editor.removeFromRight(14);
    editor.removeFromTop(8);
    selectedLabel_.setBounds(editor.removeFromTop(28));

    auto strip = editor.removeFromTop(72);
    constexpr int stripGap = 10;
    const auto firstWidth = std::max(118, (strip.getWidth() - stripGap * 4) / 5);
    auto placeField = [&](juce::Label& label, juce::Component& component, int width)
    {
        auto cell = strip.removeFromLeft(width);
        label.setBounds(cell.removeFromTop(18));
        component.setBounds(cell.removeFromTop(34));
        strip.removeFromLeft(stripGap);
    };
    placeField(effectLabel_, effectBox_, firstWidth);
    placeField(quantizeLabel_, quantizeBox_, firstWidth);
    placeField(stereoLabel_, stereoBox_, firstWidth);
    placeField(midiLabel_, midiLearnButton_, firstWidth + 50);
    auto latchCell = strip;
    latchCell.removeFromTop(17);
    latchButton_.setBounds(latchCell.removeFromTop(35));

    editor.removeFromTop(6);
    auto knobs = editor;
    constexpr int knobGap = 10;
    const auto knobWidth = (knobs.getWidth() - knobGap * 4) / 5;
    auto placeKnob = [&](juce::Label& label, juce::Slider& slider)
    {
        auto cell = knobs.removeFromLeft(knobWidth);
        label.setBounds(cell.removeFromTop(18));
        slider.setBounds(cell);
        knobs.removeFromLeft(knobGap);
    };
    placeKnob(intensityLabel_, intensitySlider_);
    placeKnob(lengthLabel_, lengthSlider_);
    placeKnob(attackLabel_, attackSlider_);
    placeKnob(releaseLabel_, releaseSlider_);
    placeKnob(shapeLabel_, shapeSlider_);
}

void GlitchDeckEditor::selectSlot(int slot)
{
    const auto nextSlot = std::clamp(slot, 0, GlitchDeckPlugin::numSlots - 1);
    if (nextSlot != selectedSlot_ && plugin_.isMidiLearning(selectedSlot_))
        plugin_.toggleMidiLearn(selectedSlot_);

    selectedSlot_ = nextSlot;
    for (int i = 0; i < GlitchDeckPlugin::numSlots; ++i)
        if (pads_[static_cast<std::size_t>(i)] != nullptr)
            pads_[static_cast<std::size_t>(i)]->setSelected(i == selectedSlot_);
    syncControlsFromPlugin();
    refreshMidiLearnButton();
    repaint();
}

void GlitchDeckEditor::syncControlsFromPlugin()
{
    syncing_ = true;
    globalMix_.setValue(plugin_.parameterValue(glitchdeck::ids::mix), juce::dontSendNotification);
    const auto& ids = plugin_.slotIds(selectedSlot_);
    effectBox_.setSelectedId(std::clamp(plugin_.parameterInt(ids.effect), 0, 7) + 1, juce::dontSendNotification);
    quantizeBox_.setSelectedId(std::clamp(plugin_.parameterInt(ids.quantize), 0, 5) + 1, juce::dontSendNotification);
    stereoBox_.setSelectedId(std::clamp(plugin_.parameterInt(ids.stereo), 0, 3) + 1, juce::dontSendNotification);
    latchButton_.setToggleState(plugin_.parameterBool(ids.latch), juce::dontSendNotification);
    intensitySlider_.setValue(plugin_.parameterValue(ids.intensity), juce::dontSendNotification);
    lengthSlider_.setValue(plugin_.parameterValue(ids.length), juce::dontSendNotification);
    attackSlider_.setValue(plugin_.parameterValue(ids.attack), juce::dontSendNotification);
    releaseSlider_.setValue(plugin_.parameterValue(ids.release), juce::dontSendNotification);
    shapeSlider_.setValue(plugin_.parameterValue(ids.shape), juce::dontSendNotification);
    selectedLabel_.setText("TRIGGER " + juce::String(selectedSlot_ + 1) + "  /  "
                               + toJuce(plugin_.effectName(selectedSlot_)).toUpperCase(),
                           juce::dontSendNotification);
    syncing_ = false;
}

void GlitchDeckEditor::refreshMidiLearnButton()
{
    const auto learning = plugin_.isMidiLearning(selectedSlot_);
    const auto activity = toJuce(plugin_.midiActivityText());
    midiLabel_.setText("MIDI IN  ·  " + activity, juce::dontSendNotification);
    midiLearnButton_.setToggleState(learning, juce::dontSendNotification);
    midiLearnButton_.setButtonText(learning
        ? "LEARNING…  " + activity
        : "LEARN  ·  " + toJuce(plugin_.midiBindingText(selectedSlot_)));
}

void GlitchDeckEditor::timerCallback()
{
    plugin_.applyPendingMidiLearnFromUi();
    syncControlsFromPlugin();
    refreshMidiLearnButton();
    for (auto& pad : pads_)
        if (pad != nullptr)
            pad->repaint();

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

    if (!hasKeyboardFocus(true) || textEditorHasFocus)
    {
        releaseKeyboardTriggers();
        return;
    }

    for (int i = 0; i < GlitchDeckPlugin::numSlots; ++i)
    {
        const auto down = juce::KeyPress::isKeyCurrentlyDown('1' + i);
        if (down != keyboardDown_[static_cast<std::size_t>(i)])
        {
            keyboardDown_[static_cast<std::size_t>(i)] = down;
            plugin_.setTriggerFromUi(i, down);
        }
    }
}

void GlitchDeckEditor::releaseKeyboardTriggers()
{
    for (int i = 0; i < GlitchDeckPlugin::numSlots; ++i)
    {
        if (!keyboardDown_[static_cast<std::size_t>(i)])
            continue;
        keyboardDown_[static_cast<std::size_t>(i)] = false;
        plugin_.setTriggerFromUi(i, false);
    }
}
