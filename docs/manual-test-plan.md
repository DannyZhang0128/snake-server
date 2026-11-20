# Manual test plan: main play flow

Tester: ________   Date: ________   Commit: ________

Setup: server running (`cd server && ./server`), page server running
(`cd client && python3 -m http.server 8000`), browser at http://localhost:8000.
Steps 7 to 9 and 13 to 18 need two browser tabs (or windows) open at the same time.

| # | Step | Expected result | Result (pass/fail, notes) |
|---|---|---|---|
| 1 | Open the page | HUD shows "connected", "length: 10", and about "20 msg/s" | |
| 2 | Move the mouse around | Snake turns smoothly toward the cursor, no instant snaps | |
| 3 | Hold the mouse button or Space | Snake moves about twice as fast; length drops about 2 per second | |
| 4 | Keep boosting | Length stops dropping at 10 and the snake returns to normal speed | |
| 5 | Drive over yellow food | Food disappears; length goes up by 1 per item | |
| 6 | Steer into the red border | The snake dies: the screen dims and shows "You died" | |
| 7 | Open a second tab | A second snake (different color) appears, and the first tab sees it | |
| 8 | Close the second tab | Its snake disappears within about a second; server logs "player 2 left" | |
| 9 | In the console, type `ws.send("garbage")` | Nothing changes; the server's "rejected" count rises in the next stats line | |
| 10 | Stop the server with Ctrl+C | HUD changes to "DISCONNECTED" | |
| 11 | Restart the server; serve the client on port 8001; open http://localhost:8001 | HUD shows "DISCONNECTED" and the server logs "rejected connection from origin" | |
| 12 | Open http://localhost:8000/?port=9999 | HUD shows "DISCONNECTED" (the port override works; nothing listens on 9999) | |
| 13 | With two tabs open, steer one snake's head into the middle of the other snake's body | The snake that hit the body dies ("You died"); the other snake keeps playing | |
| 14 | Steer two snakes head-on into each other | Both snakes die | |
| 15 | After dying, click immediately | Nothing happens during the first moment (about 0.8 seconds) | |
| 16 | Wait a second, then click (or press Space or Enter) | A new snake appears at length 10 in a different spot, and the overlay disappears | |
| 17 | Look at where a snake died, then drive over that spot | Food dots mark the old body, and each one eaten adds 1 length | |
| 18 | Steer your snake in a tight circle so its head crosses its own body | Nothing happens: a snake never dies from hitting itself | |

## Failures found and follow-ups

(none yet)
