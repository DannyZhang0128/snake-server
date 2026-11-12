#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

#include "App.h"
#include "config.h"
#include "input.h"
#include "log.h"
#include "origin.h"
#include "world.h"

// Data attached to each connection.
struct PerSocketData {
    uint32_t snakeId = 0;
};

struct Game {
    World world;
    uWS::App *app = nullptr;
    std::vector<std::string> allowedOrigins;
    std::string buf;  // reused every tick to avoid reallocating
    uint64_t tickCount = 0;
    uint64_t rejectedInputs = 0;
    double sumMs = 0.0, maxMs = 0.0;
    size_t lastBytes = 0;
};
// Created once at startup; if this allocation fails we are out of memory and terminating is fine.
// NOLINTNEXTLINE(bugprone-throwing-static-initialization)
Game game;

template <class T>
static void appendNum(std::string &s, T v) {
    char tmp[24];
    auto r = std::to_chars(tmp, tmp + sizeof tmp, v);
    s.append(tmp, r.ptr);
}

// Appends "[x,y]" with the coordinates truncated to integers.
static void appendPoint(std::string &out, float x, float y) {
    out += '[';
    appendNum(out, static_cast<int>(x));
    out += ',';
    appendNum(out, static_cast<int>(y));
    out += ']';
}

// Serialize the whole world as JSON.
static void buildState(std::string &out, const World &w) {
    out.clear();
    out += "{\"t\":\"state\",\"snakes\":[";
    bool firstSnake = true;
    for (const auto &s : w.snakes()) {
        if (!s.alive) continue;
        if (!firstSnake) out += ',';
        firstSnake = false;
        out += "{\"id\":";
        appendNum(out, s.id);
        out += ",\"len\":";
        appendNum(out, s.length);
        out += ",\"p\":[";
        for (size_t i = 0; i < s.body.size(); ++i) {
            if (i) out += ',';
            appendPoint(out, s.body[i].x, s.body[i].y);
        }
        out += "]}";
    }
    out += "],\"food\":[";
    const auto &food = w.food();
    for (size_t i = 0; i < food.size(); ++i) {
        if (i) out += ',';
        appendPoint(out, food[i].x, food[i].y);
    }
    out += "]}";
}

// Runs every kTickIntervalMs on the event loop's thread.
static void onTick(struct us_timer_t *) {
    using clock = std::chrono::steady_clock;
    auto t0 = clock::now();

    game.world.tick(kDt);

    if (!game.world.snakes().empty()) {
        buildState(game.buf, game.world);
        game.app->publish(kStateTopic, game.buf, uWS::OpCode::TEXT);  // one send to all subscribers
        game.lastBytes = game.buf.size();
    }

    double ms = std::chrono::duration<double, std::milli>(clock::now() - t0).count();
    game.sumMs += ms;
    game.maxMs = std::max(game.maxMs, ms);
    ++game.tickCount;

    if (game.tickCount % kTickHz == 0) {
        logMsg(LogLevel::Info,
               "tick %llu | snakes %zu | msg %zu B | avg %.3f ms | max %.3f ms | rejected %llu",
               static_cast<unsigned long long>(game.tickCount), game.world.snakes().size(),
               game.lastBytes, game.sumMs / kTickHz, game.maxMs,
               static_cast<unsigned long long>(game.rejectedInputs));
        game.sumMs = 0.0;
        game.maxMs = 0.0;
    }
}

int main() {
    const int port = getPort();
    game.allowedOrigins = getAllowedOrigins();
    uWS::App app;
    game.app = &app;

    app.ws<PerSocketData>(
        "/*",
        {.maxPayloadLength = kMaxPayloadBytes,
         .idleTimeout = kIdleTimeoutSec,
         .upgrade =
             [](auto *res, auto *req, auto *context) {
                 std::string_view origin = req->getHeader("origin");
                 if (!isOriginAllowed(origin, game.allowedOrigins)) {
                     // Log at most 100 characters of the header.
                     std::string shown(origin.substr(0, 100));
                     logMsg(LogLevel::Warn, "rejected connection from origin '%s'", shown.c_str());
                     res->writeStatus("403 Forbidden")->end("Origin not allowed");
                     return;
                 }
                 res->template upgrade<PerSocketData>({}, req->getHeader("sec-websocket-key"),
                                                      req->getHeader("sec-websocket-protocol"),
                                                      req->getHeader("sec-websocket-extensions"),
                                                      context);
             },
         .open =
             [](auto *ws) {
                 auto *data = ws->getUserData();
                 data->snakeId = game.world.addSnake();
                 ws->subscribe(kStateTopic);

                 char hello[96];
                 int n = std::snprintf(hello, sizeof hello,
                                       "{\"t\":\"welcome\",\"id\":%u,\"w\":%d,\"h\":%d}",
                                       data->snakeId, static_cast<int>(World::kWidth),
                                       static_cast<int>(World::kHeight));
                 ws->send(std::string_view(hello, static_cast<size_t>(n)), uWS::OpCode::TEXT);

                 logMsg(LogLevel::Info, "player %u joined (%zu snakes)", data->snakeId,
                        game.world.snakes().size());
             },
         .message =
             [](auto *ws, std::string_view msg, uWS::OpCode op) {
                 if (op != uWS::OpCode::TEXT) return;
                 float angle = 0.f;
                 bool boost = false;
                 if (!parseInput(msg, angle, boost)) {
                     ++game.rejectedInputs;  // dropped; counted in the once-per-second stats line
                     return;
                 }
                 game.world.setInput(ws->getUserData()->snakeId, angle, boost);
             },
         .close =
             [](auto *ws, int, std::string_view) {
                 auto *data = ws->getUserData();
                 game.world.removeSnake(data->snakeId);
                 logMsg(LogLevel::Info, "player %u left (%zu snakes)", data->snakeId,
                        game.world.snakes().size());
             }});

    struct us_timer_t *timer = us_create_timer((struct us_loop_t *)uWS::Loop::get(), 0, 0);
    us_timer_set(timer, onTick, kTickIntervalMs, kTickIntervalMs);

    // LIBUS_LISTEN_EXCLUSIVE_PORT: by default uSockets sets SO_REUSEPORT on Linux, which lets a
    // stale second server bind the same port and silently steal about half of the connections.
    app.listen(port, LIBUS_LISTEN_EXCLUSIVE_PORT, [port](auto *token) {
        if (!token) {
            logMsg(LogLevel::Error, "failed to listen on port %d (is it already in use?)", port);
            std::exit(1);
        }
        logMsg(LogLevel::Info, "listening on port %d", port);
    });

    app.run();
}