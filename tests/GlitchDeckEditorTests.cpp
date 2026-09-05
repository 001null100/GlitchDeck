#include "GlitchDeckEditor.hpp"
#include <cstdio>
#include <cstdlib>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); std::abort(); } } while(false)

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;
    const clap_host_t host {
        CLAP_VERSION, nullptr, "GlitchDeck editor tests", "Null Exo", "", "1",
        [](const clap_host_t*, const char*) -> const void* { return nullptr; },
        [](const clap_host_t*) {}, [](const clap_host_t*) {}, [](const clap_host_t*) {}
    };
    auto* plugin = new GlitchDeckPlugin(&host);
    const auto* api = plugin->clapPlugin();
    CHECK(api->init(api));
    {
        GlitchDeckEditor editor(*plugin);
        editor.setVisible(true);
        for (const auto size : { juce::Point<int>(820, 620), juce::Point<int>(1040, 740), juce::Point<int>(1500, 1000) })
        {
            editor.setSize(size.x, size.y);
            for (int i = 0; i < editor.getNumChildComponents(); ++i)
            {
                const auto* child = editor.getChildComponent(i);
                CHECK(child != nullptr);
                CHECK(!child->getBounds().isEmpty());
                CHECK(editor.getLocalBounds().contains(child->getBounds()));
            }
            const auto image = editor.createComponentSnapshot(editor.getLocalBounds());
            CHECK(image.isValid());
            const auto file = juce::File::getCurrentWorkingDirectory().getChildFile(
                "GlitchDeck-" + juce::String(size.x) + "x" + juce::String(size.y) + ".png");
            auto stream = file.createOutputStream();
            CHECK(stream != nullptr && stream->openedOk());
            juce::PNGImageFormat png;
            CHECK(png.writeImageToStream(image, *stream));
        }
        bool testedMix = false, testedPanic = false;
        for (int i = 0; i < editor.getNumChildComponents(); ++i)
        {
            auto* child = editor.getChildComponent(i);
            if (auto* slider = dynamic_cast<juce::Slider*>(child);
                slider != nullptr && slider->getTitle() == "Global mix percentage")
            {
                CHECK(slider->getValueFromText("50%") == 0.5);
                CHECK(slider->getValueFromText("12.5") == 0.125);
                slider->setValue(slider->getValueFromText("50%"), juce::sendNotificationSync);
                CHECK(plugin->parameterValue(glitchdeck::ids::mix) == 0.5);
                testedMix = true;
            }
            if (auto* button = dynamic_cast<juce::TextButton*>(child);
                button != nullptr && button->getButtonText() == "PANIC / ESC")
            {
                CHECK(static_cast<bool>(button->onClick));
                button->onClick(); testedPanic = true;
            }
        }
        CHECK(testedMix && testedPanic);
        editor.setVisible(false);
    }
    api->destroy(api);
    std::puts("GlitchDeck editor layout, rendering, percentage entry and panic wiring passed");
}
