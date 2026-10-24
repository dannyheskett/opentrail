# OpenTrail: architecture

A faithful reimplementation of *The Oregon Trail* (MECC, Apple II, 1985,
v1.4) in C99, in the same family as openklondike,
opensweeper and openbounty (raylib 6.0 on desktop, web and Android; Metal on iOS).

See [BUILD.md](BUILD.md) for building and testing.

## Sources of truth

- `docs/OREGON-TRAIL-SPEC.md`: the specification (REQ-NNN). Written in the
  present perfect ("the game has done X"), and it cites program:line.
- `legacy/extracted/side{A,B}/*.bas`: the original BASIC. **When the spec and
  the BASIC disagree, the BASIC wins**; fix the spec.
- `legacy/extracted/` (the files on the original disks) and `legacy/memory/`
  (two memory images of the booted game, MECC's parts only: the operating
  system's area is blanked): everything `assets/` is built from. The disk
  images are not kept (they carry Apple's DOS); nothing of Apple's is. New
  original art will replace the bundled art later, so keep asset loading
  swappable.

## Architecture

- **Layout:** `src/engine/` rules; `src/machine/` the emulated Apple II core
  both modes run on (clock, keyboard, speaker, tunes); `src/legacy/` the
  Apple II screens; `src/modern/` the new-art screens
  (raylib-free scene building); `src/host/` raylib window, input, audio,
  `main.c`, and one presenter per mode (`view_legacy.c`, `view_modern.c`).
  Only `src/host/` uses raylib, so everything else runs headless in tests.
- **Engine (`src/engine/`)**: a
  line-by-line port of the BASIC programs. It draws nothing, and it owns the
  game loop as the BASIC did. Wherever the original drew or waited for input,
  it calls the host's `io(ctx, g, Req*)` (`ReqKind` in `src/game.h`, each
  kind naming the BASIC lines it covers) and continues with the answer.
  - Keep the BASIC variable names (in comments) and cite the line numbers in
    comments, so the code can be read side by side with the listing.
  - Reproduce the original's quirks (spec §36). Also keep Applesoft semantics:
    `FOR` bodies run at least once, and both sides of `AND`/`OR` are
    evaluated, so `RND` is still consumed.
  - Money is exact cents (`my_cents`). Every price is a multiple of 5 cents,
    so this matches the BASIC except for its float noise.
  - Where the float noise shows (the miles and the weather they depend on),
    compute in Applesoft's own arithmetic, `a2num.h` (bit for bit the
    original's; left operands rounded, constants via `A2K()`).
  - Handoffs between the chained programs go through `g->mem` (the
    original's POKE/PEEK area, spec §30). FLOAT and WIN start from it alone.
  - Game over from any depth (`& RNH,"MENU"`) is `game_over()` → `longjmp`.
  - The RNG is pluggable (`RndFn`). Applesoft's own `RND` is not emulated.
- **Machine + legacy (`src/machine/`, `src/legacy/` text, gfx, dalles, floatdraw, hunt)**: the emulated
  Apple II the screens draw on. `Mach.ram` holds the hi-res pages, save-under
  buffers and loaded image sets, at the original's addresses. Each `&` command
  is a C function `amp_<cmd>` with the original arguments (`ARG_NONE` for an
  omitted one), drawing what the original machine code drew, pixel for pixel.
  Time is an emulated 6502 clock: `mach_advance()` presents a frame every 1/60 s
  of cycles. Drawing, loading and the BASIC's own time cost nothing; only the
  game's deliberate pace charges the original's cycle counts: music
  (`tune.c`, speaker clicks timestamped with `mach_click`), the hunt and the
  raft (real time), the crossing animation, and the travel days
  (`trailscr.c`, about 2 s each).
- **Legacy screens (`src/legacy/` screens, common, menu, buy, trailscr, endscr)**: the
  display half of the BASIC, ported line by line. They look as the original
  did; its slowness is not kept (instant text and pictures, no disk or
  interpreter time, a 2 s title that any key skips). `screens_io` answers each
  engine `Req`. Keep BASIC's exact PRINT semantics: `PRINT CL$` is `CL CR`,
  `PRINT X;` has no CR.
- **Host (`src/host/`; `ntsc.c` in legacy, `speaker.c` in machine)**: raylib window, keyboard
  mapped to Apple key codes, audio stream, and clean colour at 560x192 dots
  (lines doubled, whole-number scaled). Alt+Enter toggles fullscreen; Esc
  asks whether to quit and pauses the game (legacy: `src/legacy/overlay.c`, a
  41000-style box drawn on a copy of the machine; modern: a panel in
  `view_modern.c`), and Shift+Esc is the Apple's Esc. F12 or `--record [FILE.mp4]` records to MP4 (`recorder.c`, the
  family's recorder: 60 fps H.264 of game time, no sound, legacy 1120x768,
  modern 1280x720; vendored `third_party/minih264`, `minimp4`). As in the
  family's games (openblocks' `present.c`), each video frame is drawn at twice
  the size with the views free to scale by fractions (modern fills the frame),
  colour blended but alpha kept opaque, then minified bilinearly; the REC mark
  is the window's only; a new game while recording starts a new auto-named
  file (`opentrail-YYYYMMDD-HHMMSS.mp4`). The game owns the loop on the main thread
  (the web build runs it under ASYNCIFY; iOS will need a game thread). Each
  frame is drawn, then `WindowShouldClose()` (on the web it yields to the
  browser, which is when the keys arrive), then the keys are read:
  `EndDrawing` empties raylib's key queue, so keys read before the yield
  were lost on the web. The modes: `--classic` (`--legacy`) or `--modern`,
  else `chooser.c` asks in the window (modern's title picture and font); a
  web build is one mode, `-DOT_WEB_MODE=0|1`, keeps its files in IndexedDB
  (`main.c`), and quitting reloads the page.
- **Hosts on phones**: the host's raylib-free half is `src/host/core.c` (the
  original's files, the sound's ring) and `persist.c` (the graves and the Top
  Ten), shared by every host. Android is the raylib host with
  `platform_android.c` (logcat, the app's data directory, the safe insets from
  `android/`'s OpenTrailActivity) and modern mode by touch. iOS links no
  raylib: `ios/host_ios.c` runs the game on its own thread, draws the
  software canvas and hands it to Metal as one texture (`present_metal.m`),
  mixes the sound into AVAudioEngine (`audio_ios.m`), and turns UIKit's taps
  (`ios_main.m`, `plat_ios.m`) into keys; `make test`'s `ios-purity` keeps
  the iOS build's C free of raylib. Both are landscape, modern mode only.
- **Assets (`assets/`, built by `tools/art.py assets`)**: raw original file
  bodies plus the boot title and the font cut from the boot image. They are
  loaded through `Mach.load`, and compiled into the game
  (`tools/art.py embed` -> `build/embed.c`, `src/host/embed.h`), so each
  build is one file: raylib linked statically and trimmed to the features the
  game uses (`scripts/raylib_flags.sh`), stripped (Linux ~2.6 MB, needing
  only libc and libm). Modern mode's pictures are compiled in too
  (`tools/art.py embed --art` -> `build/embed_art.c`, `EMBEDDED_ART`;
  `OPENTRAIL_ART=DIR` loads `DIR/<id>.png` instead, to try art without a
  rebuild). The web's classic game carries none (`build/embed_art_none.c`).
- **Modern mode**: a second shell answering the same engine
  Reqs with new Retro Diffusion art on a 384 by 252 canvas (1152x756 at 3x;
  a wider window or phone gets plain bars beside it): the art's 216 rows
  (`MS_ART_H`) at the top, a band under them for a caption and the prompt;
  scene and UI as separate layers
  so portrait can stack them. Retro Diffusion jobs cost money: run them
  deliberately (`tools/art.py cost` first).
  - Art: everything Retro Diffusion is in one committed tree, `art/`:
    `manifest.json` lists every image (its size and its `source`),
    `jobs/<id>.json` are the jobs, `inputs/` the images jobs start from,
    `runs/<job>/runNN/` the output. Every image in `assets/modern/` has
    exactly one source: the one approved run of its job (`01_raw.png`, an
    animation's `frame_NN.png`, or `02_edited.png` with `edit.json` when it
    was fixed after generation), or `tools/art.py local` (drawn in code: the
    map, the two river scenes (the crossing composed, the Columbia from the
    original's layout, both in the generated scenes' colours), the six
    travel backdrops, the divider, and the
    text font `ui_font`, Press Start 2P rendered at its own 8 px into 8x8
    cells from `third_party/pressstart2p/`, SIL Open Font License 1.1). A job makes
    one image, or several from one run (an animation's frames; the four
    river crossings share `travel_icon_01`), or the start frame of another
    job (`hunt_hunter_dirs` and the two `hunt_hunter_*_diag`: the hunter's
    turn and his diagonals, cut into `inputs/`). Each job keeps exactly one
    run, holding only the files used and its
    `job`/`request`/`task`/`poll` records; every file in `inputs/` is a
    kept job's input or reference, or an input of `art.py local`
    (`map_clean.png`, the map's base). Four jobs (`travel_icon_01`, `_16`,
    `_17`, `travel_wagon_anim`) started from inputs made in `build/` that
    were not kept: their runs are the record, and they cannot be re-run as
    they are. Nothing generated lives anywhere else (`build/originals/` is
    only the original's art, cut out of `legacy/` again on demand). One tool, `tools/art.py`, does it all: `cost`/`run`
    a job, `seam`, `gallery` (the review page: file, original, new art),
    `local`, `anchors`, `keep`/`check` (post-processing, loop check), `plan`
    (`art/PLAN.md`: every job, its images, its run, its price). Approved art is copied into `assets/modern/`
    by hand, and every image is approved by eye first.
  - The recipe that has worked: the game's own size; a prompt that describes
    the subject alone (not a whole frame) and ends with a plain background
    colour that contrasts with it ("on a plain magenta background"; dark blue
    behind gold lettering), with `remove_bg`; prompt expansion on; seed 1848;
    `rd_pro__default` for sprites ($0.18), `rd_plus__environment` for scenes
    ($0.06). Likeness: the original (or an approved image) as
    `reference_image_paths` (RD Pro only), which guides without copying; a
    complex figure (the raft) does better with no reference; the thief took the
    approved hunter (`inputs/style_ref_hunter.png`) only as a style reference. A
    base image (`input_image_path`, `strength`) at sprite size comes back as
    a copy, so only the fire and broken part (the modern wagon, on black)
    start from one at that size. RD Pro (the references) stops at 256 px, so
    a scene that must match other art starts from a base made of approved
    art instead (`inputs/scene_landmark_NN_base.png`), run at `strength`
    0.55, which keeps the layout and redraws it as one picture: Fort
    Laramie's is the old vista with the trail icon's fort put in, Fort
    Boise's with the icon's tower and wall for the stockade, Fort Hall's with
    its white walls in the icon's tan. Prompts alone do not move
    `rd_plus__environment` off its centred one-point layout, and the run may
    still change details (Fort Hall's chimneys came back a cross and a tree). Animations
    (`rd_advanced_animation__walking` $0.14, `__custom_action` $0.25) start from
    the approved still padded for motion room, 4 frames (`frames_duration`
    4, `return_spritesheet`), at sizes in multiples of 8. Frames that fill
    their image (panel, button, the Top Ten scroll) run with `remove_bg` off.
    The hunting grounds are a 64x64 `rd_tile__single_tile` tile repeated to
    384x216 (that style rejects `tile_x`/`tile_y`). One figure in several
    poses comes from one image, so it stays one figure at one size: the
    hunter is the approved right-aim still padded to 48x64, turned to all
    8 directions by `rd_advanced_animation__rotate` ($0.10, a 3x3 sheet:
    `hunt_hunter_dirs`), and each aim's walk starts from its cell (north
    from behind, north-east 3/4 from behind, east, south-east 3/4 from the
    front, south from the front: the field's 3/4 view, not the sky and the
    ground); the game mirrors the east side for the west. An animal's dead
    pose is the last frame of a `custom_action` from its run's start frame.
    No inpainting. After generation, only the
    leftover background may be made transparent: by Retro Diffusion's
    `background_remover` ($0.01; send it the image on one flat colour) or by
    hand, and the run records it.
  - Music (`src/modern/music.c`, raylib-free): the original's tunes
    (MS0-MS17.BIN, TS.BIN, already embedded) played by a synthesised
    orchestra, rendered on the fly on the emulated clock and mixed by the
    host (`host_set_music`, stereo). The melody is the original's, note for
    note and at its tempo (in concert pitch: MIDI = pitch + 41); the harmony
    is worked out at load (key, metre, a chord per bar), and the parts are
    flute, horn or violins on the melody, a string pad, cellos, a harp and
    timpani, through a small hall. It plays where the original's tunes did
    (`wait_tune`, the tune-playing "Press SPACE BAR": Independence and each
    landmark after looking around, the leg message, the arrival, the
    tombstone and all dead) and stabs on a raft collision; Ctrl-S silences
    it. No audio files: `build/music_wav OUTDIR` renders previews (and the
    speaker's originals) to WAV for listening. Legacy mode keeps the speaker.
  - Drawing: `src/modern/draw.c` lays every screen out on a software canvas
    (`canvas.c`: RGBA, 384 by 252, the GPU's pixel-centre rule,
    nearest sampling, PNGs by stb_image), raylib-free; a host only uploads it
    as one texture and scales it (`view_modern.c`), so every platform shows
    the same pixels.
  - Text and layout (`draw.c`): one fixed-width 8 px font (`ui_font`),
    so tables line up. Laid out as the original's screens (`PANEL_*` in
    `modern.h`): a text screen of the original's is a page (`page()`), alone
    on the screen: the title centred, the dividers at 34 and 210 where the
    original has them (`rule`), the text, the prompt centred at the bottom;
    a page may carry a picture (`spot`) and the hunt's expert keys
    (`keys_x/y`, the hunting instructions). The travel screen is the
    original's: the scene (0-128), a box on its ground under the wagon for a
    message or a question (from y 80; Yes and No side by side), "Press
    RETURN to size up the situation" under the scene (131), the status as six
    lines with the labels' colons in a column (146-222), the box's "Press
    SPACE BAR" at the bottom (234). A landmark's picture looked at is whole,
    its name and the date in a band under it (`PANEL_CAPTION`). The pictures
    of the original (the title and menu, Matt's store, the map, the hunt, the
    raft, the tombstone) keep their art, a box at the bottom; Matt's panel is
    tall, his portrait at its right, standing on the floor, its hint in the
    band under the picture. Where nothing else uses that band, the scene's
    status bar does (`MScene.bar`): the hunt's bullets and keys, the raft's
    river and keys, the crossing's river, date and method. Long choices,
    titles and hints wrap. Matt's store shows alone before his greeting.
  - Touch (`docs/INPUT.md`): `MScene.touch` (set by the host) makes the
    words say "tap" and the scene carry tap targets: choices at least 20
    pixels tall, `MScene.btn` (the hunt's Walk and Fire, the raft's halves),
    `tap_key` (Return on the travel screen), the hunt's aim from
    `hunter_x/y`; `draw.c` draws the keyboard, the number pad and the menu,
    and `modern_tap()` turns a tap into keys. A touch host scales the canvas
    by fractions to fill the screen.
  - Modern's keys (`docs/INPUT.md`): every choice is numbered and its digit
    picks it (yes/no too: 1 Yes, 2 No, as well as Y/N; the quit dialog the
    same); choices and yes/no take their key at
    once and let a Return straight after go; Esc Esc leaves the game; the
    tune waits go on at any key but Ctrl-S; Space leaves Matt's store.
  - Opening (`intro()` in `modern.c`, once at start-up): the logo fades in on
    black (1 s), then `scene_title` behind it (1 s), then "Press any key to
    begin" opens the menu; a key during the fades shows it all at once.
  - Travel screen (`travel_view()` in `modern.c`, `draw_travel()` in
    `draw.c`): a backdrop `travel_bg_<prairie|mountains>_<grass|dry|snow>`,
    192x64 drawn at 2x (the scene is 384x128), looping left to right and
    panned right with the landmark while the ox-and-wagon walks in place
    (`travel_wagon_1..8`, 160x64 frames, an 8-frame walk; `WAGON_X/Y` in
    `modern.c`); its ground line is y=76, the horizon y=56. All six
    backdrops are drawn by `art.py local`. The landmark icons (`travel_icon_NN`) come in from the left with
    the pan and stop with their right edge at `LANDMARK_STOP`, and stay on
    arrival (the original keeps the icon while it asks to look around); the
    wagon (`WAGON_X` 191) leaves the thief room behind it, at its rear as the
    original's; `LANDMARK_ART`
    in `modern.c` gives each one's bottom row: rivers on the water line
    (y=89), the others on or below the ground line, Chimney Rock and the Blue
    Mountains on the horizon at 2x.
  - Hunt (`hunt()`/`hunt_frame()` in `modern.c`): the original's game
    (`hunt.c`) runs as it is; before each frame `hunt_view()` reads the field
    back (scenery, carcasses, animals, the hunter, the shot) and `hunt_frame()`
    rebuilds it as sprites on `scene_hunt_zone<ZO>` at the original's
    positions scaled to the canvas, each standing with its feet at the middle
    of its box's bottom edge. The feet points (`src/modern/hunt_anchor.h`) are
    measured from the art by `tools/art.py anchors` (run it after changing a
    hunt image; `make test` fails while it is stale). The animals run a
    4-frame cycle; the hunter's pose picks his aim, mirrored for the west.

## Conventions

- C99, 4-space indent, `//` comments, family naming (types `Game`, `Req`;
  functions `snake_case`).
