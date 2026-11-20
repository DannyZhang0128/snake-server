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

// ---------- helpers ----------

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

// Steers the snake along a scripted wandering path until it has eaten enough food. It turns back
// toward the middle of the map when it gets near a wall, so it never dies on the way.
static void wanderUntilLength(World &w, uint32_t id, int targetLength) {
    for (int t = 0; t < 20000 && snakeById(w, id).length < targetLength; ++t) {
        const auto head = snakeById(w, id).head;
        if (t % 20 == 0) w.setInput(id, std::sin(static_cast<float>(t) * 0.37f) * kPi, false);
        const float margin = 600.f;
        if (head.x < margin || head.x > World::kWidth - margin || head.y < margin ||
            head.y > World::kHeight - margin) {
            w.setInput(id, std::atan2(World::kHeight / 2 - head.y, World::kWidth / 2 - head.x),
                       false);
        }
        w.tick(kDt);
    }
}

// Ticks until the snake is dead (or maxTicks pass); returns true if it died.
static bool tickUntilDead(World &w, uint32_t id, int maxTicks) {
    for (int i = 0; i < maxTicks && snakeById(w, id).alive; ++i) w.tick(kDt);
    return !snakeById(w, id).alive;
}

// ---------- World: basics ----------

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

TEST_CASE("World: new snakes spawn away from living snakes") {
    World w(1, 0);
    uint32_t first = w.addSnakeAt({2000.f, 2000.f}, 0.f);
    std::vector<uint32_t> ids = {first};
    for (int i = 0; i < 10; ++i) ids.push_back(w.addSnake());
    for (size_t i = 0; i < ids.size(); ++i) {
        for (size_t j = i + 1; j < ids.size(); ++j) {
            CHECK(distance(snakeById(w, ids[i]).head, snakeById(w, ids[j]).head) >=
                  World::kSpawnClearance);
        }
    }
}

// ---------- World: movement ----------

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
    // +3.5 rad is the same heading as -2.78 rad, so the short way is a negative turn.
    w.setInput(id, a0 + 3.5f, false);
    w.tick(kDt);
    CHECK(snakeById(w, id).angle - a0 == doctest::Approx(-World::kTurnRate * kDt));
}

TEST_CASE("World: snake settles exactly on the target heading") {
    World w(1, 0);
    uint32_t id = w.addSnakeAt({2000.f, 2000.f}, 0.f);
    w.setInput(id, 1.0f, false);
    for (int i = 0; i < 60; ++i) w.tick(kDt);
    CHECK(snakeById(w, id).angle == doctest::Approx(1.0f).epsilon(1e-4));
}

TEST_CASE("World: without boost a snake moves at base speed") {
    World w(1, 0);
    uint32_t id = w.addSnakeAt({2000.f, 2000.f}, 0.f);
    auto before = snakeById(w, id).head;
    w.tick(kDt);
    CHECK(distance(before, snakeById(w, id).head) ==
          doctest::Approx(World::kBaseSpeed * kDt).epsilon(1e-3));
}

TEST_CASE("World: boost is ignored at minimum length, so length never drops below it") {
    World w(1, 0);
    uint32_t id = w.addSnakeAt({500.f, 2000.f}, 0.f);
    auto before = snakeById(w, id).head;
    w.setInput(id, 0.f, true);
    w.tick(kDt);
    CHECK(distance(before, snakeById(w, id).head) ==
          doctest::Approx(World::kBaseSpeed * kDt).epsilon(1e-3));
    for (int i = 0; i < 150; ++i) w.tick(kDt);
    CHECK(snakeById(w, id).alive);
    CHECK(snakeById(w, id).length == World::kMinLength);
}

// ---------- World: food and boost ----------

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
    uint32_t id = w.addSnakeAt({2000.f, 2000.f}, 0.f);
    feed(w, id, 10);
    auto before = snakeById(w, id).head;
    w.setInput(id, 0.f, true);
    w.tick(kDt);
    CHECK(distance(before, snakeById(w, id).head) ==
          doctest::Approx(World::kBoostSpeed * kDt).epsilon(1e-3));
}

TEST_CASE("World: boosting costs about kBoostCostPerSec length per second") {
    World w(1, 0);
    uint32_t id = w.addSnakeAt({500.f, 2000.f}, 0.f);
    feed(w, id, 10);  // length 20
    w.setInput(id, 0.f, true);
    for (int i = 0; i < 50; ++i) w.tick(kDt);  // 2.5 s of boosting, costs about 5
    int length = snakeById(w, id).length;
    CHECK(length >= 15);  // 20 - 5, give or take one for float rounding
    CHECK(length <= 16);
}

TEST_CASE("World: boosting stops at the minimum length instead of going below it") {
    World w(1, 0);
    uint32_t id = w.addSnakeAt({500.f, 2000.f}, 0.f);
    feed(w, id, 10);
    w.setInput(id, 0.f, true);
    for (int i = 0; i < 150; ++i) w.tick(kDt);  // far longer than it takes to run out
    CHECK(snakeById(w, id).alive);
    CHECK(snakeById(w, id).length == World::kMinLength);
}

// ---------- World: death ----------

TEST_CASE("World: a snake dies when its head leaves the map") {
    World w(1, 0);
    uint32_t id = w.addSnakeAt({3900.f, 2000.f}, 0.f);  // heading east, 100 units from the wall
    CHECK(tickUntilDead(w, id, 100));
    CHECK(snakeById(w, id).head.x > World::kWidth);
}

TEST_CASE("World: a dead snake stops moving and ignores input") {
    World w(1, 0);
    uint32_t id = w.addSnakeAt({3900.f, 2000.f}, 0.f);
    REQUIRE(tickUntilDead(w, id, 100));
    auto resting = snakeById(w, id).head;
    w.setInput(id, kPi, true);
    for (int i = 0; i < 10; ++i) w.tick(kDt);
    CHECK(distance(resting, snakeById(w, id).head) == doctest::Approx(0.f));
    CHECK_FALSE(snakeById(w, id).alive);
}

TEST_CASE("World: takeDeaths reports each death exactly once") {
    World w(1, 0);
    uint32_t id = w.addSnakeAt({3900.f, 2000.f}, 0.f);
    CHECK(w.takeDeaths().empty());
    REQUIRE(tickUntilDead(w, id, 100));
    auto deaths = w.takeDeaths();
    REQUIRE(deaths.size() == 1);
    CHECK(deaths[0] == id);
    CHECK(w.takeDeaths().empty());
    for (int i = 0; i < 10; ++i) w.tick(kDt);  // already dead: must not die again
    CHECK(w.takeDeaths().empty());
}

TEST_CASE("World: a dead snake drops one food item per body point, inside the map") {
    World w(1, 0);
    uint32_t id = w.addSnakeAt({3900.f, 2000.f}, 0.f);
    feed(w, id, 5);  // 15 body points by the time it reaches the wall
    REQUIRE(w.food().empty());
    REQUIRE(tickUntilDead(w, id, 100));
    CHECK(w.food().size() == snakeById(w, id).body.size());
    for (const auto &f : w.food()) {
        CHECK(f.x >= 0.f);
        CHECK(f.x <= World::kWidth);
        CHECK(f.y >= 0.f);
        CHECK(f.y <= World::kHeight);
    }
}

// ---------- World: snake-vs-snake collisions ----------

TEST_CASE("World: a snake that runs into another snake's body dies, the other survives") {
    World w(1, 0);
    uint32_t victim = w.addSnakeAt({1000.f, 1000.f}, 0.f);         // heading east
    uint32_t attacker = w.addSnakeAt({1050.f, 1100.f}, -kPi / 2);  // heading up, into its path
    for (int i = 0; i < 30; ++i) w.tick(kDt);
    CHECK_FALSE(snakeById(w, attacker).alive);
    CHECK(snakeById(w, victim).alive);
    auto deaths = w.takeDeaths();
    REQUIRE(deaths.size() == 1);
    CHECK(deaths[0] == attacker);
}

TEST_CASE("World: a head-on crash kills both snakes, whatever their order in the world") {
    for (bool swapOrder : {false, true}) {
        World w(1, 0);
        uint32_t a, b;
        if (!swapOrder) {
            a = w.addSnakeAt({1000.f, 1000.f}, 0.f);
            b = w.addSnakeAt({1200.f, 1000.f}, kPi);
        } else {
            b = w.addSnakeAt({1200.f, 1000.f}, kPi);
            a = w.addSnakeAt({1000.f, 1000.f}, 0.f);
        }
        for (int i = 0; i < 30; ++i) w.tick(kDt);
        CHECK_FALSE(snakeById(w, a).alive);
        CHECK_FALSE(snakeById(w, b).alive);
        CHECK(w.takeDeaths().size() == 2);
    }
}

TEST_CASE("World: a snake never collides with its own body") {
    World w(1, 0);
    uint32_t id = w.addSnakeAt({1000.f, 2000.f}, 0.f);
    feed(w, id, 30);
    // the head is always within one step (10 units) of its own previous body points
    for (int i = 0; i < 50; ++i) w.tick(kDt);
    CHECK(snakeById(w, id).alive);
}

TEST_CASE("World: dead snakes are no longer obstacles") {
    World w(1, 0);
    uint32_t wallRunner = w.addSnakeAt({3900.f, 3000.f}, 0.f);
    REQUIRE(tickUntilDead(w, wallRunner, 100));
    // a new snake placed on top of the dead snake's old body must not crash
    auto spot = snakeById(w, wallRunner).body[3];
    uint32_t other = w.addSnakeAt(spot, 0.f);
    w.tick(kDt);
    CHECK(snakeById(w, other).alive);
}

// ---------- World: respawn ----------

TEST_CASE("World: a dead snake can respawn with the same id at minimum length") {
    World w(1, 0);
    uint32_t id = w.addSnakeAt({3900.f, 2000.f}, 0.f);
    feed(w, id, 5);
    REQUIRE(tickUntilDead(w, id, 100));
    CHECK(w.respawnSnake(id));
    const Snake &s = snakeById(w, id);
    CHECK(s.alive);
    CHECK(s.length == World::kMinLength);
    CHECK(s.body.size() == 1);
    CHECK(s.head.x >= 0.f);
    CHECK(s.head.x <= World::kWidth);
    CHECK(s.head.y >= 0.f);
    CHECK(s.head.y <= World::kHeight);
    CHECK_FALSE(s.boosting);
    w.tick(kDt);
    CHECK(snakeById(w, id).alive);  // and it plays normally again
}

TEST_CASE("World: respawn does nothing for a living snake or an unknown id") {
    World w(1, 0);
    uint32_t id = w.addSnakeAt({2000.f, 2000.f}, 0.f);
    feed(w, id, 5);
    auto before = snakeById(w, id).head;
    CHECK_FALSE(w.respawnSnake(id));
    CHECK_FALSE(w.respawnSnake(9999));
    CHECK(snakeById(w, id).length == World::kMinLength + 5);
    CHECK(distance(before, snakeById(w, id).head) == doctest::Approx(0.f));
}

// ---------- parseInput ----------

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
    CHECK(parseInput("10,1", angle, boost));  // exactly at the limit
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

TEST_CASE("isRespawnRequest accepts only the exact respawn message") {
    CHECK(isRespawnRequest("respawn"));
    for (const char *no : {"", "Respawn", "respawn ", " respawn", "respawn,1", "1.5,1"}) {
        CHECK_MESSAGE(!isRespawnRequest(no), "should not match: '" << no << "'");
    }
}

// ---------- origin checks ----------

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
