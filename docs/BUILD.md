# Building and testing OpenTrail

- `make` builds `build/opentrail` (raylib, from `third_party/raylib-install`,
  built by `scripts/build_raylib_linux.sh`); `make all` also runs the tests.
  `make run` starts the game.
- `make windows` cross-compiles `build/opentrail-x64.exe` and `-x86.exe`
  (mingw-w64; raylib by `scripts/build_raylib_windows.sh`); `make mac` builds
  a universal `build/opentrail-mac` on a Mac (`scripts/build_raylib_mac.sh`).
- `make web` builds the two web games, `build/web/classic/` and
  `build/web/modern/` (each `index.html`, `opentrail.js`, `opentrail.wasm`,
  everything compiled in; the page is `web/shell.html`), with emcc on PATH
  and raylib from `scripts/build_raylib_web.sh`; `make web-serve` serves
  them. `make dist-linux dist-windows dist-mac dist-web` package the
  releases (`opentrail-build-N-*`; the web's are one zip per game, as the
  family's site reads them from the latest release: `opentrail-build-N-web-wasm.zip`
  the modern game, `opentrail-classic-build-N-web-wasm.zip` the classic);
  `.github/workflows/ci.yml` builds every target (pushes to main, every pull
  request), and `release.yml`, as the family's (openblocks'), makes every
  push to main a release: the next `release-N`, tagged once every build has
  passed (a manual run is a dry run). Both cache each platform's raylib
  (keyed on its script and `raylib_flags.sh`) and install packages through
  `scripts/apt_install.sh` (time-limited, retried, the downloaded packages
  kept in the CI cache, so a slow mirror is met once). The version is the
  family's (openbounty's): `v1.0.<N>`, N the release's number (the release
  workflow passes `RELEASE_VERSION`; locally the latest `release-N` tag, 0
  before the first), in a generated `build/version.h` (`OT_VERSION`); the
  host puts it in `Mach.version`, `--version` prints it, and the opening
  screen shows it at its bottom right, 6 pixels in, in each mode's own text:
  modern's font, white with a shadow, on the line of "Press any key to
  begin" and gone 3 s after it appears (`MScene.version`); on
  classic's title, the Apple's text on its own black cells.
- `make test` (ASan/UBSan):
  - `test_engine`: rules, with a scripted host (`tests/script_io.h`).
  - `test_sound`: the tune player's pitch and note length; every tune plays;
    modern mode's music keeps the original's lengths, is heard and never
    clips, loops, fades and renders the same twice.
  - `test_play`: games of random keys through the legacy screens
    (`build/play`), each to its end without a sanitizer error.
  - `test_harness` (`tests/test_harness.py`): the harness plays modern mode
    into the hunt and legacy mode to the menu (SKIP without a display).
- `build/play "KEYS" out.bin` drives the whole game headlessly (`~` =
  Return; `monkey:N:SEED` types random keys). `build/scr2png out.bin
  out.png` renders a page through the game's renderer.
- `build/harness [--modern] [--seed N] [--size WxH]` (`tools/c/harness.c`) is
  the real game (engine, screens, presenters; a hidden window) stepped by
  commands on stdin, one JSON answer per line: `key`/`type`/`click` queue
  input, `step N`, `wait` (until the game blocks on a key) and `until KIND`
  (the next engine request of that kind, `any`, or `field`: the hunt is on)
  run time, `state` gives the frame, the current request, the game, the
  modern scene and the hunt field, `snap PATH [W H]` the frame as a PNG
  (`384 252` is the modern canvas at 1x), `watch on` an event line per
  request. Time moves only when told, so runs repeat. It watches the engine
  through `ot_set_observer()`. `tools/harness.py start [--modern]` keeps one
  running behind a socket for separate shell calls (`harness.py wait`,
  `harness.py state`, `harness.py stop`; the stream is logged to
  `/tmp/opentrail-harness.log`); `Harness()` in it is the same as a library.
- `tools/playtest.py OUT [--mode modern|legacy|both]` plays whole games
  through the harness along 20 variations (paths: occupations, months,
  rivers, splits, The Dalles, deaths, Esc Esc, the menus), keeping a
  screenshot of every new screen; `docs/PLAYTEST.md` describes the player and
  the variations, and `docs/INPUT.md` how the two modes read the keys.
- New engine behaviour gets a test that names its REQ.
- Phones: `make android` / `android-play` (`ANDROID_NDK=` and
  `ANDROID_SDK_ROOT=` on the command line; raylib from
  `scripts/build_raylib_android.sh --ndk`), `make ios-sim` / `ios` on macOS;
  CI launches each on an emulator or a Simulator. `tools/store_shots.py`
  makes the store screenshots from the real game (the harness, `--touch`);
  `docs/STORE-SUBMISSION.md` is the store setup.
