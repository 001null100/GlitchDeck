#pragma once

#include <algorithm>
#include <charconv>
#include <cmath>
#include <string_view>

namespace glitchdeck::ui
{
// UI percentages are 0..100; the existing CLAP parameter identity remains 0..1.
// Invalid/unfinished text preserves the current value rather than jumping to zero.
inline double parsePercent(std::string_view text, double current) noexcept
{
    const auto trim = [](std::string_view value) {
        const auto first = value.find_first_not_of(" \t\r\n");
        if (first == std::string_view::npos) return std::string_view{};
        const auto last = value.find_last_not_of(" \t\r\n");
        return value.substr(first, last - first + 1);
    };
    text = trim(text);
    if (!text.empty() && text.back() == '%') text.remove_suffix(1);
    text = trim(text);
    if (!text.empty() && text.front() == '+') text.remove_prefix(1);
    if (text.empty()) return current;
    double percentage = 0.0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), percentage);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size() || !std::isfinite(percentage))
        return current;
    return std::clamp(percentage / 100.0, 0.0, 1.0);
}
} // namespace glitchdeck::ui
