// Unit tests for the pure logic: game rules, input parsing, origin checks, config.
// Build and run with: make test-unit
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

#include "config.h"
#include "input.h"
#include "origin.h"
#include "world.h"

//---------- helpers ----------

static const Snake &snakeById(const World &w, uint32_t id) {
    for (const auto &s : w.snakes()) {
        if (s.id == id) return s;
    }
    FAIL("no snake with id " << id);
    return w.snakes().front();  // unreachable: FAIL aborts the test case
}

// Works for whatever the 2D point type in world.h is called.
template <class P>
static float distance(const P &a, const P &b) {
    return std::hypot(a.x - b.x, a.y - b.y);
}

// Drops n food items on the snake's head; the next tick eats them all (+1 length each).
static void feed(World &w, uint32_t id, int n) {
    for (int i = 0; i < n; ++i) w.spawnFood(snakeById(w, id).head);
    w.tick(kDt);
}

// Steers the snake along a scripted wandering path until it has eaten enough food.
static void wanderUntilLength(World &w, uint32_t id, int targetLength) {
    for (int t = 0; t < 20000 && snakeById(w, id).length < targetLength; ++t) {
        if (t % 20 == 0) w.setInput(id, std::sin(static_cast<float>(t) * 0.37f) * kPi, false);
        w.tick(kDt);
    }
}

//---------- World ----------

TEST_CASE("World: new snakes get unique ids, minimum length, and spawn inside the map") {
    World w;
    uint32_t a = w.addSnake();
    uint32_t b = w.addSnake();
    CHECK(a != 0);
    CHECK(a != b);
    const Snake &s = snakeById(w, a);
    CHECK(s.length == World::kMinLength);
    CHECK(s.alive);
    CHECK(s.head.x >= 0.f);
    CHECK(s.head.x <= World::kWidth);
    CHECK(s.head.y >= 0.f);
    CHECK(s.head.y <= World::kHeight);
}

TEST_CASE("World: removeSnake removes only the requested snake") {
    World w;
    uint32_t a = w.addSnake();
    uint32_t b = w.addSnake();
    w.removeSnake(a);
    REQUIRE(w.snakes().size() == 1);
    CHECK(w.snakes()[0].id == b);
}

TEST_CASE("World: input for an unknown snake id is ignored") {
    World w;
    uint32_t a = w.addSnake();
    float targetBefore = snakeById(w, a).targetAngle;
    w.setInput(9999, 1.f, true);
    CHECK(snakeById(w, a).targetAngle == doctest::Approx(targetBefore));
    CHECK_FALSE(snakeById(w, a).boosting);
}

TEST_CASE("World: snake turns toward the target, limited by the turn rate") {
    World w;
    uint32_t id = w.addSnake();
    float a0 = snakeById(w, id).angle;
    w.setInput(id, a0 + 3.0f, false);  // far away, so the turn is capped
    w.tick(kDt);
    CHECK(snakeById(w, id).angle - a0 == doctest::Approx(World::kTurnRate * kDt));
}

TEST_CASE("World: snake turns the short way around when the target wraps past +-pi") {
    World w;
    uint32_t id = w.addSnake();
    float a0 = snakeById(w, id).angle;
    //+3.5 rad is the same heading as -2.78 rad, so the short way is a negative turn.
    w.setInput(id, a0 + 3.5f, false);
    w.tick(kDt);
    CHECK(snakeById(w, id).angle - a0 == doctest::Approx(-World::kTurnRate * kDt));
}

TEST_CASE("World: snake settles exactly on the target heading") {
    World w;
    uint32_t id = w.addSnake();
    float target = snakeById(w, id).angle + 1.0f;
    w.setInput(id, target, false);
    for (int i = 0; i < 60; ++i) w.tick(kDt);
    CHECK(snakeById(w, id).angle == doctest::Approx(target).epsilon(1e-4));
}

TEST_CASE("World: without boost a snake moves at base speed") {
    World w(1, 0);
    uint32_t id = w.addSnake();
    auto before = snakeById(w, id).head;
    w.setInput(id, snakeById(w, id).angle, false);  // already facing this way: no turning
    w.tick(kDt);
    CHECK(distance(before, snakeById(w, id).head) ==
          doctest::Approx(World::kBaseSpeed * kDt).epsilon(1e-3));
}

TEST_CASE("World: boost is ignored at minimum length, so length never drops below it") {
    World w(1, 0);
    uint32_t id = w.addSnake();
    auto before = snakeById(w, id).head;
    w.setInput(id, snakeById(w, id).angle, true);
    w.tick(kDt);
    CHECK(distance(before, snakeById(w, id).head) ==
          doctest::Approx(World::kBaseSpeed * kDt).epsilon(1e-3));
    for (int i = 0; i < 400; ++i) w.tick(kDt);
    CHECK(snakeById(w, id).length == World::kMinLength);
}

TEST_CASE("World: eating food grows the snake and food is replenished") {
    World w;
    uint32_t id = w.addSnake();
    wanderUntilLength(w, id, 15);
    REQUIRE(snakeById(w, id).length >= 15);
    CHECK(w.food().size() == World::kFoodTarget);
}

TEST_CASE("World: each food item eaten adds exactly one length") {
    World w(1, 0);
    uint32_t id = w.addSnake();
    feed(w, id, 3);
    CHECK(snakeById(w, id).length == World::kMinLength + 3);
}

TEST_CASE("World: boosting a long snake moves at boost speed") {
    World w(1, 0);
    uint32_t id = w.addSnake();
    feed(w, id, 10);
    auto before = snakeById(w, id).head;
    w.setInput(id, snakeById(w, id).angle, true);
    w.tick(kDt);
    CHECK(distance(before, snakeById(w, id).head) ==
          doctest::Approx(World::kBoostSpeed * kDt).epsilon(1e-3));
}

TEST_CASE("World: boosting costs about kBoostCostPerSec length per second") {
    World w(1, 0);
    uint32_t id = w.addSnake();
    feed(w, id, 10);  // length 20
    w.setInput(id, snakeById(w, id).angle, true);
    for (int i = 0; i < 50; ++i) w.tick(kDt);  // 2.5 s of boosting, costs about 5
    int length = snakeById(w, id).length;
    CHECK(length >= 15);  // 20 - 5, give or take one for float rounding
    CHECK(length <= 16);
}

TEST_CASE("World: boosting stops at the minimum length instead of going below it") {
    World w(1, 0);
    uint32_t id = w.addSnake();
    feed(w, id, 10);
    w.setInput(id, snakeById(w, id).angle, true);
    for (int i = 0; i < 1000; ++i) w.tick(kDt);
    CHECK(snakeById(w, id).length == World::kMinLength);
}

TEST_CASE("World: the head can never leave the map") {
    World w;
    uint32_t id = w.addSnake();
    w.setInput(id, 0.f, false);  // head east for far longer than it takes to hit the wall
    for (int i = 0; i < 2000; ++i) w.tick(kDt);
    const Snake &s = snakeById(w, id);
    CHECK(s.head.x <= World::kWidth);
    CHECK(s.head.x == doctest::Approx(World::kWidth));
    CHECK(s.head.y >= 0.f);
    CHECK(s.head.y <= World::kHeight);
}

//---------- parseInput ----------

TEST_CASE("parseInput accepts well-formed input") {
    float angle = 0.f;
    bool boost = false;

    CHECK(parseInput("1.5708,1", angle, boost));
    CHECK(angle == doctest::Approx(1.5708f));
    CHECK(boost);

    CHECK(parseInput("-3.14,0", angle, boost));
    CHECK(angle == doctest::Approx(-3.14f));
    CHECK_FALSE(boost);

    CHECK(parseInput("0,0", angle, boost));
    CHECK(parseInput("10,1", angle, boost));
}

TEST_CASE("parseInput rejects malformed or hostile input") {
    float angle = 0.f;
    bool boost = false;
    const std::vector<std::string> bad = {
        "",       "garbage", "1.5",   ",1",    "1.5,",   "1.5,2",   "1.5,01", "1.5abc,1", " 1.5,1",
        "1.5,1 ", "1.5,1,1", "nan,0", "inf,1", "-inf,0", "10.01,0", "999,0",  "-10.5,1",
    };
    for (const auto &msg : bad) {
        CHECK_MESSAGE(!parseInput(msg, angle, boost), "should reject: '" << msg << "'");
    }
}

TEST_CASE("parseInput rejects over-long messages even if the number is valid") {
    float angle = 0.f;
    bool boost = false;
    std::string longButValid = "0." + std::string(40, '0') + ",1";
    CHECK(longButValid.size() > kMaxInputBytes);
    CHECK_FALSE(parseInput(longButValid, angle, boost));
}

TEST_CASE("parseInput leaves its outputs untouched when it fails") {
    float angle = 7.f;  // sentinel values
    bool boost = true;
    CHECK_FALSE(parseInput("nan,0", angle, boost));
    CHECK_FALSE(parseInput("1.5,2", angle, boost));
    CHECK(angle == doctest::Approx(7.f));
    CHECK(boost);
}

//---------- origin checks ----------

TEST_CASE("parseOriginList splits, trims, and skips empty entries") {
    auto v = parseOriginList(" http://a.example , http://b.example,, ");
    REQUIRE(v.size() == 2);
    CHECK(v[0] == "http://a.example");
    CHECK(v[1] == "http://b.example");
    CHECK(parseOriginList("").empty());
}

TEST_CASE("isOriginAllowed: exact matches and non-browser clients only") {
    const std::vector<std::string> allowed = {"http://localhost:8000"};
    CHECK(isOriginAllowed("http://localhost:8000", allowed));
    CHECK(isOriginAllowed("", allowed));  // no Origin header: bot or test script
    CHECK_FALSE(isOriginAllowed("http://evil.example", allowed));
    CHECK_FALSE(isOriginAllowed("http://localhost:8001", allowed));               // different port
    CHECK_FALSE(isOriginAllowed("http://localhost:8000.evil.example", allowed));  // prefix trick
    CHECK_FALSE(isOriginAllowed("null", allowed));  // pages opened from file://
}

TEST_CASE("getAllowedOrigins reads ALLOWED_ORIGINS and falls back to the default") {
    unsetenv("ALLOWED_ORIGINS");
    CHECK(isOriginAllowed("http://localhost:8000", getAllowedOrigins()));
    setenv("ALLOWED_ORIGINS", "https://game.example", 1);
    CHECK(isOriginAllowed("https://game.example", getAllowedOrigins()));
    CHECK_FALSE(isOriginAllowed("http://localhost:8000", getAllowedOrigins()));
    unsetenv("ALLOWED_ORIGINS");
}

// ---------- config ----------

TEST_CASE("getPort reads PORT and falls back to the default for bad values") {
    unsetenv("PORT");
    CHECK(getPort() == kDefaultPort);
    setenv("PORT", "9002", 1);
    CHECK(getPort() == 9002);
    for (const char *bad : {"abc", "0", "65536", "-5", "12x", ""}) {
        setenv("PORT", bad, 1);
        CHECK_MESSAGE(getPort() == kDefaultPort, "PORT='" << bad << "' should fall back");
    }
    unsetenv("PORT");
}