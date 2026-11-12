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
    static constexpr size_t kFoodTarget = 2000;  // total number of food items in the world

    explicit World(uint32_t seed = 1, size_t foodTarget = kFoodTarget)
        : rng_(seed), foodTarget_(foodTarget) {
        food_.reserve(foodTarget_);
        topUpFood();
    }

    // Places one food item for tests
    void spawnFood(point p) { food_.push_back(p); }

    uint32_t addSnake() {
        Snake s;
        s.id = nextId_++;
        s.head = {randRange(500.f, kWidth - 500.f),
                  randRange(500.f, kHeight - 500.f)};    // spawn 500 units away from borders
        s.angle = s.targetAngle = randRange(-kPi, kPi);  // random starting angle
        s.length = kMinLength;
        s.body.push_front(s.head);
        snakes_.push_back(std::move(s));  // sends data to vector without copying, s is now empty
        return snakes_.back().id;
    }

    void removeSnake(uint32_t id) {
        // removes all elements that match s.id == id, [&] means its an inline func, allowing access
        // to local variables like id
        std::erase_if(snakes_, [&](const Snake &s) { return s.id == id; });
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
        // each tick checks alive state, moves snakes, food check, and restores food.
        // need to add collison detection
        for (auto &s : snakes_) {
            if (!s.alive) continue;
            stepSnake(s, t);
            eatFood(s);
        }
        topUpFood();
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
        // move the head of the snake, limited by borders
        s.head.x = std::clamp(s.head.x + std::cos(s.angle) * speed * t, 0.f, kWidth);
        s.head.y = std::clamp(s.head.y + std::sin(s.angle) * speed * t, 0.f, kHeight);

        // moves head to the front, remove tails until length is correct
        s.body.push_front(s.head);
        while (static_cast<int>(s.body.size()) > s.length) s.body.pop_back();
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

    std::mt19937 rng_;
    size_t foodTarget_;
    std::vector<Snake> snakes_;
    std::vector<point> food_;
    uint32_t nextId_ = 1;
};
