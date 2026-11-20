// Tests for the browser client's logic (death, respawn, camera). No browser needed: the real
// client/index.html script runs in Node against a fake canvas and a fake WebSocket.
// Run with `make test-integration` from the server/ folder.
const { test } = require("node:test");
const assert = require("node:assert/strict");
const vm = require("node:vm");
const fs = require("node:fs");
const path = require("node:path");

const HTML = fs.readFileSync(path.join(__dirname, "..", "..", "..", "client", "index.html"), "utf8");
const SCRIPT = HTML.match(/<script>([\s\S]*)<\/script>/)[1];

// Loads a fresh copy of the client and returns helpers to drive it.
function loadClient() {
  const handlers = {};
  const sent = [];
  const texts = [];
  const translates = [];
  const state = { clock: 0, ws: null };

  const ctx = new Proxy({}, {
    get(target, prop) {
      if (prop === "fillText") return (t) => texts.push(t);
      if (prop === "translate") return (x, y) => translates.push([x, y]);
      if (prop in target) return target[prop];
      return () => {};
    },
    set(target, prop, value) { target[prop] = value; return true; },
  });
  const canvas = { width: 800, height: 600, getContext: () => ctx };

  class FakeWebSocket {
    constructor() { this.readyState = 1; state.ws = this; }
    send(message) { sent.push(message); }
  }
  FakeWebSocket.OPEN = 1;

  const sandbox = {
    document: { getElementById: () => canvas },
    innerWidth: 800, innerHeight: 600,
    addEventListener: (type, fn) => { (handlers[type] = handlers[type] || []).push(fn); },
    location: { search: "", hostname: "localhost" },
    WebSocket: FakeWebSocket,
    performance: { now: () => state.clock },
    requestAnimationFrame: () => {},
    setInterval: () => {},
    URLSearchParams, JSON, Math, console,
  };
  vm.createContext(sandbox);
  vm.runInContext(SCRIPT, sandbox);
  state.ws.onopen();

  return {
    sent,
    texts,
    translates,
    deliver: (msg) => state.ws.onmessage({ data: JSON.stringify(msg) }),
    frame: () => { texts.length = 0; translates.length = 0; vm.runInContext("draw()", sandbox); },
    fire: (type, event = {}) => (handlers[type] || []).forEach((fn) => fn(event)),
    get: (name) => vm.runInContext(name, sandbox),
    setClock: (ms) => { state.clock = ms; },
    respawnCount: () => sent.filter((m) => m === "respawn").length,
  };
}

const snake = (id, x, y, len = 10) => ({ id, len, p: [[x, y], [x - 10, y]] });

// A client that has joined as player 7 and sees itself and player 8.
function joinedClient() {
  const c = loadClient();
  c.deliver({ t: "welcome", id: 7, w: 4000, h: 4000 });
  c.deliver({ t: "state", snakes: [snake(7, 1500, 900), snake(8, 1600, 900)], food: [] });
  return c;
}

test("the HUD shows our length while alive", () => {
  const c = joinedClient();
  c.frame();
  assert.equal(c.get("dead"), false);
  assert.ok(c.texts.includes("length: 10"), `HUD text: ${c.texts}`);
});

test("another player's death does not mark us dead", () => {
  const c = joinedClient();
  c.deliver({ t: "dead", id: 8 });
  assert.equal(c.get("dead"), false);
});

test("our own death shows the overlay and keeps the camera where we died", () => {
  const c = joinedClient();
  c.frame();
  const cameraWhileAlive = c.translates[0];

  c.setClock(5000);
  c.deliver({ t: "dead", id: 7 });
  c.deliver({ t: "state", snakes: [snake(8, 1610, 900)], food: [] });
  c.frame();

  assert.equal(c.get("dead"), true);
  assert.ok(c.texts.includes("You died"), `HUD text: ${c.texts}`);
  assert.deepEqual(c.translates[0], cameraWhileAlive);
});

test("a click right after dying does not respawn, a later click or Enter does", () => {
  const c = joinedClient();
  c.setClock(5000);
  c.deliver({ t: "dead", id: 7 });

  c.setClock(5300);
  c.fire("mousedown");
  assert.equal(c.respawnCount(), 0, "too soon after dying");
  assert.equal(c.get("boost"), false, "clicking while dead must not start boosting");

  c.setClock(6000);
  c.fire("mousedown");
  assert.equal(c.respawnCount(), 1);
  c.fire("keydown", { code: "Enter", preventDefault() {} });
  assert.equal(c.respawnCount(), 2);
  assert.equal(c.get("dead"), true, "still dead until the server confirms");
});

test("a state that contains our snake again ends the dead state", () => {
  const c = joinedClient();
  c.setClock(5000);
  c.deliver({ t: "dead", id: 7 });
  c.deliver({ t: "state", snakes: [snake(8, 1610, 900)], food: [] });
  assert.equal(c.get("dead"), true);

  c.deliver({ t: "state", snakes: [snake(7, 2500, 2500), snake(8, 1620, 900)], food: [] });
  c.frame();
  assert.equal(c.get("dead"), false);
  assert.ok(!c.texts.includes("You died"));
  assert.ok(c.texts.includes("length: 10"));
});

test("a frame drawn between the dead message and the next state cannot hide the overlay", () => {
  const c = joinedClient();
  c.setClock(5000);
  c.deliver({ t: "dead", id: 7 });
  c.frame(); // the old state still contains our snake here
  c.deliver({ t: "state", snakes: [snake(8, 1610, 900)], food: [] });
  c.frame();
  assert.equal(c.get("dead"), true);
  assert.ok(c.texts.includes("You died"), `HUD text: ${c.texts}`);
});

test("while alive a click boosts and never sends respawn", () => {
  const c = joinedClient();
  c.fire("mousedown");
  assert.equal(c.get("boost"), true);
  assert.equal(c.respawnCount(), 0);
});
