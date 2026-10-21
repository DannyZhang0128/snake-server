#include "world.h"
#include <chrono>
#include <cstdio>
#include <thread>

int main() {
    using clock = std::chrono::steady_clock;
    constexpr int kTickHz = 20;
    constexpr auto kTickDur = std::chrono::nanoseconds(1'000'000'000 / kTickHz);
    constexpr float kt = 1.0f / kTickHz;

    World world;
    for (int i = 0; i < 5; i++) {world.addSnake();}

    //generates rng, random angles and a number to determine fake players decisions
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> angleDist(-kPi, kPi), choice(0.f, 1.f);

    uint64_t tickCount = 0;
    double sumMs = 0.0, maxMs = 0.0;
    auto next = clock::now();

    while (true) {
        auto t0 = clock::now();

        for (const auto& s : world.snakes()) 
        {
            //2 percent to change directions, 30 percent to boost while changing
            if (choice(rng) < 0.02f) world.setInput(s.id, angleDist(rng), choice(rng) < 0.3f);
        }
        world.tick(kt);

        double ms = std::chrono::duration<double, std::milli>(clock::now() - t0).count();
        sumMs += ms;
        maxMs = std::max(maxMs, ms);
        tickCount++;

        //sends a report every 20 ticks, 1 second
        if (tickCount % kTickHz == 0) {   
            int longest = 0;
            for (const auto& s : world.snakes()) {longest = std::max(longest, s.length);}

            std::printf("tick %6llu | snakes %zu | food %zu | avg %.3f ms | max %.3f ms | longest %d\n",
                        static_cast<unsigned long long>(tickCount),
                        world.snakes().size(), world.food().size(),
                        sumMs / kTickHz, maxMs, longest);
            sumMs = 0.0;
            maxMs = 0.0;
        }

        //sleep until next tick
        next += kTickDur;
        auto now = clock::now();
        //resyncs program if it falls atleast 1 tick behind
        if (now > next + kTickDur) next = now;   
        std::this_thread::sleep_until(next);
    }
}