# Snake Server

A realtime multiplayer, Slither.io-style browser game. An authoritative C++ server (uWebSockets) runs the simulation; a Canvas2D browser client renders it. Personal learning project, work in progress.

## Features (current)

- Server-authoritative simulation at 20 Hz: turn-rate-limited movement, growth from food, boost that costs length
- Multiple players in one room; world state broadcast to all clients as JSON
- Input validation at the network boundary (malformed input is dropped and counted)
- Mouse steering, camera follow, and a HUD with connection status and bandwidth

**Not built yet:** snake-vs-snake collisions and death, client interpolation, binary protocol, interest management, bot load testing.

## Requirements

Linux or WSL2. g++ 11+ with C++20, make, git, zlib headers, python3, Node 18+ for integration tests.

## Run it

First terminal, clone the submodule needed:

```bash
git clone https://github.com/DannyZhang0128/snake-server
cd snake-server
git submodule update --init server/third_party/uWebSockets
git -C server/third_party/uWebSockets submodule update --init uSockets
cd server
make
./server
```


In a second terminal, serve the client:

```bash
cd snake-server/client
python3 -m http.server 8000
```

Open http://localhost:8000. Steer with the mouse; hold the mouse button or Space to boost.

## Configuration

| Variable | Default | Meaning |
|---|---|---|
| `PORT` | `9001` | Port the game server listens on. Example: `PORT=9002 ./server` |
| `ALLOWED_ORIGINS` | `http://localhost:8000,http://127.0.0.1:8000` | Websites allowed to connect. Connections with no Origin header (bots, scripts) are always accepted. |

The client connects to port 9001 unless you open it as `http://localhost:8000/?port=9002`. See `.env.example`.

## Project layout

```
client/index.html   Canvas2D client (rendering, input)
server/world.h      Game rules: snakes, food, movement (no networking)
server/main.cpp     WebSocket server, input validation, broadcast loop
server/config.h     Constants and environment settings
server/log.h        Leveled logger
server/sim_main.cpp Headless simulation for timing experiments
server/Makefile     build, format, lint
server/input.h      Input message parsing and validation
server/origin.h     Origin allow-list check for WebSocket connections
server/tests/       Unit tests (doctest) and integration tests (Node)
```

## Protocol (WebSocket, text frames)

| Direction | Message | Meaning |
|---|---|---|
| client to server | `angle,boost` (e.g. `1.571,1`) | Desired heading in radians and boost flag |
| server to client | `{"t":"welcome","id":1,"w":4000,"h":4000}` | Sent once on connect: your snake id and map size |
| server to client | `{"t":"state","snakes":[...],"food":[...]}` | World state, sent every tick |

Invalid input is ignored and counted. Messages over 64 bytes close the connection.

## Request Flow

```
browser --"angle,boost"--> uWebSockets handler --> parseInput --> World::setInput
browser <--JSON state----- publish <-- buildState <-- World::tick <-- timer (every 50 ms)
```

## Development

```bash
make format        # needs: sudo apt install -y clang-format clang-tidy
make lint
make test          # integration tests, Node 18+
make test-unit     
make clean
```

## Roadmap

Collisions and death, client interpolation, binary protocol, spatial grid with interest management, bot load tests, then a TypeScript and PixiJS client.
