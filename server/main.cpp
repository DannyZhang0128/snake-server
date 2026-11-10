
#include "App.h"
#include "world.h"
#include <chrono>
#include <cstdio>

constexpr int kTickHz = 20;
constexpr float kDt = 1.0f / kTickHz;

//data attached to each connection.
struct PerSocketData {
    uint32_t snakeId = 0;
};


struct Game {
    World world;
    uint64_t tickCount = 0;
    double sumMs = 0.0, maxMs = 0.0;
} 
Game game;

//runs every 1000/kTickHz ms
static void onTick(struct us_timer_t *) {
    using clock = std::chrono::steady_clock;
    auto t0 = clock::now();

    game.world.tick(kDt);

    double ms = std::chrono::duration<double, std::milli>(clock::now() - t0).count();
    game.sumMs += ms;
    game.maxMs = std::max(game.maxMs, ms);
    ++game.tickCount;

    if (game.tickCount % kTickHz == 0) {
        std::printf("tick %6llu | snakes %zu | food %zu | avg %.3f ms | max %.3f ms\n",
                    static_cast<unsigned long long>(game.tickCount),
                    game.world.snakes().size(), game.world.food().size(),
                    game.sumMs / kTickHz, game.maxMs);
        game.sumMs = 0.0;
        game.maxMs = 0.0;
    }
}

int main() {
    uWS::App app;

    app.ws<PerSocketData>("/*", {
        .open = [](auto *ws) {
            auto *data = ws->getUserData();
            data->snakeId = game.world.addSnake();
            std::printf("player %u joined (%zu snakes)\n",
                        data->snakeId, game.world.snakes().size());
        },
        .message = [](auto *, std::string_view, uWS::OpCode) {
            
        },
        .close = [](auto *ws, int, std::string_view) {
            auto *data = ws->getUserData();
            game.world.removeSnake(data->snakeId);
            std::printf("player %u left (%zu snakes)\n",
                        data->snakeId, game.world.snakes().size());
        }
    });

    //repeating tick timer
    struct us_timer_t *timer = us_create_timer((struct us_loop_t *) uWS::Loop::get(), 0, 0);
    us_timer_set(timer, onTick, 1000 / kTickHz, 1000 / kTickHz);

    app.listen(9001, [](auto *token) {
        std::printf(token ? "listening on 9001\n" : "failed to listen\n");
    });

    app.run();
}
