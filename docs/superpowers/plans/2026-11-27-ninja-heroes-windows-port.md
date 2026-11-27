# Ninja Heroes Windows Port Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** One click on a Rust launcher updates Ninja Heroes New Era from the official patch server and starts a native Windows build of the game that plays like a desktop game.

**Architecture:** Two programs share one data folder. `launcher/` (Rust) installs the game's assets from the owner's APK, applies hot-update patches and starts the game. `port/` holds pinned-upstream cocos2d-x v2, a numbered patch series, and the game app (C++ host plus `shim.js`, which stands in for the Android-only native classes). Part C adds desktop input as further engine patches.

**Tech Stack:** Rust stable (`ureq`, `zip`, `serde`/`serde_json`, `toml`, `eframe`, `rfd`, `windows-sys`; dev: `tempfile`, `tiny_http`). C++ cocos2d-x v2 JSB, VS 2022 Build Tools v143, Win32, PowerShell build script.

**Spec:** `docs/superpowers/specs/2026-11-27-ninja-heroes-windows-port-design.md`. The spike in `spike/` (git-ignored) is the reference for every engine fix. `spike/tools/` holds the analysis scripts.

**Parts:** A (launcher) and B (game) each produce working software on their own. A can be tested against a hand-built `bin\`; B can be run against a data folder that A created or that was filled by hand. C needs B.

## Global Constraints

- Engine: `https://github.com/cocos2d/cocos2d-x.git` commit `dab2794b106d2ceb54597f4f0e9a2ae7e2ac8894` (branch `v2`).
- Bindings submodule `scripting/javascript/bindings/generated` = `cfbd4d1e703b695c4897d818c9a4231bca0fa24b`, fetched from `https://github.com/kusharami/cocos2dx-autogen-bindings.git`.
- Deps: `https://github.com/cocos2d/cocos2d-x-3rd-party-libs-bin/archive/v2-deps-9.zip`, SHA-256 `a5868031ffb6bbc6991e42ffba5b69773b1b4890bb89b8305c5063438ccbce9c`, extracted over the engine root with the top folder stripped.
- Game build: Win32 Release, `/p:PlatformToolset=v143 /p:WindowsTargetPlatformVersion=10.0`, env `CL=/DHAVE_STRUCT_TIMESPEC`, `LINK=legacy_stdio_definitions.lib ws2_32.lib`.
- Data root: `%LOCALAPPDATA%\NinjaHeroesPC\` containing `game\`, `bin\`, `logs\`, `staging\`, `launcher.toml`.
- Game exe: `bin\NinjaHeroes.exe --width <w> --height <h> [--fullscreen] --log <root>\logs\game.log`, working directory `<root>\game\`. Window title `Ninja Heroes`. While running, the game holds the named mutex `Local\NinjaHeroesPC.Game`.
- Version check: `http://games1.<s>:9001/game/version?v=<v>&ch=<ch>` → `"<new>#<bytes>"`. Patch: `https://<c>/xxrz/<first char of new>/<v>_<new>.zip`. `<s>`, `<c>`, `<v>`, `<ch>` come from `game\VERSION` (`{"v","c","s","ch","d"}`).
- Channel/SDK type is the JS string `"108"` (`ANDROID_LYTO_HLR_PAY_COLORS`). Never seed saved-data keys `A2`–`A6`.
- **Never log** email addresses, passwords, tokens, query strings or saved-data values. Requests: method + path. Shim: method name + argument count, plus the key for saved-data calls with `@` addresses masked.
- No game assets, APK contents, engine sources or deps in git: `port/.engine/`, `port/out/` and `spike/` are ignored.

## Review Focus

- **No network at launch** (Wi-Fi off, DNS failure, server down): the launcher gives up within ~5 s and offers Retry / Play anyway, never hangs. Test in Task A4.
- **Bad patch download** (truncated, wrong size, or a zip with `..`/absolute/drive-letter entries): rejected, and `game\` and `VERSION` are untouched. Tests in Tasks A2 and A3.
- **Wrong APK chosen** (another app, or a file that isn't a zip): a clear error, nothing written to `game\`. Test in Task A2.
- **Window minimized, restored, or resized very small:** no crash and correct scaling afterwards (Windows sends a 0×0 size on minimize). Check in Task C4.
- **Launcher started while the game is already running:** no update, no second game. It brings the running window to the front. Test in Task A5.

---

## Part A — Launcher (`launcher/`)

### Task A1: Crate, VERSION file, URLs, redaction

**Files:**
- Create: `launcher/Cargo.toml`, `launcher/src/main.rs` (stub `fn main(){}` for now), `launcher/src/version.rs`, `launcher/src/urls.rs`
- Modify: `.gitignore` (add `launcher/target/`, `port/.engine/`, `port/out/`)

**Interfaces:**
- Produces: `version::VersionFile { pub v: String, pub c: String, pub s: String, pub ch: String, pub d: String }` (serde); `version::read_version(path: &Path) -> anyhow::Result<VersionFile>`; `version::set_version(path: &Path, new_v: &str) -> anyhow::Result<()>` (rewrites `v`, keeps the other fields); `urls::version_url(v: &VersionFile) -> String`; `urls::patch_url(cdn: &str, cur: &str, new: &str) -> String`; `urls::parse_version_reply(body: &str) -> anyhow::Result<(String, u64)>`; `urls::redact(url: &str) -> &str`.

- [ ] **Step 1: Write failing unit tests** (in `#[cfg(test)]` modules)

```rust
let vf = VersionFile{ v:"1.1.1".into(), c:"patch.kageherostudio.com".into(), s:"kageherostudio.com".into(), ch:"108".into(), d:"false".into() };
assert_eq!(version_url(&vf), "http://games1.kageherostudio.com:9001/game/version?v=1.1.1&ch=108");
assert_eq!(patch_url("patch.kageherostudio.com","1.1.1","2.6.1"), "https://patch.kageherostudio.com/xxrz/2/1.1.1_2.6.1.zip");
assert_eq!(parse_version_reply("2.6.1#281207445\n").unwrap(), ("2.6.1".to_string(), 281207445));
assert!(parse_version_reply("<html>").is_err());
assert!(parse_version_reply("2.6.1#").is_err());
assert_eq!(redact("http://a/game/login?acc=me@x.com&pwd=1"), "http://a/game/login");
// version.rs: read_version on {"v":"2.6.1",...} -> v == "2.6.1"; set_version(path,"2.7.0") then read_version -> v=="2.7.0", ch=="108"
```

- [ ] **Step 2: Run `cargo test` in `launcher/`.** Expected: compile errors / FAIL.
- [ ] **Step 3: Implement the functions above.** `parse_version_reply` trims whitespace and splits on `#`.
- [ ] **Step 4: `cargo test`.** Expected: PASS.
- [ ] **Step 5: Commit** `feat(launcher): version file, update URLs, redaction`

### Task A2: Safe extraction, APK install, patch apply

**Files:**
- Create: `launcher/src/paths.rs`, `launcher/src/extract.rs`, `launcher/src/install.rs`
- Test: inline tests that build zips with the `zip` crate in `tempfile` dirs

**Interfaces:**
- Consumes: `version::read_version`, `version::set_version`.
- Produces: `paths::Paths { pub root, pub game, pub bin, pub logs, pub staging, pub settings: PathBuf }`; `Paths::new(root: PathBuf) -> Paths`; `Paths::default_root() -> PathBuf` (`%LOCALAPPDATA%\NinjaHeroesPC`); `extract::extract_zip(zip: &Path, dest: &Path, prefix: &str) -> anyhow::Result<usize>` (only entries under `prefix`, with `prefix` stripped; validates **every** name before writing anything); `extract::move_over(src: &Path, dest: &Path) -> anyhow::Result<()>` (moves files, overwriting, then removes `src`); `install::install_from_apk(p: &Paths, apk: &Path) -> anyhow::Result<()>`; `install::apply_patch(p: &Paths, zip: &Path, new_v: &str) -> anyhow::Result<()>`.

- [ ] **Step 1: Write failing tests**
  - `extract_rejects_unsafe`: zips containing `../x`, `/abs`, `C:/x` or `a\..\..\x` each return `Err`, and `dest` stays empty.
  - `extract_strips_prefix`: an APK-like zip with `assets/VERSION`, `assets/UI/main.jsc` and `classes.dex`, using prefix `assets/` → `dest/VERSION` and `dest/UI/main.jsc` exist and `classes.dex` doesn't; returns `2`.
  - `install_rejects_wrong_apk`: a zip without `assets/VERSION` → `Err` containing "not the Ninja Heroes APK", and `game\` is not created. A non-zip file → `Err`.
  - `apply_patch_overlays_and_sets_version_last`: `game\` holds `VERSION` (v 1.1.1) and `UI/a.png`; the patch zip holds `UI/a.png` (new bytes), `UI/b.png` and `.gitignore` → `a.png` replaced, `b.png` added, `.gitignore` absent, `VERSION.v == "2.6.1"`, `staging\` removed.
  - `apply_patch_unsafe_leaves_game_untouched`: a patch with `../evil` → `Err`; `game\` files and `VERSION.v` unchanged.
- [ ] **Step 2: `cargo test`.** Expected: FAIL.
- [ ] **Step 3: Implement.** `apply_patch`: extract with prefix `""` into `staging\` (skipping `.gitignore`), then `move_over(staging, game)`, then `set_version` **last**. An interrupted move leaves the old `v`, so the next run re-applies the same patch. `install_from_apk`: extract with prefix `assets/` into `staging\`, require `staging\VERSION`, then `move_over` into `game\`.
- [ ] **Step 4: `cargo test`.** Expected: PASS.
- [ ] **Step 5: Commit** `feat(launcher): safe zip extraction, APK install, patch apply`

### Task A3: HTTP with timeouts and verified downloads

**Files:**
- Create: `launcher/src/http.rs`

**Interfaces:**
- Produces: `trait Http { fn get_text(&self, url: &str) -> anyhow::Result<String>; fn download(&self, url: &str, dest: &Path, expected_len: u64, progress: &mut dyn FnMut(u64)) -> anyhow::Result<()>; }`; `struct UreqHttp` with connect timeout 5 s, read timeout 10 s for `get_text` and 30 s for `download`. `download` writes `dest.part`, renames to `dest` only if the byte count equals `expected_len`, and otherwise deletes the part file and returns `Err`.

- [ ] **Step 1: Write failing tests** using `tiny_http` on `127.0.0.1:0`:
  - `download_ok`: serves 1000 bytes, `expected_len` 1000 → `dest` holds 1000 bytes, no `.part` left.
  - `download_size_mismatch`: serves 999 bytes, `expected_len` 1000 → `Err`, neither `dest` nor `dest.part` exists.
  - `get_text_unreachable_fails_fast`: `get_text("http://127.0.0.1:9/")` → `Err` within 6 s.
- [ ] **Step 2: `cargo test`.** Expected: FAIL.
- [ ] **Step 3: Implement `UreqHttp`.** Log URLs only through `urls::redact`.
- [ ] **Step 4: `cargo test`.** Expected: PASS.
- [ ] **Step 5: Commit** `feat(launcher): HTTP client with timeouts and verified downloads`

### Task A4: Update flow

**Files:**
- Create: `launcher/src/update.rs`

**Interfaces:**
- Consumes: `Http`, `Paths`, `urls::*`, `install::apply_patch`, `version::read_version`.
- Produces: `enum Progress { Checking, Downloading { done: u64, total: u64 }, Applying }`; `enum Outcome { UpToDate(String), Updated { from: String, to: String } }`; `update::run(p: &Paths, http: &dyn Http, on: &mut dyn FnMut(Progress)) -> anyhow::Result<Outcome>`. Loops at most 10 times. It stops when the reply's version equals the local `v`. Each round downloads to `<root>\staging.zip`, applies, and deletes the zip.

- [ ] **Step 1: Write failing tests** with a `FakeHttp` (scripted replies; `download` writes a prepared zip):
  - `up_to_date`: reply `"2.6.1#1"` with local `2.6.1` → `Outcome::UpToDate("2.6.1")`, no download.
  - `one_patch`: local `1.1.1`, replies `["2.6.1#<len>", "2.6.1#<len>"]` → `Updated{from:"1.1.1",to:"2.6.1"}`, and the download URL requested was `https://patch.kageherostudio.com/xxrz/2/1.1.1_2.6.1.zip`.
  - `offline`: `get_text` errors → `run` returns `Err`, and `game\` is unchanged.
  - `garbage_reply`: reply `"<html>"` → `Err`.
  - `runaway_guard`: the server always answers a new version → `Err` after 10 rounds.
- [ ] **Step 2: `cargo test`.** Expected: FAIL.
- [ ] **Step 3: Implement `run`.**
- [ ] **Step 4: `cargo test`.** Expected: PASS.
- [ ] **Step 5: Commit** `feat(launcher): update loop`

### Task A5: Settings, launch, single instance, UI

**Files:**
- Create: `launcher/src/settings.rs`, `launcher/src/launch.rs`, `launcher/src/ui.rs`
- Modify: `launcher/src/main.rs`

**Interfaces:**
- Consumes: everything above.
- Produces: `settings::Settings { width: u32 /*1136*/, height: u32 /*640*/, fullscreen: bool /*false*/ }` with `load(path) -> Settings` (defaults when missing or invalid) and `save`; `launch::game_command(p: &Paths, s: &Settings) -> std::process::Command` (args and working directory per Global Constraints); `launch::game_running() -> bool` (`OpenMutexW` on `Local\NinjaHeroesPC.Game`); `launch::focus_game()` (`FindWindowW` by title `Ninja Heroes` → `SetForegroundWindow`).

Flow in `main.rs`: if `game_running()` → `focus_game()` and exit. If `game\VERSION` is missing → `rfd` file dialog for the APK (or `--apk <path>`) → `install_from_apk`, with progress in the window. Then `update::run`. If it's up to date, launch with **no window shown**. If an update is needed, show the `eframe` window with status and a progress bar until done, then launch. On `Err` from `run`, show the window with the error (URL redacted) and **Retry** / **Play anyway** buttons. Log to `logs\launcher.log`.

- [ ] **Step 1: Write failing tests:** `settings_defaults_when_missing` (1136/640/false); `game_command_args` (args `--width 1136 --height 640 --log <root>\logs\game.log`, no `--fullscreen`, `current_dir == game\`); `game_running_detects_mutex` (create `Local\NinjaHeroesPC.Game` in the test → `true`; after drop → `false`).
- [ ] **Step 2: `cargo test`.** Expected: FAIL.
- [ ] **Step 3: Implement** `settings.rs`, `launch.rs`, `ui.rs` and the flow in `main.rs`.
- [ ] **Step 4: `cargo test`.** Expected: PASS. Then `cargo build --release`.
- [ ] **Step 5: Manual check:** with a fresh `%LOCALAPPDATA%\NinjaHeroesPC`, run `launcher.exe --apk D:\personal\apk-porting\apk\NinjaHeroesNewEraRelease.apk`. Expected: the progress window shows extraction, then a 268 MB download, then `game\VERSION` has `"v":"2.6.1"`. It then fails to launch with a clear "game not installed" message until Part B installs `bin\`. Run it again: no window, no download.
- [ ] **Step 6: Commit** `feat(launcher): settings, single instance, UI, launch`

## Part B — Game (`port/`)

### Task B1: Pinned engine fetch, patch series, clean build

**Files:**
- Create: `port/engine.lock` (the pins from Global Constraints), `port/build.ps1`, `port/patches/0001-chipmunk-cxx11-literals.patch`, `0002-chipmunk-export-cpvslerp.patch`, `0003-ccstdc-snprintf-vs2015.patch`, `0004-iob-func-shim.patch`, `0005-httpclient-no-pthread-exit.patch`, `0006-websocket-log-and-reassembly.patch`, `0007-logging-release-and-redaction.patch`

**Interfaces:**
- Produces: `port/build.ps1 [-Rebuild] [-Install] [-Test]`. It fetches into `port/.engine/cocos2d-x` (skipped if it's already at the pinned commit), verifies the deps SHA-256, applies `patches/*.patch` in order with `git apply` (skipped when already applied), copies `port/app/` to `projects/NinjaHeroes/`, builds, and stages the exe, DLLs and `shim.js` into `port/out/bin/`. `-Install` copies `port/out/bin/*` to `%LOCALAPPDATA%\NinjaHeroesPC\bin\`. `-Test` builds and runs `port/tests/*.cpp` (Task C1).

- [ ] **Step 1: Export the patches from the spike.** `git -C spike/cocos2d-x diff -- <file>` per fix: 0001 `external/chipmunk/src/chipmunk.c`; 0002 `external/chipmunk/src/cpVect.c`; 0003 `cocos2dx/platform/win32/CCStdC.h`; 0004 `cocos2dx/platform/win32/CCCommon.cpp`; 0005 `extensions/network/HttpClient.cpp`; 0006 `extensions/network/WebSocket.cpp` + `WebSocket.h`; 0007 `scripting/javascript/bindings/ScriptingCore.cpp`, `XMLHTTPRequest.cpp`, `jsb_websocket.cpp`. Drop the spike's `.gitmodules` change: `build.ps1` points the submodule at the fork instead. Keep the `__iob_func` shim (0004) as the design's fallback. Rebuilding libjpeg/libpng/libwebsockets is deferred because 0006 already stops libwebsockets using `stderr`, and the image libraries only reach it on corrupt-file errors.
- [ ] **Step 2: Write `build.ps1`.** It reuses the spike's `spike/build.ps1` MSBuild invocation and env vars.
- [ ] **Step 3: Verify a clean build.** Delete `port/.engine`, then run `port/build.ps1` (Task B2's app files must exist, so do B1 and B2 together before this step). Expected: `exit 0`, and `port/out/bin/NinjaHeroes.exe`, `mozjs.dll` and `shim.js` exist.
- [ ] **Step 4: Commit** `build(port): pinned engine fetch and VS2022 patch series`

### Task B2: Game host app

**Files:**
- Create: `port/app/Classes/AppDelegate.cpp/.h`, `port/app/Classes/PcBindings.cpp/.h`, `port/app/proj.win32/main.cpp`, `port/app/proj.win32/NinjaHeroes.vcxproj` (+ `.filters`, `.sln`, `game.rc`, `res/`), derived from `spike/cocos2d-x/projects/NinjaSpike/proj.win32` with the project and target renamed to `NinjaHeroes` and the pre-build resource copy removed.

**Interfaces:**
- Consumes: the patched engine from B1.
- Produces, for scripts: `pc.openURL(url: string) -> bool` (only `http://` and `https://`; anything else is refused and logged by scheme only; opens via `ShellExecuteW`); `pc.readTextFile(relPath: string) -> string | null` (read via `CCFileUtils` from the working directory). `pc.debug: boolean` (true only with `--debug`). Hook for Task C3: `pc.onEnter` is called by the host when Enter is pressed. Command line: `--width`, `--height`, `--fullscreen`, `--log` per Global Constraints, plus `--debug` (off by default; the launcher never passes it).

- [ ] **Step 1: Write `main.cpp`.** Parse the args. `freopen` stdout/stderr to `--log` path (unbuffered). Create mutex `Local\NinjaHeroesPC.Game`. Keep the spike's crash logging (`abort` hooks for UCRT and `msvcrt.dll`, unhandled and first-chance worker-thread exceptions, symbolized stack). Window title `Ninja Heroes`, frame size from args.
- [ ] **Step 2: Write `AppDelegate`.** Search path `UI`, resolution order `{"resources-large", ""}`, the stock template's register callbacks plus `register_pc_bindings`. Run `<exe dir>\shim.js` (absolute path), then `main.js`. Display stats off.
- [ ] **Step 3: Write `PcBindings`** with the functions above.
- [ ] **Step 4: Verify.** `port/build.ps1 -Install`, then start via the launcher (data folder from Task A5). Expected: a window titled `Ninja Heroes`, the game reaches its loading spinner, `logs\game.log` exists, and it contains `Get data from file(shim.js)` failures until B3 adds the file.
- [ ] **Step 5: Commit** `feat(port): Windows host app with pc.* bindings and crash logging`

### Task B3: Android shim

**Files:**
- Create: `port/app/Resources/shim.js` (staged to `bin\` by `build.ps1`)

**Interfaces:**
- Consumes: `pc.readTextFile`, `pc.openURL`, `sys.localStorage`.
- Produces: the globals the game reads before `main.js`: `ts.CommonJsb`, `ts.JSBTools`, `ts.ChannelBinding`, `px.StorageUtil`, `anysdk`, `channelBinding`. Behavior is per the spec's shim table. `VERSION` is read once via `pc.readTextFile("VERSION")` and `JSON.parse`. `goToVersion()` shows the game's own prompt telling the owner to restart via the launcher. Google/Facebook login show "not available on PC". Unknown members are callable no-ops that log `"[shim] <path>(<n> args)"`. Saved-data calls log the key with `@` addresses masked as `<email>`.

- [ ] **Step 1: Write `shim.js`.** Start from `spike/cocos2d-x/projects/NinjaSpike/Resources/stubs.js`, minus the spike seeding and the hard-coded answers. Values come from `VERSION`. The spike's diagnostics (JSON.parse failure lengths, the scene dumper and WebSocket message shapes from `post.js`) are kept only inside `if (pc.debug) { … }`.
- [ ] **Step 2: Verify login (owner logs in themselves).** `build.ps1 -Install`, then launch. Expected: the login form appears (Email, Password, LOGIN, SIGN UP, RESET PASSWORD); after logging in, the server list and PLAY appear. `Select-String -Path logs\game.log -Pattern '[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}' -Quiet` returns `False`.
- [ ] **Step 3: Commit** `feat(port): Android shim`

### Task B4: Derive channel, SDK type and version the Android way

**Files:**
- Modify: `port/app/Resources/shim.js`

**Interfaces:**
- Consumes: `spike/tools/disasm.py`, `spike/tools/xrefs.py`, and `spike/apkx` (the extracted `lib/arm64-v8a/libcocos2djs.so`; re-extract from the APK if missing).
- Produces: a function `seedFromVersion(vf)` in `shim.js`, run at the moment the Android build seeds these values, so that `serverUtil.getChannel()` and `getSDKType()` return `"108"` and `getVersion()` returns `vf.v`.

- [ ] **Step 1: Trace.** List the callers of `VersionUtil::getChannel`, `getVersion` and `getServerAddr` (`xrefs.py` / `disasm.py` around their call sites). Identify which JS function or saved value the native code feeds, and when. Write the finding as a short comment at the top of `seedFromVersion`. In the same pass, find the return format of the native `CommonJsb::getClientDeviceInfo` (the game stores it under `A6`), and make `ts.CommonJsb.getClientDeviceInfo()` return a value of that format with Windows-appropriate contents.
- [ ] **Step 2: Implement `seedFromVersion`** to match. If the trace fails, fall back to the spike's post-load overrides (`spike/.../Resources/post.js`: `getChannel`, `getSDKType`, `getVersion`, and the `XMLHttpRequest.open` rewrite of `http://static.dreamjelly.com:8082/` → `http://central.kageherostudio.com:80/`), each marked `// shortcut:`, with the reason the trace failed.
- [ ] **Step 3: Verify.** Launch and log in. Expected in `game.log`: `XHR GET http://games1.kageherostudio.com:9001/game/checkClientVersion` (or the last server), then `XHR GET http://central.kageherostudio.com:80/game/servers` **without** the rewrite (or with it, if on the fallback path). PLAY reaches the game, with no "new version available".
- [ ] **Step 4: Commit** `feat(port): seed channel/version like the Android build`

## Part C — Desktop input

### Task C1: Input math, tested in isolation

**Files:**
- Create: `port/app/Classes/DesktopInputMath.h/.cpp` (no engine includes; `struct Pt { float x, y; }`; `enum class Axis { Vertical, Horizontal }`), `port/tests/input_math_tests.cpp` (assert-based, exits non-zero on failure)

**Interfaces:**
- Produces: `std::vector<Pt> wheelDragPath(Pt start, int wheelDelta, Axis axis, float pxPerNotch, int steps)` (start point plus `steps` points; cocos y points up). Wheel down (`delta < 0`) drags **up** for vertical lists and **left** for horizontal ones. `std::vector<Pt> arrowSwipePath(Pt center, bool right, float width, int steps)` (a drag of `0.4*width`; `right == true` drags left). `std::pair<std::vector<Pt>, std::vector<Pt>> pinchPaths(Pt center, int wheelDelta, float pxPerNotch, int steps)` (two fingers 100 px apart; `delta > 0` moves them apart).

- [ ] **Step 1: Write the failing tests**

```cpp
auto p = wheelDragPath({500,300}, -120, Axis::Vertical, 120.f, 4);
assert(p.size()==5 && p.front().x==500 && p.front().y==300 && p.back().x==500 && p.back().y==420);
auto h = wheelDragPath({500,300}, -240, Axis::Horizontal, 120.f, 4);
assert(h.back().x==260 && h.back().y==300);
auto a = arrowSwipePath({568,320}, true, 1136.f, 6);
assert(a.size()==7 && a.back().x < a.front().x && std::abs((a.front().x-a.back().x)-454.4f) < 0.01f);
auto [f1,f2] = pinchPaths({568,320}, 120, 60.f, 4);
assert(std::abs(f2.back().x - f1.back().x) > std::abs(f2.front().x - f1.front().x));
```

- [ ] **Step 2: `port/build.ps1 -Test`.** Expected: FAIL (does not compile).
- [ ] **Step 3: Implement** the three functions (linear interpolation).
- [ ] **Step 4: `port/build.ps1 -Test`.** Expected: `input_math_tests: all passed`.
- [ ] **Step 5: Commit** `feat(port): desktop input path math`

### Task C2: Wheel, arrows, pinch, Esc

**Files:**
- Create: `port/patches/0010-win32-wheel-arrows-pinch.patch` (against `cocos2dx/platform/win32/CCEGLView.cpp/.h`)
- Modify: `port/app/Classes/AppDelegate.cpp` (hosts the synthetic-touch queue if it isn't in the engine)

**Interfaces:**
- Consumes: `wheelDragPath`, `arrowSwipePath`, `pinchPaths`.
- Produces: `WM_MOUSEWHEEL` (Shift → horizontal, Ctrl → pinch), `VK_LEFT` / `VK_RIGHT` → synthetic touches. Points are queued and dispatched **one per frame** through `CCEGLView::handleTouchesBegin/Move/End`, with reserved touch ids 7 and 8, at the cursor (wheel) or the window centre (arrows). Steps are 6 per gesture and `pxPerNotch` is 120 (both tunable constants). Ignored while a real mouse button is down. Esc already maps to the keypad back event in `CCEGLView.cpp`; leave it.

- [ ] **Step 1: Implement the patch** and regenerate it with `git -C port/.engine/cocos2d-x diff`.
- [ ] **Step 2: Verify (manual, in game).** The wheel scrolls a long list such as the inventory or mail, about one row per notch. Shift+wheel and ← / → change pages on a paged screen. Ctrl+wheel zooms where the game supports pinch, and otherwise does nothing. Esc closes the current sub-screen as Android back does. Clicking and dragging still work as before.
- [ ] **Step 3: Commit** `feat(port): wheel, arrow and pinch input`

### Task C3: Enter confirms single-button dialogs

**Files:**
- Modify: `port/patches/0010-…` (route `VK_RETURN` to `pc.onEnter` when no text field has focus), `port/app/Resources/shim.js`

**Interfaces:**
- Consumes: the `pc.onEnter` hook from Task B2.
- Produces: `pc.onEnter = function () {…}` in `shim.js`. It finds the top-most visible dialog of the game's prompt type (`PromptView` and similar; inspect a live one with a scene dump like the spike's `post.js` dumper). If that dialog has exactly one enabled button, it invokes that button's handler; otherwise it does nothing.

- [ ] **Step 1: Inspect** a live reminder dialog (log in with an empty email field to get "You can only use mailbox format…") and note its button and handler names in a comment.
- [ ] **Step 2: Implement** the host routing and `pc.onEnter`.
- [ ] **Step 3: Verify.** With that reminder open, Enter closes it. On a two-button dialog, Enter does nothing. While typing in a text field, Enter submits the field (C5), not a dialog.
- [ ] **Step 4: Commit** `feat(port): Enter confirms single-button dialogs`

### Task C4: Resizable window and F11 fullscreen

**Files:**
- Create: `port/patches/0011-win32-resize-fullscreen.patch` (against `CCEGLView.cpp/.h`)

**Interfaces:**
- Produces: `WS_OVERLAPPEDWINDOW` with a resizable frame. `WM_SIZE` with a non-zero size → `setFrameSize(w,h)`, then re-apply the stored design resolution size and policy so the scale and viewport are recomputed. `SIZE_MINIMIZED` and zero sizes skip that and keep the existing background/foreground calls. F11 toggles borderless fullscreen on the current monitor and back to the previous window rect. `--fullscreen` starts in fullscreen.

- [ ] **Step 1: Implement the patch.**
- [ ] **Step 2: Verify.** Drag-resize to a tall shape and a wide shape: the game stays centred and letterboxed, and clicks land on what's shown. Minimize then restore: no crash, same scale. Resize to the smallest the frame allows: no crash. F11 → fullscreen; F11 → back to the previous size. Start with `--fullscreen`: starts fullscreen.
- [ ] **Step 3: Commit** `feat(port): resizable window and F11 fullscreen`

### Task C5: Inline text editing

**Files:**
- Create: `port/patches/0012-win32-inline-editbox.patch` (against `extensions/GUI/CCEditBox/CCEditBoxImplWin.cpp/.h`)

**Interfaces:**
- Produces: `CCEditBoxImplWin::openKeyboard()` no longer opens `CWin32InputBox`. Instead it places a native Win32 `EDIT` child window exactly over the edit box's on-screen rect (node bounds → GL view → window pixels, so it follows C4's scaling). It uses `ES_PASSWORD` for password boxes and the box's font size, initial text and max length. Enter commits and calls the delegate's `editBoxReturn`; Tab commits and opens the next visible edit box in creation order (keep a static registry of live instances); Esc or clicking elsewhere commits and closes. Commit = `setText` plus `editBoxTextChanged` and `editBoxEditingDidEnd`. Ctrl+V comes free with `EDIT`. Window resizing while editing repositions the control.

- [ ] **Step 1: Implement the patch.**
- [ ] **Step 2: Verify.** On the login form, click EMAIL: a caret appears in the field itself, with no pop-up. Typing shows text, Tab moves to PASSWORD (masked), and Enter submits the login. Ctrl+V pastes. Resizing while the field is open keeps the caret over the field.
- [ ] **Step 3: Commit** `feat(port): inline text editing`

### Task C6: Play-test friction pass and acceptance

**Files:**
- Create: `docs/superpowers/notes/2026-xx-xx-playtest-friction.md` (date of the session)

- [ ] **Step 1: Full acceptance from scratch.** Delete `%LOCALAPPDATA%\NinjaHeroesPC` and the game's local-storage folder, then run `port/build.ps1 -Install` and the launcher with the APK. Expected: extract → update to current → login form → the owner logs in → server list → PLAY → in game. Restart → straight to server selection, no download. The email check on both logs returns `False`.
- [ ] **Step 2: The owner plays a normal session.** Record each remaining friction point in the notes file: screen, what happened, what was expected.
- [ ] **Step 3: For each item,** add a per-screen tweak (in `shim.js` or a new numbered patch) and re-verify, or mark it accepted together with the owner. Done when every item is fixed or accepted.
- [ ] **Step 4: Commit** `docs: play-test friction list resolved`
