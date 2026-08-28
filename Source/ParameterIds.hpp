#pragma once

#include <nullclap/Id.hpp>
#include <array>
#include <string>
#include <string_view>

namespace glitchdeck::ids
{
inline constexpr clap_id mix = nullclap::stableId("glitchdeck.mix");
inline constexpr clap_id audioInput = nullclap::stableId("glitchdeck.audio.main-in");
inline constexpr clap_id audioOutput = nullclap::stableId("glitchdeck.audio.main-out");
inline constexpr clap_id midiInput = nullclap::stableId("glitchdeck.note.midi-in");
inline constexpr clap_id performanceRemote = nullclap::stableId("glitchdeck.remote.performance");

struct Slot
{
    clap_id trigger = CLAP_INVALID_ID;
    clap_id effect = CLAP_INVALID_ID;
    clap_id latch = CLAP_INVALID_ID;
    clap_id midiType = CLAP_INVALID_ID;
    clap_id midiNumber = CLAP_INVALID_ID;
    clap_id midiChannel = CLAP_INVALID_ID;
    clap_id quantize = CLAP_INVALID_ID;
    clap_id stereo = CLAP_INVALID_ID;
    clap_id intensity = CLAP_INVALID_ID;
    clap_id length = CLAP_INVALID_ID;
    clap_id attack = CLAP_INVALID_ID;
    clap_id release = CLAP_INVALID_ID;
    clap_id shape = CLAP_INVALID_ID;
};

inline Slot makeSlot(int slot)
{
    const auto prefix = std::string("glitchdeck.slot") + std::to_string(slot + 1) + ".";
    return {
        nullclap::stableId(prefix + "trigger"),
        nullclap::stableId(prefix + "effect"),
        nullclap::stableId(prefix + "latch"),
        nullclap::stableId(prefix + "midi-type"),
        nullclap::stableId(prefix + "midi-number"),
        nullclap::stableId(prefix + "midi-channel"),
        nullclap::stableId(prefix + "quantize"),
        nullclap::stableId(prefix + "stereo"),
        nullclap::stableId(prefix + "intensity"),
        nullclap::stableId(prefix + "length-ms"),
        nullclap::stableId(prefix + "attack-ms"),
        nullclap::stableId(prefix + "release-ms"),
        nullclap::stableId(prefix + "shape"),
    };
}

inline std::array<Slot, 8> makeSlots()
{
    std::array<Slot, 8> result {};
    for (int i = 0; i < static_cast<int>(result.size()); ++i)
        result[static_cast<std::size_t>(i)] = makeSlot(i);
    return result;
}
} // namespace glitchdeck::ids
