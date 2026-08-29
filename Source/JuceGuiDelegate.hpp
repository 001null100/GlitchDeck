#pragma once

#include "GlitchDeckPlugin.hpp"

#include <nullclap/Gui.hpp>
#include <juce_gui_basics/juce_gui_basics.h>

#include <cstdint>
#include <memory>

class GlitchDeckEditor;

class JuceGuiDelegate final : public nullclap::GuiDelegate
{
public:
    explicit JuceGuiDelegate(GlitchDeckPlugin& plugin) noexcept;
    ~JuceGuiDelegate() override;

    bool isApiSupported(const char* api, bool floating) const noexcept override;
    const char* preferredApi() const noexcept override;
    bool create(const char* api, bool floating) noexcept override;
    void destroy() noexcept override;
    bool setScale(double scale) noexcept override;
    bool show() noexcept override;
    bool hide() noexcept override;
    bool getSize(std::uint32_t& width, std::uint32_t& height) noexcept override;
    bool canResize() const noexcept override { return true; }
    bool getResizeHints(clap_gui_resize_hints_t& hints) noexcept override;
    bool adjustSize(std::uint32_t& width, std::uint32_t& height) noexcept override;
    bool setSize(std::uint32_t width, std::uint32_t height) noexcept override;
    bool setParent(const clap_window_t& window) noexcept override;

private:
    std::uint32_t physicalWidth() const noexcept;
    std::uint32_t physicalHeight() const noexcept;
    std::uint32_t toLogical(std::uint32_t physical) const noexcept;
    std::uint32_t toPhysical(std::uint32_t logical) const noexcept;
    void applyLogicalEditorSize() noexcept;

    GlitchDeckPlugin& plugin_;
    std::unique_ptr<juce::ScopedJuceInitialiser_GUI> juceInitialiser_;
    std::unique_ptr<GlitchDeckEditor> editor_;

    // JUCE component bounds are logical pixels. CLAP's Win32 GUI contract uses
    // physical pixels, so keep the design size logical and translate at the host
    // boundary using the scale supplied by clap_plugin_gui::set_scale().
    std::uint32_t logicalWidth_ = 980;
    std::uint32_t logicalHeight_ = 660;
    double scale_ = 1.0;
};
