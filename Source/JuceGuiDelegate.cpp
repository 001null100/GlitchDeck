#include "JuceGuiDelegate.hpp"
#include "GlitchDeckEditor.hpp"

#include <clap/ext/gui.h>

#include <algorithm>
#include <cmath>
#include <cstring>

JuceGuiDelegate::JuceGuiDelegate(GlitchDeckPlugin& plugin) noexcept
    : plugin_(plugin)
{
}

JuceGuiDelegate::~JuceGuiDelegate()
{
    destroy();
}

bool JuceGuiDelegate::isApiSupported(const char* api, bool floating) const noexcept
{
#if JUCE_WINDOWS
    return !floating && api != nullptr && std::strcmp(api, CLAP_WINDOW_API_WIN32) == 0;
#else
    (void)api;
    (void)floating;
    return false;
#endif
}

const char* JuceGuiDelegate::preferredApi() const noexcept
{
#if JUCE_WINDOWS
    return CLAP_WINDOW_API_WIN32;
#else
    return nullptr;
#endif
}

bool JuceGuiDelegate::create(const char* api, bool floating) noexcept
{
    if (!isApiSupported(api, floating) || editor_ != nullptr)
        return false;

    try
    {
        juceInitialiser_ = std::make_unique<juce::ScopedJuceInitialiser_GUI>();
        editor_ = std::make_unique<GlitchDeckEditor>(plugin_);
        applyLogicalEditorSize();
        editor_->setVisible(false);
        return true;
    }
    catch (...)
    {
        editor_.reset();
        juceInitialiser_.reset();
        return false;
    }
}

void JuceGuiDelegate::destroy() noexcept
{
    if (editor_ != nullptr)
    {
        editor_->setVisible(false);
        if (editor_->isOnDesktop())
            editor_->removeFromDesktop();
        editor_.reset();
    }
    juceInitialiser_.reset();
}

bool JuceGuiDelegate::setScale(double scale) noexcept
{
    if (!std::isfinite(scale) || scale <= 0.0)
        return false;

    // Win32 CLAP dimensions are physical pixels while JUCE component bounds are
    // logical pixels. JUCE still owns the actual native DPI scaling, so we return
    // false (the explicit override is ignored) but retain Bitwig's scale value to
    // translate every CLAP size to/from the logical JUCE editor size.
    scale_ = std::clamp(scale, 0.5, 4.0);
    applyLogicalEditorSize();
    return false;
}

bool JuceGuiDelegate::show() noexcept
{
    if (editor_ == nullptr)
        return false;
    editor_->setVisible(true);
    editor_->grabKeyboardFocus();
    return true;
}

bool JuceGuiDelegate::hide() noexcept
{
    if (editor_ == nullptr)
        return false;
    editor_->setVisible(false);
    return true;
}

std::uint32_t JuceGuiDelegate::toPhysical(std::uint32_t logical) const noexcept
{
    return std::max<std::uint32_t>(1, static_cast<std::uint32_t>(std::lround(static_cast<double>(logical) * scale_)));
}

std::uint32_t JuceGuiDelegate::toLogical(std::uint32_t physical) const noexcept
{
    return std::max<std::uint32_t>(1, static_cast<std::uint32_t>(std::lround(static_cast<double>(physical) / scale_)));
}

std::uint32_t JuceGuiDelegate::physicalWidth() const noexcept
{
    return toPhysical(logicalWidth_);
}

std::uint32_t JuceGuiDelegate::physicalHeight() const noexcept
{
    return toPhysical(logicalHeight_);
}

void JuceGuiDelegate::applyLogicalEditorSize() noexcept
{
    if (editor_ != nullptr)
        editor_->setSize(static_cast<int>(logicalWidth_), static_cast<int>(logicalHeight_));
}

bool JuceGuiDelegate::getSize(std::uint32_t& width, std::uint32_t& height) noexcept
{
    width = physicalWidth();
    height = physicalHeight();
    return editor_ != nullptr;
}

bool JuceGuiDelegate::getResizeHints(clap_gui_resize_hints_t& hints) noexcept
{
    hints = {};
    hints.can_resize_horizontally = true;
    hints.can_resize_vertically = true;
    hints.preserve_aspect_ratio = false;
    return true;
}

bool JuceGuiDelegate::adjustSize(std::uint32_t& width, std::uint32_t& height) noexcept
{
    auto logicalWidth = toLogical(width);
    auto logicalHeight = toLogical(height);
    logicalWidth = std::clamp<std::uint32_t>(logicalWidth, 820, 1500);
    logicalHeight = std::clamp<std::uint32_t>(logicalHeight, 570, 1000);
    width = toPhysical(logicalWidth);
    height = toPhysical(logicalHeight);
    return true;
}

bool JuceGuiDelegate::setSize(std::uint32_t width, std::uint32_t height) noexcept
{
    if (!adjustSize(width, height))
        return false;

    logicalWidth_ = toLogical(width);
    logicalHeight_ = toLogical(height);
    applyLogicalEditorSize();
    return true;
}

bool JuceGuiDelegate::setParent(const clap_window_t& window) noexcept
{
#if JUCE_WINDOWS
    if (editor_ == nullptr || window.api == nullptr || window.win32 == nullptr
        || std::strcmp(window.api, CLAP_WINDOW_API_WIN32) != 0)
        return false;

    if (editor_->isOnDesktop())
        editor_->removeFromDesktop();

    editor_->addToDesktop(0, window.win32);
    applyLogicalEditorSize();
    return editor_->isOnDesktop();
#else
    (void)window;
    return false;
#endif
}
