# OpenTrail

*The Oregon Trail* (MECC, Apple II, 1985) rebuilt in C: the original game,
rule for rule and screen for screen, without its 1985 waiting. Published
with the permission of the original game's rights holder (see
[NOTICE](NOTICE)).

Two ways to play, on one engine:

- **Classic**: the original's screens, ported line by line from its BASIC
  display code onto an emulated Apple II screen, drawn at the Apple's 560x192
  dots in clean colour. Its text, graphics, music and raft sprites are rebuilt
  from the original machine code. Text and pictures appear at once and there
  are no disk waits; the music, travel days, hunt and raft keep the
  original's pace.
- **Modern**: new pixel art on a 384x252 canvas, laid out as the original's
  screens, with the original's tunes played by a synthesised orchestra. The
  hunt is the original's game, drawn with the new hunter, animals and
  hunting grounds. Every choice is numbered, so any menu can be answered
  with the number keys alone (1 Yes, 2 No).

## Platforms

| Platform | Build | Modes |
|---|---|---|
| Linux (x86_64) | one file, needs only libc and libm | both (it asks) |
| Windows (x64, x86) | one `.exe` | both |
| macOS (universal) | one file | both |
| Web | two games, each a page, a script and one WebAssembly file | classic, modern |
| Android (arm64) | an APK, and an App Bundle for Google Play | modern, by touch |
| iOS (arm64) | an app for the App Store (TestFlight first) | modern, by touch |

Every build carries everything it needs: both modes, the original's data and
the modern art are compiled in.

## Playing

Download the latest release for your platform from the
[releases page](https://github.com/dannyheskett/opentrail/releases) and run
it. `opentrail --classic` or `--modern` skips the question; `--version`
prints the version, which the opening screen also shows.

The Top Ten and the graves are kept in your data directory
(`~/.local/share/opentrail`, `%APPDATA%\opentrail`,
`~/Library/Application Support/opentrail`), and on the web in the browser.

## Controls

The game's own keys: digits for choices, Return, the space bar, the arrow
keys (the hunt, the raft), Ctrl-S for sound. In modern mode a click picks a
choice too. Esc asks whether to quit (Shift+Esc is the Apple's Esc; Esc Esc
abandons the game and goes back to the menu, as in the original),
Alt+Enter toggles fullscreen, F12 starts and stops recording an MP4 (60 fps,
no sound) in the working directory, and `--record [FILE.mp4]` records from
the start. `--touch` plays modern mode by taps, as on a phone (tap the
choices, the on-screen keyboard, the hunt's field and buttons; a menu button
at the top right). [docs/INPUT.md](docs/INPUT.md) has every prompt's keys
and taps.

## Building

```
scripts/build_raylib_linux.sh   # once: raylib 6.0 into third_party/
python3 tools/art.py assets     # once: assets/ from legacy/
make                            # build/opentrail
make run                        # play it
make test                       # the tests (ASan/UBSan)

scripts/build_raylib_windows.sh # once (mingw-w64)
make windows                    # build/opentrail-x64.exe, -x86.exe

scripts/build_raylib_mac.sh     # once, on a Mac
make mac                        # build/opentrail-mac (arm64 + x86_64)

source path/to/emsdk/emsdk_env.sh
scripts/build_raylib_web.sh     # once (Emscripten)
make web                        # build/web/classic/, build/web/modern/
make web-serve                  # both at http://localhost:8080/
```

Android needs the SDK, the NDK (26.3) and a JDK 17, passed on the make command
line; iOS needs macOS with Xcode (26+) and links no raylib:

```
scripts/build_raylib_android.sh --ndk <ndk>
make android ANDROID_NDK=<ndk> ANDROID_SDK_ROOT=<sdk>        # build/opentrail.apk
make android-play ANDROID_NDK=<ndk> ANDROID_SDK_ROOT=<sdk>   # build/opentrail.aab
make ios-sim                    # build/ios-sim/OpenTrail.app (Simulator)
make ios                        # build/OpenTrail.ipa
```

Each raylib script cleans raylib's shared source tree before it builds, so
the archives can be built in any order. `make test`'s harness test needs a
display and Python with numpy and Pillow; without a display it is skipped.
[docs/BUILD.md](docs/BUILD.md) covers the tests, the harness (the real game
stepped by commands, for scripted play and screenshots) and the
play-tester.

## Releases

Work branches merge into `staging` (squash) and `main` takes merges from
`staging` only; `.github/workflows/ci.yml` builds and tests every pull request,
and every job must pass. Merging `staging` into `main` runs
`.github/workflows/build.yml`: it numbers the build (the next `release-N`,
version `v1.0.N`), builds every platform, and once every build has passed tags
the commit and publishes a GitHub Release. It never ships to a store.

Shipping is the separate, manual `.github/workflows/release.yml`: it takes a
`release-N` and ships it to Google Play (the track you pick), then the App
Store (TestFlight for internal testers, then App Review -- withdrawing any
earlier version still pending review), stops at the first failure, and reads
each store back at the end. [docs/STORE-SUBMISSION.md](docs/STORE-SUBMISSION.md)
covers the stores.

## How it is made

- [docs/OREGON-TRAIL-SPEC.md](docs/OREGON-TRAIL-SPEC.md): the specification
  (REQ-NNN), recovered from the original: its BASIC listings, data tables,
  and its machine code by disassembly.
- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md): how the code is laid out: a
  rules engine that is a line-by-line port of the BASIC, an emulated Apple II
  core both modes draw on, the classic and modern screens, and a raylib host.
- [docs/PLAYTEST.md](docs/PLAYTEST.md): the automated play-tester.

| Path | Content |
|---|---|
| `src/engine/` | the rules: the original BASIC's logic, no display |
| `src/machine/`, `src/legacy/` | the emulated Apple II and the classic screens |
| `src/modern/` | the modern screens and the music (raylib-free) |
| `src/host/` | the raylib window, input, audio, and one presenter per mode |
| `legacy/` | the files from the original disks and MECC's machine code (the sources of `assets/`) |
| `assets/` | the original data files and `assets/modern/` the modern art, all compiled into the game |
| `art/` | the modern art's sources: the manifest, the Retro Diffusion jobs, their inputs and the approved run of each |
| `tools/` | `art.py` (assets and art), the harness, the headless player, the play-tester |
| `tests/` | engine, sound and harness tests |
| `third_party/` | the MP4 recorder's encoders and the Press Start 2P font; raylib is built here |

## Credits

*The Oregon Trail* (1985) was made by MECC. OpenTrail is by Dan Heskett.
The modern art was made with [Retro Diffusion](https://www.retrodiffusion.ai/)
and in code; the text font is Press Start 2P by CodeMan38. Built with
[raylib](https://www.raylib.com/).

## License

OpenTrail's code and its modern art are under the MIT License
([LICENSE](LICENSE)). The original game's content (`legacy/`, `assets/*.bin`,
and its text, rules and data as ported) is not: it is published with the
permission of its rights holder. The Oregon Trail is a trademark of its
owner; OpenTrail is not affiliated with or endorsed by the trademark owner
or by Apple Inc. See [NOTICE](NOTICE).
