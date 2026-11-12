# Manual test plan: main play flow

Setup: server running (`cd server && ./server`), page server running
(`cd client && python3 -m http.server 8000`), browser at http://localhost:8000.

| # | Step | Expected result | Result (pass/fail, notes) |
|---|---|---|---|
| 1 | Open the page | HUD shows "connected", "length: 10", and about "20 msg/s" | |
| 2 | Move the mouse around | Snake turns smoothly toward the cursor, no instant snaps | |
| 3 | Hold the mouse button or Space | Snake moves about twice as fast; length drops about 2 per second | |
| 4 | Keep boosting | Length stops dropping at 10 and the snake returns to normal speed | |
| 5 | Drive over yellow food | Food disappears; length goes up by 1 per item | |
| 6 | Steer into the red border | Snake stops at the border and cannot leave the map | |
| 7 | Open a second tab | A second snake (different color) appears, and the first tab sees it | |
| 8 | Close the second tab | Its snake disappears within about a second; server logs "player 2 left" | |
| 9 | In the console, type `ws.send("garbage")` | Nothing changes; the server's "rejected" count rises in the next stats line | |
| 10 | Stop the server with Ctrl+C | HUD changes to "DISCONNECTED" | |
| 11 | Restart the server; in `client/` run `python3 -m http.server 8001`; open http://localhost:8001 | HUD shows "DISCONNECTED" and the server logs "rejected connection from origin" | |
| 12 | Open http://localhost:8000/?port=9999 | HUD shows "DISCONNECTED" (the port override works; nothing listens on 9999) | |

## Failures found and follow-ups

(none yet)