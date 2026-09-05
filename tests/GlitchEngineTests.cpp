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

void reverseMatchesAlphaSixLoop()
{
    constexpr auto test = "reverseMatchesAlphaSixLoop";
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

void stutterMatchesAlphaSixLoop()
{
    constexpr auto test = "stutterMatchesAlphaSixLoop";
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

void pitchDiveMatchesAlphaSixCapture()
{
    constexpr auto test = "pitchDiveMatchesAlphaSixCapture";
    GlitchEngine engine;
    engine.prepare(1000.0, 2);
    engine.setSlotConfig(0, configFor(GlitchEngine::EffectType::pitchDive));
    primeRamp(engine);

    engine.trigger(0, true);
    expectNear(test, engine.processSample(100.0f, 100.0f, 1.0f).left, 94.0f);
}

void pitchRiseMatchesAlphaSixReadHead()
{
    constexpr auto test = "pitchRiseMatchesAlphaSixReadHead";
    GlitchEngine engine;
    engine.prepare(1000.0, 2);
    engine.setSlotConfig(0, configFor(GlitchEngine::EffectType::pitchRise));
    primeRamp(engine);

    engine.trigger(0, true);
    constexpr float expected[] {
        94.0f,
        95.1456337f,
        96.6892700f,
        95.0435028f,
        95.0435028f,
        95.0435028f
    };
    for (int i = 0; i < static_cast<int>(std::size(expected)); ++i)
    {
        const auto output = engine.processSample(100.0f + static_cast<float>(i), 100.0f + static_cast<float>(i), 1.0f);
        expectNear(test, output.left, expected[i]);
    }
}

void reverseRecapturesOverStutterLikeAlphaSix()
{
    constexpr auto test = "reverseRecapturesOverStutterLikeAlphaSix";
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

void temporalIntensityDoesNotBecomeDryMix()
{
    constexpr auto test = "temporalIntensityDoesNotBecomeDryMix";
    GlitchEngine engine;
    engine.prepare(1000.0, 2);
    auto config = configFor(GlitchEngine::EffectType::reverse);
    config.intensity = 0.25f;
    engine.setSlotConfig(0, config);
    primeRamp(engine);

    engine.trigger(0, true);

    // The live sample is deliberately far away from the captured material. If
    // Intensity were still used as wet/dry, this would land near 774 instead of 97.
    const auto output = engine.processSample(1000.0f, 1000.0f, 1.0f);
    expectNear(test, output.left, 97.0f);
}

void globalMixStillControlsDryWet()
{
    constexpr auto test = "globalMixStillControlsDryWet";
    GlitchEngine engine;
    engine.prepare(1000.0, 2);
    engine.setSlotConfig(0, configFor(GlitchEngine::EffectType::reverse));
    primeRamp(engine);

    engine.trigger(0, true);
    const auto output = engine.processSample(1000.0f, 1000.0f, 0.5f);
    expectNear(test, output.left, 548.5f);
}
} // namespace

int main()
{
    reverseMatchesAlphaSixLoop();
    stutterMatchesAlphaSixLoop();
    pitchDiveMatchesAlphaSixCapture();
    pitchRiseMatchesAlphaSixReadHead();
    reverseRecapturesOverStutterLikeAlphaSix();
    temporalIntensityDoesNotBecomeDryMix();
    globalMixStillControlsDryWet();
    std::cout << "GlitchEngine alpha-6 fidelity + wet-path tests passed\n";
    return EXIT_SUCCESS;
}
