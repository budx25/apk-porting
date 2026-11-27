# Ninja Heroes New Era on Windows — native port design

Date: 2026-11-27 · Status: draft for approval

## Intent

The owner plays one Android game, **Ninja Heroes New Era** (`apk/NinjaHeroesNewEraRelease.apk`), and wants it on their Windows PC without the emulator ritual: no MuMu boot wait, no Android home screen. Success is one click, and the game opens in its own window, already up to date.

A throwaway spike (`spike/`, 2026-11-27) proved that the game runs **natively** on Windows. It got through the version check, the 2.6.1 hot update, email login, the server list, the game-server WebSocket login and in-game, at roughly 52–60 fps using 179 MB. This design turns that spike into a maintainable build. The spike is reference material only: it is not to be grown into the product.

## Goals

- One click starts the game. If the game server announces a new version, the patch downloads and applies first, then the game opens.
- The game behaves like the Android client for everything the owner uses: email/password login, server choice, gameplay, sound.
- Links the game opens (payment pages, news, costume shop) open in the PC's **default external browser**. This was part of the owner's original request.
- **It feels like a desktop game, not a phone in a window.** Every phone-only motion that slows the owner down (click-hold-drag to page or scroll, a separate pop-up box to type in, a fixed small window) gets a desktop equivalent. See "Desktop input".
- Rebuilding from a clean checkout is one documented script per part.

## Non-goals

- Running other APKs, or an "upload APK / list APKs" library. That only made sense for the emulator route.
- Google and Facebook sign-in. They depend on Android SDK tokens, and the owner logs in with email.
- In-app purchases via Google Play or AnySDK, ads, analytics (TalkingData, AdMob, Pokkt).
- Sharing with other players, an installer, code signing, auto-updating the launcher itself.
- A 64-bit game build. The only matching JS runtime is 32-bit.

## Anti-goals (a failure even if it works)

- **Logging credentials.** Email addresses, passwords, session tokens and saved-data values must never reach log files or console output. The spike leaked the owner's email into its log twice before redaction was added.
- **Committing the game's assets or the APK's code** into the repo. Assets come from the owner's own APK and the official patch server at run time.
- **Hammering or probing the game's servers.** The client makes only the requests the Android client makes, at the same points.
- **Reintroducing the spike's blind shortcuts** (pinned values and URL rewrites) without first trying to derive the real values.

## Constraints

- **Engine:** cocos2d-x **2.x JSB**, built from upstream branch `v2` (commit `dab2794`, 2016-07-01) with dependency bundle **`v2-deps-9`** from `cocos2d/cocos2d-x-3rd-party-libs-bin`. Only that bundle's `spidermonkey-win32/lib/mozjs.dll` reads the game's bytecode format (**141**, SpiderMonkey of the Firefox 22 era). The stock 2.2.6 runtime is format 137 and refuses it.
- **JS bindings submodule** `scripting/javascript/bindings/generated` is pinned to `cfbd4d1e…`. That commit is gone from `folecr/cocos2dx-autogen-bindings` and still exists in the fork `kusharami/cocos2dx-autogen-bindings`. Git verifies it by hash.
- **Toolchain:** VS 2022 Build Tools (v143, Windows SDK 10.0.22621), Win32 Release. Rust stable for the launcher (any target).
- **The game is online-only.** Endpoints, all plain HTTP/WS except the CDN:
  - version: `http://games<N>.kageherostudio.com:9001/game/version?v=<cur>&ch=108` → `"<new>#<bytes>"`
  - patch: `https://patch.kageherostudio.com/xxrz/<first char of new>/<cur>_<new>.zip` (verified for 1.1.1→2.6.1: 281,207,445 bytes, size equals `<bytes>`)
  - server list: `http://central.kageherostudio.com:80/game/servers`
  - login: `http://central.kageherostudio.com:80/game/lyto/login`
  - game socket: `ws://games<sid>.kageherostudio.com:60<sid>/nadsocket`
- **Channel 108** = `ANDROID_LYTO_HLR_PAY_COLORS` (a JS **string** `"108"`), from the APK's `assets/VERSION`: `{"v","c":cdn,"s":server domain,"ch","d":debug}`.

## Approaches considered

| | Approach | Trade-off | Verdict |
|---|---|---|---|
| A | Pure C++: engine fork with built-in updater, one exe | Fewest parts; updater/UI in 2014-era C++ | Not chosen |
| **B** | **C++ game + Rust launcher** (updater, settings, launch) sharing only a data folder | Two programs; each part in the language that suits it, and C++ shrinks to "run the game" | **Chosen by the owner** |
| C | Rust host embedding the engine via FFI | Most "Rust", but heavy FFI to an old engine for no gain over B | Dropped |

## Design

### Repo layout (builder may adjust names)

```
launcher/          Rust crate: update + launch
port/
  engine.lock      pinned engine commit, deps tag, bindings commit + fork URL
  patches/         patch series applied to the pinned engine (the fixes below)
  app/             the game app: AppDelegate, main.cpp, shim.js, native helpers
  build.ps1        fetch pinned engine + deps -> apply patches -> build Win32 Release
docs/
spike/             throwaway reference; add to .gitignore, never built on
```

The engine (~124 MB) and the deps (~145 MB) are fetched by `build.ps1` into an ignored folder, not committed.

### Runtime data folder

`%LOCALAPPDATA%\NinjaHeroesPC\` (designer's call; the builder may pick another per-user location):

```
game\              resources root the game exe runs from (UI/, armatures/, sound/, costume/, VERSION)
bin\               game exe + DLLs (copied from the port build)
logs\              launcher.log, game.log (redacted, see Logging)
launcher.toml      settings (window size; data path override)
```

### Launcher (Rust)

On each start:

1. **First run:** ask for the APK path (file dialog, or a CLI argument). Extract `assets/**` into `game\`, rejecting unsafe paths (absolute, `..`, drive letters).
2. **Update check:** read `game\VERSION`, then call the version endpoint with `v`/`ch` from it. If the reply's version differs from `v`, download the patch URL to a temp file. Verify the byte count equals `<bytes>`. Extract it over `game\` with the same path rules, then rewrite `VERSION`'s `v`. Repeat until the server reports no newer version.
3. **Launch** `bin\NinjaHeroes.exe` with the working directory `game\`, passing the window size from settings. Exit, or stay minimized: builder's call.

UI: a small window showing status and progress **only while it is extracting or downloading**. If the game is current, it launches straight away. The owner's goal is no waiting and no extra screens.

Error handling: when the network fails, offer "retry" and "play anyway". A partly applied patch must not leave a broken `game\`: extract to a staging folder, then swap.

### Game (C++, cocos2d-x v2 fork)

**Engine patches** (each one found in the spike; keep each as its own patch file):

1. `chipmunk.c`: spaces between string literals and macros (C++11 literal suffixes).
2. `cpVect.c`: drop `inline` from `cpvslerp` so it gets exported.
3. `CCStdC.h`: define `snprintf` → `_snprintf` only for `_MSC_VER < 1900`.
4. Build flags: `HAVE_STRUCT_TIMESPEC`; link `legacy_stdio_definitions.lib ws2_32.lib`.
5. **`__iob_func`:** the prebuilt libjpeg/libpng/libwebsockets were built against the VS2010 CRT. The spike's shim returns *copies* of `stdin`/`stdout`/`stderr`, and writing to them crashed libwebsockets. Preferred fix: rebuild those three libraries from source with v143, which removes the shim. Fallback: keep the shim, and make every user of it log through the engine instead (patch 7).
6. `HttpClient.cpp`: remove `pthread_exit` (redundant with the `return`; unsafe with `pthreadVCE2`).
7. `WebSocket.cpp`: route libwebsockets logging through `CCLog` (`lws_set_log_level`), and **reassemble fragmented messages** before delivery (`lws_is_final_fragment` / `lws_remaining_packet_payload`). Without this, any server message over 4 KB arrives broken and loading never finishes.
8. `js_log` uses `CCLog`, so script errors appear in Release builds.
9. Crash logging in `main.cpp`: hook `abort` (both UCRT and `msvcrt.dll`), plus unhandled and first-chance exceptions on worker threads, and write a symbolized stack. Keep it; it found two bugs in minutes.

**App startup** (`AppDelegate`): search path `UI`, resolution order `["resources-large", ""]`, register the same binding callbacks as the stock JS template, then run `shim.js` and then `main.js`. The bytecode `UI/main.jsc` is picked up automatically.

**Android shim** (`shim.js`, plain JS loaded before the game). It replaces what the Android build's native code provides:

| Game expects | Provide |
|---|---|
| `ts.CommonJsb` (constructed) | `getClientVersion()` → `v` from `VERSION`; `getTestVersion()` → `false`; `checkNetwork()` → `true`; `getAccountId()` → `""`; `getClientDeviceInfo()` → a plausible device-info value (format to be found, see open items); everything else is a logged no-op |
| `ts.JSBTools` (constructed) | analytics hooks (`onGuideStep`, `setAccount`, `onChargeRequest`, …): no-ops; `goToVersion()` → run the launcher's update flow (or tell the user to restart) |
| `ts.ChannelBinding` | `openURL(url)` → **default external browser** (needs one small native binding, e.g. `pc.openURL` via `ShellExecuteW`); Google/Facebook login → tell the user "not available on PC" |
| `px.StorageUtil` | `getItem` → `sys.localStorage.getItem(k) \|\| null`; `setItem` / `removeItem` → `sys.localStorage`; `encryt` / `decrpt` → identity. This is local only, so the server never sees it, and identity is fine unless the owner wants to import phone saves |
| `anysdk` | inert no-op object |
| global `channelBinding` | `new ts.ChannelBinding()` |
| `serverUtil.getChannel()`, `getSDKType()` | channel `"108"` from `VERSION` |
| `serverUtil.getVersion()` | `v` from `VERSION` (the default `"1.0.0"` makes the game server demand an update) |

Any unknown call on a shim object must **log its name and argument count only**, then return a harmless value, so new game versions show up as log lines rather than crashes.

**Derive values; don't pin them.** On Android, native code seeds the channel, SDK type and version into the script before the login scene builds its URLs. Trace how, by disassembling callers of `VersionUtil::getChannel` / `getVersion` in `lib/arm64-v8a/libcocos2djs.so` (capstone works; `spike/tools/disasm.py` and `xrefs.py` show the approach), and replicate it in `shim.js` at the same moment. When the channel is in place early, the script itself picks `central.kageherostudio.com:80` for the server list, and the spike's URL rewrite can go. Only if tracing fails, keep the spike's post-load overrides, marked as shortcuts.

**Saved-data keys** (found in the spike): the name comes before the key. `servers=A2`, `channel=A3`, `account=A4`, `sdk=A5`, `clientnfo=A6`. Don't seed any of them. Seeding `A4` created a fake saved account, the game auto-logged in with it, and the login form never appeared.

### Desktop input

Owner's requirement: every phone-only motion that hurts the experience gets a desktop equivalent where possible. Do it **at the engine level** (the Win32 window code, `CCEGLView`, and the input dispatchers) so it works on every screen without per-screen code. Fall back to per-screen handling in `shim.js` only where a generic mapping can't work.

| Phone gesture today | Desktop replacement | Notes |
|---|---|---|
| Swipe pages left/right | **Mouse wheel** or **← / →** | Synthesize the same touch swipe (began → moves → ended) at the cursor, or at screen centre for the keys, so every paged view reacts as if swiped. |
| Drag to scroll a list | **Mouse wheel** scrolls the view under the cursor; **Shift+wheel** scrolls sideways | Same synthesized drag; distance per wheel notch tuned so one notch is about one row. |
| Android back button | **Esc** | Route to the engine's keypad "back" event, which the game already handles for Android. |
| Tap the main button of a dialog | **Enter** | When a dialog with a single main button (the game's `PromptView`-style boxes) is on top, trigger that button. Best effort: dialogs with several choices are left to the mouse. |
| Tap a text field → separate Win32 pop-up box | **Type directly into the field** | Replace the old engine's modal input box (`CCEditBoxImplWin` / `CWin32InputBox`) with inline editing: caret, typing, Backspace, Tab between fields, Enter submits, paste with Ctrl+V. Password fields stay masked. |
| Pinch to zoom (only if a screen uses it) | **Ctrl+wheel** | Synthesized two-finger pinch around the cursor. |
| Fixed 1136×640 window | **Resizable window, F11 fullscreen** (borderless) | The game keeps its design resolution and scales to fit with letterboxing; the launcher's window-size setting picks the starting size. |

Leave the game's own animations, loading steps and pacing alone: they are part of how it plays, not phone-specific gestures.

**Play-testing pass:** after the generic mappings work, the owner plays normally and lists any remaining friction (screens where the wheel or keys don't behave, extra clicks, awkward spots). Each item is either fixed with a per-screen tweak or explicitly accepted. This list is part of done.

### Logging

- Both programs log to `logs\`. Request logs show **method + path only, never query strings**. Shim logs show the method name and argument count; saved-data calls also show the key, with any `@` address masked.
- Debug-only diagnostics (scene dumps, WebSocket message shapes) stay behind a flag that is off by default.

## Verification

- **Launcher unit tests:** version-reply parsing; patch URL building (`1.1.1`/`2.6.1` → `/xxrz/2/1.1.1_2.6.1.zip`); safe extraction rejects `..`, absolute paths and drive letters; a size mismatch aborts without touching `game\`.
- **Engine patches:** the build succeeds from a clean checkout via `port/build.ps1`.
- **Desktop input (manual):** the wheel and ← / → change pages on a paged screen; the wheel scrolls a long list (Shift+wheel for a sideways one); Esc closes a dialog and goes back; Enter confirms a one-button dialog; typing goes straight into the email and password fields with no pop-up; resizing the window and F11 keep the game correctly scaled. Then the play-testing friction list is fully resolved or accepted.
- **Acceptance (manual, owner logs in themselves):** fresh data folder → pick APK → update to current → login form appears → log in → choose server → in game. After that, a restart goes straight to server selection with no download. The logs contain no email address (`Select-String` check, as in the spike).

## Risks

- **Account ban:** this is an unofficial client on the owner's real account. Mitigation: behave exactly like the Android client, and use a spare account first if the owner wants.
- **Game updates** can add new native calls, change endpoints, or (worst case) move to a different runtime format, which would break the port at its root. The shim's "log unknown calls" behavior and the launcher's update flow are the early warning.
- **Old binaries:** `mozjs.dll`, `pthreadVCE2.dll` and libcurl are 2013-era builds. More crashes of the kind the spike hit may appear. Crash logging stays in so they can be found quickly.

## Left to the builder

Launcher UI toolkit and crate structure; whether the launcher stays running; how the synthesized wheel/arrow swipes are tuned (distance, speed, step per notch); how inline text editing is drawn (engine-drawn field or an overlaid native edit control); how `pc.openURL` is bound; patch-chain edge cases; the data-folder location; window-size presets; whether to rebuild the old C libraries (preferred) or keep the shim; module and file names inside `port/app/`.

## Open items to resolve during the build

- The version endpoint's reply when the client is already current. Expected to be the same version, but this needs confirming before relying on the loop's exit condition.
- What `getClientDeviceInfo()` should return. The Android `CommonJsb` returns device info that the game saves under `A6`. Find the format from the ARM code or the script.
- Which server host `<N>` the launcher should use for the version check. The spike used `games1` and later the last-played server.
