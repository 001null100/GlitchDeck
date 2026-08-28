#include "GlitchEngine.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
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

void reverseUsesStreamingHistory()
{
    constexpr auto test = "reverseUsesStreamingHistory";
    GlitchEngine engine;
    engine.prepare(1000.0, 2);
    engine.setSlotConfig(0, configFor(GlitchEngine::EffectType::reverse));
    primeRamp(engine);

    engine.trigger(0, true);
    for (int i = 0; i < 5; ++i)
    {
        const auto output = engine.processSample(100.0f + static_cast<float>(i), 100.0f + static_cast<float>(i), 1.0f);
        expectNear(test, output.left, 98.0f - static_cast<float>(i));
    }
}

void pitchGesturesDoNotImplicitlyCaptureLoops()
{
    constexpr auto test = "pitchGesturesDoNotImplicitlyCaptureLoops";
    for (const auto effect : { GlitchEngine::EffectType::pitchDive, GlitchEngine::EffectType::pitchRise })
    {
        GlitchEngine engine;
        engine.prepare(1000.0, 2);
        auto config = configFor(effect);
        config.lengthMs = 1500.0f;
        engine.setSlotConfig(0, config);
        primeRamp(engine);

        engine.trigger(0, true);
        const auto output = engine.processSample(100.0f, 100.0f, 1.0f);
        expectNear(test, output.left, 98.0f);
    }
}

void stutterStillDefinesALoop()
{
    constexpr auto test = "stutterStillDefinesALoop";
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

void releasingLoopReturnsRemainingModifierToStreaming()
{
    constexpr auto test = "releasingLoopReturnsRemainingModifierToStreaming";
    GlitchEngine engine;
    engine.prepare(1000.0, 2);
    engine.setSlotConfig(0, configFor(GlitchEngine::EffectType::stutter));
    engine.setSlotConfig(1, configFor(GlitchEngine::EffectType::reverse));
    primeRamp(engine);

    engine.trigger(0, true);
    expectNear(test, engine.processSample(100.0f, 100.0f, 1.0f).left, 94.0f);

    engine.trigger(1, true);
    expectNear(test, engine.processSample(101.0f, 101.0f, 1.0f).left, 95.0f);

    engine.trigger(0, false);
    constexpr float expected[] { 94.0f, 93.0f, 92.0f, 91.0f, 90.0f, 89.0f };
    for (int i = 0; i < static_cast<int>(std::size(expected)); ++i)
    {
        const auto output = engine.processSample(102.0f + static_cast<float>(i), 102.0f + static_cast<float>(i), 1.0f);
        expectNear(test, output.left, expected[i]);
    }
}
} // namespace

int main()
{
    reverseUsesStreamingHistory();
    pitchGesturesDoNotImplicitlyCaptureLoops();
    stutterStillDefinesALoop();
    releasingLoopReturnsRemainingModifierToStreaming();
    std::cout << "GlitchEngine regression tests passed\n";
    return EXIT_SUCCESS;
}
