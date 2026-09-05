#include "GlitchDeckEditor.hpp"
#include "UiValueText.hpp"

#include <algorithm>
#include <cmath>

namespace
{
const auto background = juce::Colour(0xff0d1016);
const auto panel = juce::Colour(0xff151a23);
const auto panelRaised = juce::Colour(0xff1d2430);
const auto text = juce::Colour(0xffeef3f8);
const auto muted = juce::Colour(0xffa4b0c2);
const auto accent = juce::Colour(0xff55d9ff);
const auto hot = juce::Colour(0xffff4fc8);
const auto queuedColour = juce::Colour(0xffffd176);

void styleLabel(juce::Label& label, const juce::String& caption)
{
    label.setText(caption, juce::dontSendNotification);
    label.setColour(juce::Label::textColourId, muted);
    label.setJustificationType(juce::Justification::centredLeft);
    label.setFont(juce::Font(12.0f, juce::Font::bold));
}
juce::String toJuce(const std::string& value) { return juce::String::fromUTF8(value.c_str()); }

bool textEntryHasFocus()
{
    for (auto* c = juce::Component::getCurrentlyFocusedComponent(); c != nullptr; c = c->getParentComponent())
        if (dynamic_cast<juce::TextEditor*>(c) != nullptr) return true;
    return false;
}

void setPercentFormat(juce::Slider& slider)
{
    slider.textFromValueFunction = [](double value) { return juce::String(value * 100.0, 1) + "%"; };
    slider.valueFromTextFunction = [&slider](const juce::String& value) {
        return glitchdeck::ui::parsePercent(value.toStdString(), slider.getValue());
    };
}
}

GlitchDeckEditor::TriggerPad::TriggerPad(GlitchDeckPlugin& plugin, int slotIndex)
    : plugin_(plugin), slot_(slotIndex)
{
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    setWantsKeyboardFocus(true);
    setTitle("Trigger " + juce::String(slot_ + 1));
    setTooltip("Hold left mouse or key " + juce::String(slot_ + 1)
        + " to play. Right-click selects without firing. Latch toggles on each press.");
}

void GlitchDeckEditor::TriggerPad::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced(1.0f);
    const bool active = plugin_.isSlotActive(slot_), pending = plugin_.isSlotPending(slot_);
    const auto colour = pending ? queuedColour : active ? hot : selected_ ? accent : juce::Colour(0xff3a485c);
    g.setColour(active ? juce::Colour(0xff293648) : panelRaised);
    g.fillRoundedRectangle(bounds, 10.0f);
    g.setColour(colour);
    g.drawRoundedRectangle(bounds, 10.0f, selected_ || active || pending ? 2.0f : 1.0f);
    auto content = getLocalBounds().reduced(12, 10);
    auto top = content.removeFromTop(18);
    g.setFont(juce::Font(11.0f, juce::Font::bold));
    g.setColour(selected_ ? accent : muted);
    g.drawText("KEY " + juce::String(slot_ + 1), top.removeFromLeft(56), juce::Justification::centredLeft);
    g.setColour(colour);
    const bool latch = plugin_.parameterBool(plugin_.slotIds(slot_).latch);
    g.drawText(pending ? "QUEUED" : active ? "ACTIVE" : latch ? "LATCH" : "HOLD", top,
               juce::Justification::centredRight, true);
    auto binding = content.removeFromBottom(18);
    g.setColour(muted);
    g.setFont(juce::Font(11.5f));
    g.drawText(toJuce(plugin_.midiBindingText(slot_)), binding, juce::Justification::centredLeft, true);
    g.setColour(text);
    g.setFont(juce::Font(getHeight() < 125 ? 18.0f : 23.0f, juce::Font::bold));
    g.drawText(toJuce(plugin_.effectName(slot_)).toUpperCase(), content.removeFromTop(30),
               juce::Justification::centredLeft, true);
    if (getHeight() >= 125)
    {
        const auto& ids = plugin_.slotIds(slot_);
        const int quantize = std::clamp(plugin_.parameterInt(ids.quantize), 0, 5);
        g.setFont(juce::Font(12.0f));
        g.setColour(muted);
        g.drawText(juce::String(GlitchDeckPlugin::quantizeNames()[static_cast<std::size_t>(quantize)])
            + " / " + juce::String(plugin_.parameterValue(ids.length), 0) + " ms", content,
            juce::Justification::centredLeft, true);
    }
}

void GlitchDeckEditor::TriggerPad::mouseDown(const juce::MouseEvent& event)
{
    if (onSelected) onSelected(slot_);
    if (!event.mods.isLeftButtonDown()) return;
    grabKeyboardFocus();
    mouseHeld_ = true;
    plugin_.setTriggerFromUi(slot_, true, GlitchDeckPlugin::UiSource::mouse);
    repaint();
}
void GlitchDeckEditor::TriggerPad::mouseUp(const juce::MouseEvent&) { release(); }
void GlitchDeckEditor::TriggerPad::release()
{
    if (!mouseHeld_) return;
    mouseHeld_ = false;
    plugin_.setTriggerFromUi(slot_, false, GlitchDeckPlugin::UiSource::mouse);
    repaint();
}
void GlitchDeckEditor::TriggerPad::setSelected(bool selected)
{
    selected_ = selected;
    repaint();
}

GlitchDeckEditor::GlitchDeckEditor(GlitchDeckPlugin& plugin) : plugin_(plugin)
{
    setWantsKeyboardFocus(true);
    setOpaque(true);
    titleLabel_.setText("GLITCHDECK", juce::dontSendNotification);
    titleLabel_.setColour(juce::Label::textColourId, text);
    titleLabel_.setFont(juce::Font(27.0f, juce::Font::bold));
    addAndMakeVisible(titleLabel_);
    styleLabel(subtitleLabel_, "PLAY THE DAMAGE / 0.2.1");
    addAndMakeVisible(subtitleLabel_);
    styleLabel(globalMixLabel_, "GLOBAL MIX");
    addAndMakeVisible(globalMixLabel_);
    configureKnob(globalMix_);
    globalMix_.setSliderStyle(juce::Slider::LinearHorizontal);
    globalMix_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 72, 24);
    globalMix_.setRange(0.0, 1.0, 0.001);
    globalMix_.setColour(juce::Slider::trackColourId, accent);
    globalMix_.setDoubleClickReturnValue(true, 1.0);
    globalMix_.setTooltip("100% is fully wet while an effect is engaged. Double-click resets to 100%.");
    globalMix_.setTitle("Global mix percentage");
    setPercentFormat(globalMix_);
    bindSlider(globalMix_, [] { return glitchdeck::ids::mix; });

    panicButton_.setColour(juce::TextButton::buttonColourId, hot.withAlpha(0.15f));
    panicButton_.setColour(juce::TextButton::textColourOffId, text);
    panicButton_.setWantsKeyboardFocus(false);
    panicButton_.setTooltip("Release all pads and latches, cancel queued starts and MIDI Learn. Preserves settings.");
    panicButton_.onClick = [this] { panic(); };
    addAndMakeVisible(panicButton_);

    for (int i = 0; i < GlitchDeckPlugin::numSlots; ++i)
    {
        pads_[static_cast<std::size_t>(i)] = std::make_unique<TriggerPad>(plugin_, i);
        pads_[static_cast<std::size_t>(i)]->onSelected = [this](int slot) { selectSlot(slot); };
        addAndMakeVisible(*pads_[static_cast<std::size_t>(i)]);
    }
    selectedLabel_.setColour(juce::Label::textColourId, text);
    selectedLabel_.setFont(juce::Font(15.0f, juce::Font::bold));
    addAndMakeVisible(selectedLabel_);
    for (auto* box : { &effectBox_, &quantizeBox_, &stereoBox_, &midiTypeBox_, &midiChannelBox_ })
        configureCombo(*box);
    const auto fillCombo = [](juce::ComboBox& box, const auto& names) {
        for (std::size_t i = 0; i < names.size(); ++i) box.addItem(names[i], static_cast<int>(i) + 1);
    };
    fillCombo(effectBox_, GlitchDeckPlugin::effectNames());
    fillCombo(quantizeBox_, GlitchDeckPlugin::quantizeNames());
    fillCombo(stereoBox_, GlitchDeckPlugin::stereoNames());
    fillCombo(midiTypeBox_, GlitchDeckPlugin::midiTypeNames());
    midiChannelBox_.addItem("Any channel", 1);
    for (int channel = 1; channel <= 16; ++channel)
        midiChannelBox_.addItem("Channel " + juce::String(channel), channel + 1);

    const auto bindCombo = [this](juce::ComboBox& box, auto idForSlot) {
        box.onChange = [this, &box, idForSlot] {
            if (!syncing_ && box.getSelectedId() > 0)
                setOneShot(idForSlot(plugin_.slotIds(selectedSlot_)), box.getSelectedId() - 1);
        };
    };
    bindCombo(effectBox_, [](const auto& id) { return id.effect; });
    bindCombo(quantizeBox_, [](const auto& id) { return id.quantize; });
    bindCombo(stereoBox_, [](const auto& id) { return id.stereo; });
    bindCombo(midiTypeBox_, [](const auto& id) { return id.midiType; });
    bindCombo(midiChannelBox_, [](const auto& id) { return id.midiChannel; });
    effectBox_.setTooltip("Changing an effect releases its old capture. Press again to start the new effect.");
    quantizeBox_.setTooltip("Onsets wait for the next musical grid. Releasing a held pad cancels its queued onset.");
    stereoBox_.setTooltip("Linked, spread, swapped channels, or mono processing.");
    midiTypeBox_.setTooltip("Choose raw MIDI CC or notes. Notes also accept native CLAP note events.");
    midiChannelBox_.setTooltip("Any channel accepts independent holds on all 16 channels.");

    configureKnob(midiNumberSlider_);
    midiNumberSlider_.setSliderStyle(juce::Slider::IncDecButtons);
    midiNumberSlider_.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 55, 30);
    midiNumberSlider_.setRange(0, 127, 1);
    midiNumberSlider_.setTooltip("CC or note number. CC 120-127 are channel commands, not playable bindings.");
    bindSlider(midiNumberSlider_, [this] { return plugin_.slotIds(selectedSlot_).midiNumber; });
    latchButton_.setColour(juce::ToggleButton::textColourId, text);
    latchButton_.setColour(juce::ToggleButton::tickColourId, hot);
    latchButton_.setTooltip("Toggle on each new press instead of playing only while held. PANIC releases all latches.");
    latchButton_.onClick = [this] {
        if (!syncing_) setOneShot(plugin_.slotIds(selectedSlot_).latch, latchButton_.getToggleState() ? 1.0 : 0.0);
    };
    addAndMakeVisible(latchButton_);
    midiLearnButton_.setColour(juce::TextButton::buttonColourId, panelRaised);
    midiLearnButton_.setColour(juce::TextButton::buttonOnColourId, hot.withAlpha(0.2f));
    midiLearnButton_.setColour(juce::TextButton::textColourOffId, text);
    midiLearnButton_.setColour(juce::TextButton::textColourOnId, hot);
    midiLearnButton_.setWantsKeyboardFocus(false);
    midiLearnButton_.setTooltip("Capture the next note-on or CC press (value 64 or higher). Click again to cancel.");
    midiLearnButton_.onClick = [this] { plugin_.toggleMidiLearn(selectedSlot_); refreshMidiLearnButton(); };
    addAndMakeVisible(midiLearnButton_);

    styleLabel(effectLabel_, "EFFECT"); styleLabel(quantizeLabel_, "QUANTIZE");
    styleLabel(stereoLabel_, "STEREO"); styleLabel(midiLabel_, "MIDI TYPE");
    styleLabel(midiNumberLabel_, "NUMBER"); styleLabel(midiChannelLabel_, "CHANNEL");
    styleLabel(intensityLabel_, "INTENSITY"); styleLabel(lengthLabel_, "LENGTH");
    styleLabel(attackLabel_, "ATTACK"); styleLabel(releaseLabel_, "RELEASE"); styleLabel(shapeLabel_, "SHAPE");
    for (auto* label : { &effectLabel_, &quantizeLabel_, &stereoLabel_, &midiLabel_, &midiNumberLabel_,
        &midiChannelLabel_, &intensityLabel_, &lengthLabel_, &attackLabel_, &releaseLabel_, &shapeLabel_ })
        addAndMakeVisible(*label);
    for (auto* slider : { &intensitySlider_, &lengthSlider_, &attackSlider_, &releaseSlider_, &shapeSlider_ })
        configureKnob(*slider);
    intensitySlider_.setRange(0, 1, 0.001); shapeSlider_.setRange(0, 1, 0.001);
    lengthSlider_.setRange(2, 1500, 0.01); lengthSlider_.setSkewFactor(0.35);
    attackSlider_.setRange(0.1, 100, 0.01); attackSlider_.setSkewFactor(0.4);
    releaseSlider_.setRange(0.1, 300, 0.01); releaseSlider_.setSkewFactor(0.4);
    setPercentFormat(intensitySlider_); setPercentFormat(shapeSlider_);
    for (auto* slider : { &lengthSlider_, &attackSlider_, &releaseSlider_ })
        slider->setTextValueSuffix(" ms");
    intensitySlider_.setTooltip("Effect strength, not a dry/wet blend. Double-click restores this pad's default.");
    lengthSlider_.setTooltip("Capture duration in milliseconds. A change at the onset applies to that capture.");
    attackSlider_.setTooltip("Fade-in time in milliseconds.");
    releaseSlider_.setTooltip("Fade-out time in milliseconds. A fully wet effect does not leak dry audio under its envelope.");
    shapeSlider_.setTooltip("Effect-specific motion/shape. Double-click resets to 50%.");
    bindSlider(intensitySlider_, [this] { return plugin_.slotIds(selectedSlot_).intensity; });
    bindSlider(lengthSlider_, [this] { return plugin_.slotIds(selectedSlot_).length; });
    bindSlider(attackSlider_, [this] { return plugin_.slotIds(selectedSlot_).attack; });
    bindSlider(releaseSlider_, [this] { return plugin_.slotIds(selectedSlot_).release; });
    bindSlider(shapeSlider_, [this] { return plugin_.slotIds(selectedSlot_).shape; });
    for (auto pair : { std::pair{ &intensitySlider_, "Intensity percentage" }, { &shapeSlider_, "Shape percentage" },
        { &lengthSlider_, "Capture length milliseconds" }, { &attackSlider_, "Attack milliseconds" },
        { &releaseSlider_, "Release milliseconds" }, { &midiNumberSlider_, "MIDI number" } })
        pair.first->setTitle(pair.second);

    styleLabel(statusLabel_, ""); styleLabel(helpLabel_, "KEYS 1-8 / Right-click to select / ESC: panic / Double-click knobs to reset");
    statusLabel_.setFont(juce::Font(12.0f)); helpLabel_.setFont(juce::Font(11.0f));
    addAndMakeVisible(statusLabel_); addAndMakeVisible(helpLabel_);
    selectSlot(0);
    setSize(1040, 740);
    startTimerHz(30);
}

GlitchDeckEditor::~GlitchDeckEditor()
{
    stopTimer();
    setVisible(false);
    releaseHeldInputs();
    if (plugin_.isMidiLearning(selectedSlot_)) plugin_.toggleMidiLearn(selectedSlot_);
    for (auto* slider : { &globalMix_, &intensitySlider_, &lengthSlider_, &attackSlider_, &releaseSlider_, &shapeSlider_, &midiNumberSlider_ })
    {
        slider->hideTextBox(true);
        if (slider->onDragEnd) slider->onDragEnd();
        slider->onValueChange = {}; slider->onDragStart = {}; slider->onDragEnd = {};
    }
}

void GlitchDeckEditor::configureKnob(juce::Slider& slider)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 86, 23);
    slider.setColour(juce::Slider::rotarySliderFillColourId, accent);
    slider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff344357));
    slider.setColour(juce::Slider::thumbColourId, text);
    slider.setColour(juce::Slider::textBoxTextColourId, text);
    slider.setColour(juce::Slider::textBoxBackgroundColourId, panelRaised);
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xff344357));
    addAndMakeVisible(slider);
}
void GlitchDeckEditor::configureCombo(juce::ComboBox& box)
{
    box.setColour(juce::ComboBox::backgroundColourId, panelRaised);
    box.setColour(juce::ComboBox::textColourId, text);
    box.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff425068));
    box.setColour(juce::ComboBox::arrowColourId, accent);
    addAndMakeVisible(box);
}
void GlitchDeckEditor::bindSlider(juce::Slider& slider, std::function<clap_id()> parameter)
{
    // Capture the ID at drag start. Switching pads cannot leave the old gesture open.
    const auto draggingId = std::make_shared<clap_id>(CLAP_INVALID_ID);
    slider.onDragStart = [this, parameter, draggingId] {
        *draggingId = parameter(); plugin_.beginUiEdit(*draggingId);
    };
    slider.onValueChange = [this, &slider, parameter, draggingId] {
        if (syncing_) return;
        if (*draggingId != CLAP_INVALID_ID) plugin_.setUiValue(*draggingId, slider.getValue());
        else plugin_.setUiValueOnce(parameter(), slider.getValue()); // Text, wheel, keyboard and double-click.
    };
    slider.onDragEnd = [this, draggingId] {
        if (*draggingId != CLAP_INVALID_ID) plugin_.endUiEdit(*draggingId);
        *draggingId = CLAP_INVALID_ID;
    };
}
void GlitchDeckEditor::setOneShot(clap_id id, double value) { plugin_.setUiValueOnce(id, value); }

void GlitchDeckEditor::paint(juce::Graphics& g)
{
    g.fillAll(background);
    const auto bounds = editPanel_.toFloat();
    g.setColour(panel); g.fillRoundedRectangle(bounds, 12.0f);
    g.setColour(juce::Colour(0xff344357)); g.drawRoundedRectangle(bounds, 12.0f, 1.0f);
    g.setColour(accent); g.fillRoundedRectangle(bounds.getX(), bounds.getY(), 4.0f, bounds.getHeight(), 2.0f);
}
void GlitchDeckEditor::resized()
{
    auto area = getLocalBounds().reduced(20);
    auto header = area.removeFromTop(56);
    panicButton_.setBounds(header.removeFromRight(126).withSizeKeepingCentre(126, 34));
    header.removeFromRight(16);
    auto mixArea = header.removeFromRight(std::clamp(getWidth() / 4, 215, 285));
    globalMixLabel_.setBounds(mixArea.removeFromTop(18)); globalMix_.setBounds(mixArea.removeFromTop(32));
    titleLabel_.setBounds(header.removeFromTop(34)); subtitleLabel_.setBounds(header.removeFromTop(18));
    auto footer = area.removeFromBottom(43);
    statusLabel_.setBounds(footer.removeFromTop(23)); helpLabel_.setBounds(footer);
    area.removeFromTop(6); area.removeFromBottom(6);
    editPanel_ = area.removeFromBottom(266);
    area.removeFromBottom(12);
    constexpr int gap = 10;
    const int padWidth = (area.getWidth() - 3 * gap) / 4, padHeight = (area.getHeight() - gap) / 2;
    for (int i = 0; i < GlitchDeckPlugin::numSlots; ++i)
        pads_[static_cast<std::size_t>(i)]->setBounds(area.getX() + (i % 4) * (padWidth + gap),
            area.getY() + (i / 4) * (padHeight + gap), padWidth, padHeight);
    auto editor = editPanel_.reduced(14, 8);
    selectedLabel_.setBounds(editor.removeFromTop(26));
    auto first = editor.removeFromTop(54);
    const int fieldWidth = (first.getWidth() - 128 - 3 * gap) / 3;
    const auto field = [](juce::Rectangle<int>& row, juce::Label& label, juce::Component& control, int width) {
        auto cell = row.removeFromLeft(width); label.setBounds(cell.removeFromTop(18));
        control.setBounds(cell.removeFromTop(30)); row.removeFromLeft(10);
    };
    field(first, effectLabel_, effectBox_, fieldWidth);
    field(first, quantizeLabel_, quantizeBox_, fieldWidth);
    field(first, stereoLabel_, stereoBox_, fieldWidth);
    first.removeFromTop(18); latchButton_.setBounds(first.removeFromTop(30));
    auto midi = editor.removeFromTop(54);
    const int midiWidth = (midi.getWidth() - 144 - 3 * gap) / 3;
    field(midi, midiLabel_, midiTypeBox_, midiWidth);
    field(midi, midiNumberLabel_, midiNumberSlider_, midiWidth);
    field(midi, midiChannelLabel_, midiChannelBox_, midiWidth);
    midi.removeFromTop(18); midiLearnButton_.setBounds(midi.removeFromTop(30));
    editor.removeFromTop(4);
    const int knobWidth = (editor.getWidth() - 4 * gap) / 5;
    const auto knob = [&](juce::Label& label, juce::Slider& slider) {
        auto cell = editor.removeFromLeft(knobWidth); label.setBounds(cell.removeFromTop(18));
        slider.setBounds(cell); editor.removeFromLeft(gap);
    };
    knob(intensityLabel_, intensitySlider_); knob(lengthLabel_, lengthSlider_);
    knob(attackLabel_, attackSlider_); knob(releaseLabel_, releaseSlider_); knob(shapeLabel_, shapeSlider_);
}

void GlitchDeckEditor::selectSlot(int slot)
{
    const int next = std::clamp(slot, 0, GlitchDeckPlugin::numSlots - 1);
    if (next != selectedSlot_)
    {
        // Commit text to the old pad before changing which parameter the control edits.
        for (auto* s : { &intensitySlider_, &lengthSlider_, &attackSlider_, &releaseSlider_, &shapeSlider_, &midiNumberSlider_ })
            s->hideTextBox(false);
        if (plugin_.isMidiLearning(selectedSlot_)) plugin_.toggleMidiLearn(selectedSlot_);
    }
    selectedSlot_ = next;
    for (int i = 0; i < GlitchDeckPlugin::numSlots; ++i) pads_[static_cast<std::size_t>(i)]->setSelected(i == next);
    syncControlsFromPlugin(); refreshMidiLearnButton(); repaint();
}
void GlitchDeckEditor::syncControlsFromPlugin()
{
    syncing_ = true;
    const auto slider = [this](juce::Slider& control, clap_id id) {
        if (!control.isMouseButtonDown(true) && !control.hasKeyboardFocus(true))
            control.setValue(plugin_.parameterValue(id), juce::dontSendNotification);
        clap_param_info_t info {};
        for (std::uint32_t i = 0; i < plugin_.parameters().count(); ++i)
            if (plugin_.parameters().info(i, info) && info.id == id) {
                control.setDoubleClickReturnValue(true, info.default_value); break;
            }
    };
    const auto combo = [this](juce::ComboBox& control, clap_id id, int maximum) {
        if (!control.isPopupActive()) control.setSelectedId(std::clamp(plugin_.parameterInt(id), 0, maximum) + 1, juce::dontSendNotification);
    };
    slider(globalMix_, glitchdeck::ids::mix);
    const auto& id = plugin_.slotIds(selectedSlot_);
    combo(effectBox_, id.effect, 7); combo(quantizeBox_, id.quantize, 5); combo(stereoBox_, id.stereo, 3);
    combo(midiTypeBox_, id.midiType, 1); combo(midiChannelBox_, id.midiChannel, 16);
    slider(midiNumberSlider_, id.midiNumber); slider(intensitySlider_, id.intensity); slider(lengthSlider_, id.length);
    slider(attackSlider_, id.attack); slider(releaseSlider_, id.release); slider(shapeSlider_, id.shape);
    latchButton_.setToggleState(plugin_.parameterBool(id.latch), juce::dontSendNotification);
    midiNumberLabel_.setText(plugin_.parameterInt(id.midiType) == 1 ? "CC NUMBER" : "NOTE NUMBER", juce::dontSendNotification);
    selectedLabel_.setText("TRIGGER " + juce::String(selectedSlot_ + 1) + " / " + toJuce(plugin_.effectName(selectedSlot_)).toUpperCase(),
                           juce::dontSendNotification);
    syncing_ = false;
}
void GlitchDeckEditor::refreshMidiLearnButton()
{
    const bool learning = plugin_.isMidiLearning(selectedSlot_);
    midiLearnButton_.setToggleState(learning, juce::dontSendNotification);
    midiLearnButton_.setButtonText(learning ? "CANCEL LEARN" : "MIDI LEARN");
    const auto& id = plugin_.slotIds(selectedSlot_);
    const bool reserved = plugin_.parameterInt(id.midiType) == 1 && plugin_.parameterInt(id.midiNumber) >= 120;
    statusLabel_.setColour(juce::Label::textColourId, reserved || learning ? queuedColour : muted);
    statusLabel_.setText(reserved ? "CC 120-127 are channel commands. Choose CC 0-119 for a playable pad."
        : (learning ? "LEARNING / " : "MIDI / ") + toJuce(plugin_.midiActivityText()), juce::dontSendNotification);
}
void GlitchDeckEditor::timerCallback()
{
    plugin_.applyPendingMidiLearnFromUi();
    syncControlsFromPlugin(); refreshMidiLearnButton();
    for (auto& pad : pads_) pad->repaint();
    updateKeyboardTriggers();
    if (!isShowing()) releaseHeldInputs();
}
void GlitchDeckEditor::updateKeyboardTriggers()
{
    bool any = false;
    for (int i = 0; i < GlitchDeckPlugin::numSlots; ++i) any |= juce::KeyPress::isKeyCurrentlyDown('1' + i);
    if (!hasKeyboardFocus(true) || !isShowing() || textEntryHasFocus())
    {
        releaseKeyboardTriggers(); keyboardSuppressed_ = any; return;
    }
    if (keyboardSuppressed_) { keyboardSuppressed_ = any; return; }
    for (int i = 0; i < GlitchDeckPlugin::numSlots; ++i)
    {
        const bool down = juce::KeyPress::isKeyCurrentlyDown('1' + i);
        if (down == keyboardDown_[static_cast<std::size_t>(i)]) continue;
        keyboardDown_[static_cast<std::size_t>(i)] = down;
        plugin_.setTriggerFromUi(i, down, GlitchDeckPlugin::UiSource::keyboard);
    }
}
void GlitchDeckEditor::releaseKeyboardTriggers()
{
    for (int i = 0; i < GlitchDeckPlugin::numSlots; ++i)
        if (keyboardDown_[static_cast<std::size_t>(i)]) {
            keyboardDown_[static_cast<std::size_t>(i)] = false;
            plugin_.setTriggerFromUi(i, false, GlitchDeckPlugin::UiSource::keyboard);
        }
}
void GlitchDeckEditor::releaseHeldInputs()
{
    releaseKeyboardTriggers();
    for (auto& pad : pads_) if (pad) pad->release();
}
void GlitchDeckEditor::panic()
{
    releaseHeldInputs(); plugin_.panicFromUi(); keyboardSuppressed_ = true;
    refreshMidiLearnButton(); repaint();
}
void GlitchDeckEditor::visibilityChanged() { if (!isVisible()) releaseHeldInputs(); }
void GlitchDeckEditor::focusLost(FocusChangeType) { if (!hasKeyboardFocus(true)) releaseHeldInputs(); }
void GlitchDeckEditor::focusOfChildComponentChanged(FocusChangeType)
{
    if (!hasKeyboardFocus(true)) releaseHeldInputs();
    updateKeyboardTriggers();
}
bool GlitchDeckEditor::keyStateChanged(bool)
{
    bool handled = std::any_of(keyboardDown_.begin(), keyboardDown_.end(), [](bool held) { return held; });
    updateKeyboardTriggers();
    handled |= std::any_of(keyboardDown_.begin(), keyboardDown_.end(), [](bool held) { return held; });
    return handled && !textEntryHasFocus();
}
bool GlitchDeckEditor::keyPressed(const juce::KeyPress& key)
{
    if (key.getKeyCode() == juce::KeyPress::escapeKey && !textEntryHasFocus()) { panic(); return true; }
    return key.getKeyCode() >= '1' && key.getKeyCode() <= '8' && !textEntryHasFocus();
}
