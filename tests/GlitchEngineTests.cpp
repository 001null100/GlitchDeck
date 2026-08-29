#include "GlitchEngine.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <iterator>
#include <string_view>

namespace
{
constexpr float epsilon = 1.0e-3f;

[[noreturn]] void fail(std::string_view test, std::string_view message, float actual = 0.0f, float expected = 0.0f)
{
    std::cerr << "[FAIL] " << test << ": " << message;
    if (actual != 0.0f || expected != 0.0f)
        std::cerr << " (actual=" << actual << ", expected=" << expected << ')';
    std::cerr << '\n';
    std::exit(EXIT_FAILURE);
}

void expectNear(std::string_view test, float actual, float expected)
{
    if (!std::isfinite(actual) || std::abs(actual - expected) > epsilon)
        fail(test, "unexpected sample", actual, expected);
}

void expectBetween(std::string_view test, float actual, float minimum, float maximum)
{
    if (!std::isfinite(actual) || actual < minimum - epsilon || actual > maximum + epsilon)
        fail(test, "sample escaped captured region", actual, minimum);
}

void primeRamp(GlitchEngine& engine, int samples = 100)
{
    for (int i = 0; i < samples; ++i)
        engine.processSample(static_cast<float>(i), static_cast<float>(i), 1.0f);
}

GlitchEngine::SlotConfig configFor(GlitchEngine::EffectType effect)
{
    GlitchEngine::SlotConfig config;
    config.effect = effect;
    config.intensity = 1.0f;
    config.lengthMs = 4.0f;
    config.attackMs = 0.1f;
    config.releaseMs = 0.1f;
    config.shape = 0.5f;
    return config;
}

void reverseStandaloneUsesCapturedRegion()
{
    constexpr auto test = "reverseStandaloneUsesCapturedRegion";
    GlitchEngine engine;
    engine.prepare(1000.0, 2);
    engine.setSlotConfig(0, configFor(GlitchEngine::EffectType::reverse));
    primeRamp(engine);

    engine.trigger(0, true);
    constexpr float expected[] { 97.0f, 96.0f, 95.0f, 94.0f, 97.0f, 96.0f };
    for (int i = 0; i < static_cast<int>(std::size(expected)); ++i)
    {
        const auto output = engine.processSample(100.0f + static_cast<float>(i), 100.0f + static_cast<float>(i), 1.0f);
        expectNear(test, output.left, expected[i]);
    }
}

void pitchRiseStaysInsideCapturedRegion()
{
    constexpr auto test = "pitchRiseStaysInsideCapturedRegion";
    GlitchEngine engine;
    engine.prepare(1000.0, 2);
    auto config = configFor(GlitchEngine::EffectType::pitchRise);
    config.lengthMs = 16.0f;
    engine.setSlotConfig(0, config);
    primeRamp(engine);

    engine.trigger(0, true);
    for (int i = 0; i < 64; ++i)
    {
        const auto output = engine.processSample(100.0f + static_cast<float>(i), 100.0f + static_cast<float>(i), 1.0f);
        expectBetween(test, output.left, 82.0f, 97.0f);
    }
}

void pitchDiveAlsoDefinesACapture()
{
    constexpr auto test = "pitchDiveAlsoDefinesACapture";
    GlitchEngine engine;
    engine.prepare(1000.0, 2);
    engine.setSlotConfig(0, configFor(GlitchEngine::EffectType::pitchDive));
    primeRamp(engine);

    engine.trigger(0, true);
    const auto output = engine.processSample(100.0f, 100.0f, 1.0f);
    expectNear(test, output.left, 94.0f);
}

void stutterDefinesALoop()
{
    constexpr auto test = "stutterDefinesALoop";
    GlitchEngine engine;
    engine.prepare(1000.0, 2);
    engine.setSlotConfig(0, configFor(GlitchEngine::EffectType::stutter));
    primeRamp(engine);

    engine.trigger(0, true);
    constexpr float expected[] { 94.0f, 95.0f, 96.0f, 97.0f, 94.0f, 95.0f };
    for (int i = 0; i < static_cast<int>(std::size(expected)); ++i)
    {
        const auto output = engine.processSample(100.0f + static_cast<float>(i), 100.0f + static_cast<float>(i), 1.0f);
        expectNear(test, output.left, expected[i]);
    }
}

void reverseRecapturesWhenTriggeredOverStutter()
{
    constexpr auto test = "reverseRecapturesWhenTriggeredOverStutter";
    GlitchEngine engine;
    engine.prepare(1000.0, 2);
    engine.setSlotConfig(0, configFor(GlitchEngine::EffectType::stutter));
    engine.setSlotConfig(1, configFor(GlitchEngine::EffectType::reverse));
    primeRamp(engine);

    engine.trigger(0, true);
    expectNear(test, engine.processSample(100.0f, 100.0f, 1.0f).left, 94.0f);

    engine.trigger(1, true);
    expectNear(test, engine.processSample(101.0f, 101.0f, 1.0f).left, 98.0f);
    expectNear(test, engine.processSample(102.0f, 102.0f, 1.0f).left, 97.0f);
}

void releasingStutterKeepsReverseCapture()
{
    constexpr auto test = "releasingStutterKeepsReverseCapture";
    GlitchEngine engine;
    engine.prepare(1000.0, 2);
    engine.setSlotConfig(0, configFor(GlitchEngine::EffectType::stutter));
    engine.setSlotConfig(1, configFor(GlitchEngine::EffectType::reverse));
    primeRamp(engine);

    engine.trigger(0, true);
    engine.processSample(100.0f, 100.0f, 1.0f); // old loop, 94
    engine.trigger(1, true);
    engine.processSample(101.0f, 101.0f, 1.0f); // new reverse capture, 98

    engine.trigger(0, false);
    constexpr float expected[] { 97.0f, 96.0f, 95.0f, 98.0f, 97.0f, 96.0f };
    for (int i = 0; i < static_cast<int>(std::size(expected)); ++i)
    {
        const auto output = engine.processSample(102.0f + static_cast<float>(i), 102.0f + static_cast<float>(i), 1.0f);
        expectNear(test, output.left, expected[i]);
    }
}
} // namespace

int main()
{
    reverseStandaloneUsesCapturedRegion();
    pitchRiseStaysInsideCapturedRegion();
    pitchDiveAlsoDefinesACapture();
    stutterDefinesALoop();
    reverseRecapturesWhenTriggeredOverStutter();
    releasingStutterKeepsReverseCapture();
    std::cout << "GlitchEngine regression tests passed\n";
    return EXIT_SUCCESS;
}
