//Integration tests: start the real server binary and talk to it over WebSocket.
//Run with `make test-integration` from the server/ folder.
const { test, before, after } = require("node:test");
const assert = require("node:assert/strict");
const { spawn } = require("node:child_process");
const path = require("node:path");
const WebSocket = require("ws");

const PORT = 20000 + Math.floor(Math.random() * 20000); //random, so reruns don't collide
const URL = `ws://127.0.0.1:${PORT}`;
const SERVER = path.join(__dirname, "..", "..", "server");
const ALLOWED_ORIGIN = "http://localhost:8000";

let server;
let serverLog = "";

const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

//Polls until fn() returns something truthy, or fails after `ms`.
async function waitFor(fn, ms, what) {
  const deadline = Date.now() + ms;
  while (Date.now() < deadline) {
    const value = fn();
    if (value) return value;
    await sleep(25);
  }
  assert.fail("timed out waiting for " + what);
}

//Opens a connection and remembers the welcome message and the latest world state.
function connect(options) {
  return new Promise((resolve, reject) => {
    const ws = new WebSocket(URL, options);
    const client = { ws, welcome: null, state: null, closed: false };
    ws.on("message", (data) => {
      const msg = JSON.parse(data.toString());
      if (msg.t === "welcome") client.welcome = msg;
      else if (msg.t === "state") client.state = msg;
    });
    ws.on("close", () => { client.closed = true; });
    ws.on("open", () => resolve(client));
    ws.on("error", reject);
  });
}

const mySnake = (client) =>
  client.state && client.welcome && client.state.snakes.find((s) => s.id === client.welcome.id);

before(async () => {
  server = spawn(SERVER, [], {
    env: { ...process.env, PORT: String(PORT), ALLOWED_ORIGINS: ALLOWED_ORIGIN },
  });
  server.stdout.on("data", (chunk) => { serverLog += chunk; });
  server.stderr.on("data", (chunk) => { serverLog += chunk; });
  await waitFor(() => serverLog.includes("listening on port"), 5000, "the server to start");
});

after(() => {
  server.kill();
});

test("welcome message carries the player id and the map size", async () => {
  const client = await connect();
  await waitFor(() => client.welcome, 2000, "the welcome message");
  assert.ok(client.welcome.id > 0);
  assert.equal(client.welcome.w, 4000);
  assert.equal(client.welcome.h, 4000);
  client.ws.close();
});

test("world state includes the player's own snake and the food", async () => {
  const client = await connect();
  await waitFor(() => mySnake(client), 2000, "our snake to appear in the state");
  const me = mySnake(client);
  assert.ok(me.len >= 10);
  assert.ok(me.p.length >= 1);
  assert.equal(client.state.food.length, 2000);
  client.ws.close();
});

test("the snake steers toward the heading the client sends", async () => {
  const client = await connect();
  await waitFor(() => mySnake(client), 2000, "our snake to appear in the state");

  //Pick the direction with plenty of room so the snake can't hit a wall during the test.
  const goEast = mySnake(client).p[0][0] < 2000;
  const angle = goEast ? 0 : Math.PI;
  client.ws.send(`${angle.toFixed(3)},0`);

  await sleep(2000); //a half-turn takes at most ~0.8 s at the server's turn rate
  const a = mySnake(client).p[0];
  await sleep(1000);
  const b = mySnake(client).p[0];

  const dx = b[0] - a[0];
  const dy = b[1] - a[1];
  assert.ok((goEast ? dx : -dx) > 100, `expected about 200 units toward ${goEast ? "+x" : "-x"}, got dx=${dx}`);
  assert.ok(Math.abs(dy) < 40, `expected a nearly straight path, got dy=${dy}`);
  client.ws.close();
});

test("malformed input is ignored, counted, and does not drop the connection", async () => {
  const client = await connect();
  await waitFor(() => client.welcome, 2000, "the welcome message");
  for (const bad of ["garbage", "nan,0", "1.5,2", "999,0"]) client.ws.send(bad);

  //The server prints its rejected-input counter once per second.
  await waitFor(() => {
    const matches = [...serverLog.matchAll(/rejected (\d+)/g)];
    return matches.length && Number(matches[matches.length - 1][1]) >= 4;
  }, 3000, "the rejected counter to reach 4");
  assert.equal(client.closed, false);
  client.ws.close();
});

test("a message over the size limit closes the connection", async () => {
  const client = await connect();
  await waitFor(() => client.welcome, 2000, "the welcome message");
  client.ws.send("x".repeat(200));
  await waitFor(() => client.closed, 3000, "the server to close the connection");
});

test("connections from an allowed origin, or with no origin, are accepted", async () => {
  const browserLike = await connect({ headers: { Origin: ALLOWED_ORIGIN } });
  await waitFor(() => browserLike.welcome, 2000, "welcome for the allowed origin");
  const botLike = await connect(); // no Origin header, like a bot or script
  await waitFor(() => botLike.welcome, 2000, "welcome for no origin");
  browserLike.ws.close();
  botLike.ws.close();
});

test("connections from an unknown origin are rejected with HTTP 403", async () => {
  const status = await new Promise((resolve, reject) => {
    const ws = new WebSocket(URL, { headers: { Origin: "http://evil.example" } });
    ws.on("unexpected-response", (req, res) => {
      resolve(res.statusCode);
      ws.terminate();
    });
    ws.on("open", () => reject(new Error("the connection should have been rejected")));
    ws.on("error", () => {}); //the rejection is reported through unexpected-response
  });
  assert.equal(status, 403);
  await waitFor(() => /WARN\] rejected connection from origin 'http:\/\/evil\.example'/.test(serverLog),
    2000, "the server to log the rejection");
});

test("two players see each other, and a disconnect removes the snake", async () => {
  const a = await connect();
  const b = await connect();
  await waitFor(() => a.welcome && b.welcome, 2000, "both welcome messages");
  await waitFor(() => a.state && a.state.snakes.some((s) => s.id === b.welcome.id), 3000, "A to see B's snake");
  b.ws.close();
  await waitFor(() => a.state && !a.state.snakes.some((s) => s.id === b.welcome.id), 3000, "B's snake to disappear");
  a.ws.close();
});