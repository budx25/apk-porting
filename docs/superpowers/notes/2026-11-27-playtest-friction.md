# Play-test friction, 2026-11-27

Owner play session for plan task C6. Each item: screen, what happened, what was expected, outcome.

| # | Screen | What happened | Expected | Outcome |
|---|--------|---------------|----------|---------|
| 1 | Login form | Typing into the email and password fields flickered, and the text did not show right away. | Text shows as it is typed, no flicker. | Fixed, owner confirmed: patch 0013. The game window lacked `WS_CLIPCHILDREN`, so every GL frame painted over the inline EDIT box (patch 0012). |
| 2 | Paged screens and lists, on a trackpad | A two-finger swipe did not turn pages; the owner still had to click-drag. Sideways trackpad swipes send `WM_MOUSEHWHEEL`, which was ignored, and a trackpad's stream of small wheel deltas became many separate short drags. | A two-finger swipe turns the page and scrolls lists, like a mouse wheel. | `WheelDrag`: every wheel or trackpad scroll (both axes) is one continuous synthetic touch that lifts after 6 quiet frames. Fixed, owner confirmed. |
| 3 | Everywhere | The ← / → page keys are not needed. | No arrow-key swipes. | Removed (owner's call). |

The owner reported no further friction and closed the list. Item 1 was checked with the owner's real login; items 2–3 on a trackpad. Esc, Enter on dialogs, resize, F11 and a launcher restart (no login, no download) were not reported as issues. Neither `game.log` nor `launcher.log` contains the owner's email.
