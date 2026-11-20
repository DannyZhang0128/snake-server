#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <random>
#include <vector>

constexpr float kPi = 3.14159265358979f;

struct point {
    float x = 0.f, y = 0.f;
};

struct Snake {
    uint32_t id = 0;
    point head;
    float angle = 0.f;        // current direction (radians)
    float targetAngle = 0.f;  // desired direction from player cursor
    bool boosting = false;
    bool alive = true;
    int length = 10;         // number of body points to keep
    float boostCost = 0.f;   // length cost for boosting, can be a fraction
    std::deque<point> body;  // body[0] is the head
};

class World {
public:
    static constexpr float kWidth = 4000.f;
    static constexpr float kHeight = 4000.f;
    static constexpr float kBaseSpeed = 200.f;  // units/sec
    static constexpr float kBoostSpeed = 400.f;
    static constexpr float kTurnRate = 4.f;         // radians/sec
    static constexpr float kEatRadius = 40.f;       // distance from head to eat food
    static constexpr float kBoostCostPerSec = 2.f;  // length lost per second
    static constexpr int kMinLength = 10;
    static constexpr size_t kFoodTarget = 2000;      // total number of food items in the world
    static constexpr float kCollisionRadius = 18.f;  // head this close to another snake is a crash
    static constexpr float kSpawnClearance = 300.f;  // spawn at least this far from living snakes

    // foodTarget is how many food items the world keeps topped up (tests pass 0 to control food).
    explicit World(uint32_t seed = 1, size_t foodTarget = kFoodTarget)
        : rng_(seed), foodTarget_(foodTarget) {
        food_.reserve(foodTarget_);
        topUpFood();
    }

    // Places one food item. Tests use it for exact control; dead snakes drop food with it too.
    void spawnFood(point p) { food_.push_back(p); }

    // adds a snake at a random spot, away from the borders and from other snakes
    uint32_t addSnake() {
        point p = randomSpawn();
        return addSnakeAt(p, randRange(-kPi, kPi));  // random starting angle
    }

    // adds a snake at an exact position and heading (tests use it for exact control)
    uint32_t addSnakeAt(point p, float angle) {
        Snake s;
        s.id = nextId_++;
        resetSnake(s, p, angle);
        snakes_.push_back(std::move(s));  // sends data to vector without copying, s is now empty
        return snakes_.back().id;
    }

    void removeSnake(uint32_t id) {
        // removes all elements that match s.id == id, [&] means its an inline func, allowing access
        // to local variables like id
        std::erase_if(snakes_, [&](const Snake &s) { return s.id == id; });
    }

    // brings a dead snake back to life with the same id. returns false if the id is unknown or the
    // snake is still alive, so a client that asks to respawn while alive changes nothing
    bool respawnSnake(uint32_t id) {
        for (auto &s : snakes_) {
            if (s.id != id) continue;
            if (s.alive) return false;
            point p = randomSpawn();
            resetSnake(s, p, randRange(-kPi, kPi));
            return true;
        }
        return false;
    }

    void setInput(uint32_t id, float angle, bool boost) {
        // records player input and changes target angle and boost state, does not move the snake
        for (auto &s : snakes_) {
            if (s.id == id) {
                s.targetAngle = angle;
                s.boosting = boost;
                return;
            }
        }
    }

    void tick(float t) {
        // 1. move every living snake
        for (auto &s : snakes_) {
            if (s.alive) stepSnake(s, t);
        }

        // 2. decide who dies. every check uses the positions after everyone has moved, so the
        // result does not depend on the order of the snakes (a head-on crash kills both)
        std::vector<bool> dying(snakes_.size(), false);
        for (size_t i = 0; i < snakes_.size(); ++i) {
            if (!snakes_[i].alive) continue;
            dying[i] = outOfBounds(snakes_[i].head) || hitsOtherSnake(i);
        }

        // 3. snakes that survived eat food
        for (size_t i = 0; i < snakes_.size(); ++i) {
            if (snakes_[i].alive && !dying[i]) eatFood(snakes_[i]);
        }

        // 4. snakes that died turn into food, then the world restores its food
        for (size_t i = 0; i < snakes_.size(); ++i) {
            if (dying[i]) kill(snakes_[i]);
        }
        topUpFood();
    }

    // ids of the snakes that died since the last call. each death is reported only once
    std::vector<uint32_t> takeDeaths() {
        std::vector<uint32_t> out;
        out.swap(deaths_);
        return out;
    }

    const std::vector<Snake> &snakes() const { return snakes_; }
    const std::vector<point> &food() const { return food_; }

private:
    // random float between low and high, uses rng_ seed.
    float randRange(float low, float high) {
        return std::uniform_real_distribution<float>(low, high)(rng_);
    }

    // computes shortest signed angle turn from a to b.
    static float angleDiff(float a, float b) {
        float d = std::fmod(b - a + kPi, 2.f * kPi);
        if (d < 0.f) d += 2.f * kPi;
        return d - kPi;
    }

    static bool outOfBounds(point p) {
        return p.x < 0.f || p.x > kWidth || p.y < 0.f || p.y > kHeight;
    }

    // puts a snake (new or respawning) at p, facing angle, at minimum length
    static void resetSnake(Snake &s, point p, float angle) {
        s.head = p;
        s.angle = s.targetAngle = angle;
        s.boosting = false;
        s.alive = true;
        s.length = kMinLength;
        s.boostCost = 0.f;
        s.body.clear();
        s.body.push_front(p);
    }

    void stepSnake(Snake &s, float t) {
        // turn towards target angle, limited by turn rate
        float maxTurn = kTurnRate * t;
        s.angle += std::clamp(angleDiff(s.angle, s.targetAngle), -maxTurn, maxTurn);

        bool boost = s.boosting && s.length > kMinLength;
        float speed = boost ? kBoostSpeed : kBaseSpeed;
        if (boost) {
            s.boostCost += kBoostCostPerSec * t;
            // removes length from snake while its boosting
            while (s.boostCost >= 1.f && s.length > kMinLength) {
                --s.length;
                s.boostCost -= 1.f;
            }
        }
        // move the head of the snake. leaving the map is checked in tick() and is fatal
        s.head.x += std::cos(s.angle) * speed * t;
        s.head.y += std::sin(s.angle) * speed * t;

        // moves head to the front, remove tails until length is correct
        s.body.push_front(s.head);
        while (static_cast<int>(s.body.size()) > s.length) s.body.pop_back();
    }

    // true if the head of snake i touches the body (or head) of any other living snake. a snake
    // never collides with itself. checks every body point, will be replaced by a spatial grid
    bool hitsOtherSnake(size_t i) const {
        const float r2 = kCollisionRadius * kCollisionRadius;
        const point head = snakes_[i].head;
        for (size_t j = 0; j < snakes_.size(); ++j) {
            if (j == i || !snakes_[j].alive) continue;
            for (const point &p : snakes_[j].body) {
                float dx = p.x - head.x;
                float dy = p.y - head.y;
                if (dx * dx + dy * dy < r2) return true;
            }
        }
        return false;
    }

    // marks the snake dead and drops one food item on each of its body points
    void kill(Snake &s) {
        s.alive = false;
        s.boosting = false;
        for (const point &p : s.body) {
            spawnFood({std::clamp(p.x, 0.f, kWidth), std::clamp(p.y, 0.f, kHeight)});
        }
        deaths_.push_back(s.id);
    }

    // inefficient food check, will be replaced
    void eatFood(Snake &s) {
        const float r2 = kEatRadius * kEatRadius;
        for (size_t i = 0; i < food_.size();) {
            float dx = food_[i].x - s.head.x;
            float dy = food_[i].y - s.head.y;
            // pythagorean theorum without the sqrt to determine if the food is in range
            if (dx * dx + dy * dy <= r2) {
                food_[i] = food_.back();  // moves eaten food to back of vector for efficiency
                food_.pop_back();
                s.length++;
            } else {
                i++;
            }
        }
    }

    void topUpFood() {
        // randomly inserts food
        while (food_.size() < foodTarget_) {
            food_.push_back({randRange(0.f, kWidth), randRange(0.f, kHeight)});
        }
    }

    // random spot 500 units from the borders. tries to stay clear of living snakes, and gives up
    // after 20 tries so a crowded map can never hang the server
    point randomSpawn() {
        point p;
        for (int attempt = 0; attempt < 20; ++attempt) {
            p = {randRange(500.f, kWidth - 500.f), randRange(500.f, kHeight - 500.f)};
            if (clearOfSnakes(p)) break;
        }
        return p;
    }

    bool clearOfSnakes(point p) const {
        const float r2 = kSpawnClearance * kSpawnClearance;
        for (const Snake &s : snakes_) {
            if (!s.alive) continue;
            for (const point &q : s.body) {
                float dx = q.x - p.x;
                float dy = q.y - p.y;
                if (dx * dx + dy * dy < r2) return false;
            }
        }
        return true;
    }

    std::mt19937 rng_;
    size_t foodTarget_;
    std::vector<Snake> snakes_;
    std::vector<point> food_;
    uint32_t nextId_ = 1;
    std::vector<uint32_t> deaths_;  // snakes that died since takeDeaths() was last called
};
