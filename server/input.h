#pragma once
// Parsing and validation of the text message a client sends every tick.

#include <charconv>
#include <cmath>
#include <string_view>

#include "config.h"

//Input format: "<angle>,<0|1>" 
//Returns true and fills angle/boost only if the whole message is valid.
//On failure the outputs are left untouched.
inline bool parseInput(std::string_view msg, float &angle, bool &boost) {
    if (msg.size() > kMaxInputBytes) return false;
    size_t comma = msg.find(',');
    if (comma == std::string_view::npos || comma == 0) return false;

    const char *begin = msg.data();
    const char *end = msg.data() + comma;
    float parsed = 0.f;
    auto [ptr, ec] = std::from_chars(begin, end, parsed);
    if (ec != std::errc() || ptr != end) return false;  //not a clean number
    if (!std::isfinite(parsed) || std::fabs(parsed) > kMaxAbsAngle)
        return false;  //NaN, inf, absurd

    std::string_view flag = msg.substr(comma + 1);
    if (flag != "0" && flag != "1") return false;

    angle = parsed;
    boost = (flag == "1");
    return true;
}