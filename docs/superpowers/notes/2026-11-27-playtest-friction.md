# Play-test friction, 2026-11-27

Owner play session for plan task C6. Each item: screen, what happened, what was expected, outcome.

| # | Screen | What happened | Expected | Outcome |
|---|--------|---------------|----------|---------|
| 1 | Login form | Typing into the email and password fields flickered, and the text did not show right away. | Text shows as it is typed, no flicker. | Fixed, owner confirmed: patch 0013. The game window lacked `WS_CLIPCHILDREN`, so every GL frame painted over the inline EDIT box (patch 0012). |
