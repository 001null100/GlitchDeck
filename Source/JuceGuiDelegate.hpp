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
    GlitchDeckPlugin& plugin_;
    std::unique_ptr<juce::ScopedJuceInitialiser_GUI> juceInitialiser_;
    std::unique_ptr<GlitchDeckEditor> editor_;
    std::uint32_t width_ = 980;
    std::uint32_t height_ = 660;
};
