#pragma once
//Central place for tunable values and environment-based settings.
//Gameplay tuning (map size, speeds, food count) is in World (world.h).

#include <cstddef>
#include <cstdlib>
#include <string_view>

//---- Simulation timing ----
//uSockets timers take whole milliseconds, so kTickHz must divide 1000 evenly
//(20, 25, 40, 50, ...). Otherwise the real tick rate drifts away from kDt.
constexpr int kTickHz = 20;
static_assert(1000 % kTickHz == 0, "kTickHz must divide 1000 evenly");
constexpr float kDt = 1.0f / kTickHz;
constexpr int kTickIntervalMs = 1000 / kTickHz;

//---- Networking ----
constexpr int kDefaultPort = 9001;
constexpr std::string_view kStateTopic = "state";

//---- Input validation and connection limits ----
constexpr size_t kMaxInputBytes = 32; //longest valid input is about 8 bytes
constexpr float kMaxAbsAngle = 10.f; //valid angles are within +/- pi
constexpr unsigned int kMaxPayloadBytes = 64; //uWebSockets closes connections that exceed this
constexpr unsigned short kIdleTimeoutSec = 30;  //drop connections that go silent

//Reads PORT from the environment, else uses the default.
inline int getPort() {
    const char *env = std::getenv("PORT");
    if (!env) return kDefaultPort;
    char *end = nullptr;
    long v = std::strtol(env, &end, 10);
    if (end == env || *end != '\0' || v < 1 || v > 65535) return kDefaultPort;
    return static_cast<int>(v);
}