#pragma once
// Origin checking for WebSocket connections.
// Browsers attach an Origin header to every WebSocket handshake
// so a malicious website could make a visitor's browser connect to our server.
// We only accept origins on an allow-list. 

#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

#include "config.h"

//Splits "a, b,c" into {"a", "b", "c"}
inline std::vector<std::string> parseOriginList(std::string_view csv) {
    std::vector<std::string> out;
    size_t pos = 0;
    while (pos <= csv.size()) {
        size_t comma = csv.find(',', pos);
        if (comma == std::string_view::npos) comma = csv.size();
        std::string_view item = csv.substr(pos, comma - pos);
        while (!item.empty() && item.front() == ' ') item.remove_prefix(1);
        while (!item.empty() && item.back() == ' ') item.remove_suffix(1);
        if (!item.empty()) out.emplace_back(item);
        pos = comma + 1;
    }
    return out;
}

inline bool isOriginAllowed(std::string_view origin, const std::vector<std::string> &allowed) {
    if (origin.empty()) return true;  //non-browser client
    for (const auto &a : allowed) {
        if (origin == a) return true;
    }
    return false;
}

//ALLOWED_ORIGINS if set, otherwise the default list.
inline std::vector<std::string> getAllowedOrigins() {
    const char *env = std::getenv("ALLOWED_ORIGINS");
    return parseOriginList(env ? std::string_view(env) : kDefaultAllowedOrigins);
}