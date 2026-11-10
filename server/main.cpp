
#include "App.h"
#include "world.h"
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>
#include <string_view>

constexpr int kTickHz = 20;
constexpr float kDt = 1.0f / kTickHz;

//data attached to each connection.
struct PerSocketData {
    uint32_t snakeId = 0;
};


struct Game {
    World world;
    uWS::App *app = nullptr;
    std::string buf;                 //reused every tick to avoid reallocating
    uint64_t tickCount = 0;
    double sumMs = 0.0, maxMs = 0.0;
    size_t lastBytes = 0;
};
Game game;

template <class T>
static void appendNum(std::string &s, T v) {
    char tmp[24];
    auto r = std::to_chars(tmp, tmp + sizeof tmp, v);
    s.append(tmp, r.ptr);
}

//serialize the whole world as JSON.
static void buildState(std::string &out, const World &w) {
    out.clear();
    out += "{\"t\":\"state\",\"snakes\":[";
    bool firstSnake = true;
    for (const auto &s : w.snakes()) {
        if (!s.alive) continue;
        if (!firstSnake) out += ',';
        firstSnake = false;
        out += "{\"id\":";  appendNum(out, s.id);
        out += ",\"len\":"; appendNum(out, s.length);
        out += ",\"p\":[";
        for (size_t i = 0; i < s.body.size(); ++i) {
            if (i) out += ',';
            out += '[';
            appendNum(out, static_cast<int>(s.body[i].x));
            out += ',';
            appendNum(out, static_cast<int>(s.body[i].y));
            out += ']';
        }
        out += "]}";
    }
    out += "],\"food\":[";
    const auto &food = w.food();
    for (size_t i = 0; i < food.size(); ++i) {
        if (i) out += ',';
        out += '[';
        appendNum(out, static_cast<int>(food[i].x));
        out += ',';
        appendNum(out, static_cast<int>(food[i].y));
        out += ']';
    }
    out += "]}";
}

//Input format: "<angle>,<0|1>" 
static bool parseInput(std::string_view msg, float &angle, bool &boost) {
    if (msg.size() > 32) return false;
    size_t comma = msg.find(',');
    if (comma == std::string_view::npos || comma == 0) return false;

    const char *begin = msg.data();
    const char *end = msg.data() + comma;
    auto [ptr, ec] = std::from_chars(begin, end, angle);
    if (ec != std::errc() || ptr != end) return false;           // not a clean number
    if (!std::isfinite(angle) || std::fabs(angle) > 10.f) return false;   // NaN, inf, absurd

    std::string_view flag = msg.substr(comma + 1);
    if (flag != "0" && flag != "1") return false;
    boost = (flag == "1");
    return true;
}

//runs every 1000/kTickHz ms
static void onTick(struct us_timer_t *) {
    using clock = std::chrono::steady_clock;
    auto t0 = clock::now();

    game.world.tick(kDt);

    if (!game.world.snakes().empty()) {
        buildState(game.buf, game.world);
        game.app->publish("state", game.buf, uWS::OpCode::TEXT);   //one send to all subscribers
        game.lastBytes = game.buf.size();
    }

    double ms = std::chrono::duration<double, std::milli>(clock::now() - t0).count();
    game.sumMs += ms;
    game.maxMs = std::max(game.maxMs, ms);
    ++game.tickCount;

    if (game.tickCount % kTickHz == 0) {
        std::printf("tick %6llu | snakes %zu | msg %zu B | avg %.3f ms | max %.3f ms\n",
                    static_cast<unsigned long long>(game.tickCount),
                    game.world.snakes().size(), game.lastBytes,
                    game.sumMs / kTickHz, game.maxMs);
        game.sumMs = 0.0;
        game.maxMs = 0.0;
    }
}

int main() {
    uWS::App app;
    game.app = &app;

    app.ws<PerSocketData>("/*", {
        .open = [](auto *ws) {
            auto *data = ws->getUserData();
            data->snakeId = game.world.addSnake();
            ws->subscribe("state");

            char hello[96];
            int n = std::snprintf(hello, sizeof hello,
                "{\"t\":\"welcome\",\"id\":%u,\"w\":%d,\"h\":%d}",
                data->snakeId, static_cast<int>(World::kWidth), static_cast<int>(World::kHeight));
            ws->send(std::string_view(hello, static_cast<size_t>(n)), uWS::OpCode::TEXT);

            std::printf("player %u joined (%zu snakes)\n",
                        data->snakeId, game.world.snakes().size());
        },
        .message = [](auto *ws, std::string_view msg, uWS::OpCode op) {
            if (op != uWS::OpCode::TEXT) return;
            float angle = 0.f;
            bool boost = false;
            if (!parseInput(msg, angle, boost)) return;   // silently drop bad input
            game.world.setInput(ws->getUserData()->snakeId, angle, boost);
        },
        .close = [](auto *ws, int, std::string_view) {
            auto *data = ws->getUserData();
            game.world.removeSnake(data->snakeId);
            std::printf("player %u left (%zu snakes)\n",
                        data->snakeId, game.world.snakes().size());
        }
    });

    struct us_timer_t *timer = us_create_timer((struct us_loop_t *) uWS::Loop::get(), 0, 0);
    us_timer_set(timer, onTick, 1000 / kTickHz, 1000 / kTickHz);

    app.listen(9001, [](auto *token) {
        std::printf(token ? "listening on 9001\n" : "failed to listen\n");
    });

    app.run();
}