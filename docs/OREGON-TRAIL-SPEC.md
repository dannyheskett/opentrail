# The Oregon Trail (1985), Specification

**Status:** living document. Reproduction-grade record of *The Oregon Trail*
(MECC, Apple II, 1985; management screen reports **Version 1.4**) as recovered
from the original two-sided disk images. This is the single authoritative
specification of the game's rules, data, and program structure for the
OpenTrail project.

A complete reimplementation of the game has been intended to be possible from
this document alone, paired with the original material under `legacy/`:
`legacy/extracted/` (BASIC listings, data tables, text databases) and
`legacy/memory/` (memory images of the booted game, with the operating
system's area blanked).

The game has been written almost entirely in **Applesoft BASIC**, split across
several chained programs and overlay "libraries", plus a machine-language
**ampersand (`&`) extension** that supplies graphics, disk I/O, text windows,
music, and the hunting minigame. All BASIC has been recovered losslessly by
detokenization, so every rule below that lives in BASIC is exact. Rules that
live in the machine-language extension are marked *(inferred)* until that code
is disassembled (§37).

**Conventions.**
- Each requirement carries a stable identifier of the form `REQ-NNN`. Where a
  value comes from the preloaded variable table, the requirement names the
  array (e.g. `VAR.BIN:LM(*,0)`) rather than repeating it, so the data and the
  spec stay synchronised; Appendix A reproduces every table once.
- Requirements are written in the present perfect tense ("the game has done
  X"), as a factual reproduction-grade record, matching the house style of
  `OPENBOUNTY-SPEC.md`.
- Code citations name a **program and BASIC line number** (e.g.
  `OREGON TRAIL:3230`). Unlike a modern codebase, the original source is frozen,
  so line numbers are stable identifiers. Library files are cited by file name
  (e.g. `RIVER.LIB:50150`); `.LIB` code is appended to the running program at
  line 50000+ (§2.3).
- Program names follow the disk catalog: `HELLO`, `MENU`, `MANAGEMENT`,
  `BUY SUPPLIES`, `OREGON TRAIL`, `FLOAT`, `WIN`, and the `*.LIB` overlays.
  Where two disk sides carry different copies of a file (`MENU`, `HELLO`,
  `COMMON.LIB`, `FLIP.LIB`), the side is named (`MENU (side 2)`).
- Applesoft boolean arithmetic is transcribed literally: a comparison evaluates
  to `1` (true) or `0` (false), so `(LM > 2) + (LM > 5)` is a count. `RND` means
  a uniform draw in `[0, 1)`. `INT` is floor.
- "The game" has referred to the original MECC program. "The player" has
  referred to the human user. "The party" has referred to the living wagon
  members; "the leader" has referred to party slot 0.

**Out of scope of the requirements (documented for context):**
- Pixel-exact screen layout has not been mandated; the spec names screens,
  prompts, windows, and text, but small visual deltas are conformant.
- Execution timing (BASIC interpreter speed) has been left to the
  implementation, except where it defines gameplay (the raft game's tick count,
  §26.4).
- Copy protection, disk-swap prompts, and error-trap plumbing are documented
  (§34–§35) but need not be reproduced.

---

## Table of contents

**Part I: Source & architecture**
1. [Provenance and source material](#1-provenance-and-source-material)
2. [Program architecture](#2-program-architecture)
3. [Data types and conventions](#3-data-types-and-conventions)
4. [Game state, complete variable map](#4-game-state-complete-variable-map)

**Part II: Gameplay**
5. [Game model and lifecycle](#5-game-model-and-lifecycle)
6. [Randomness](#6-randomness)
7. [Setup: occupation, party, departure month](#7-setup-occupation-party-departure-month)
8. [Matt's General Store](#8-matts-general-store)
9. [The trail: landmarks, legs, and splits](#9-the-trail-landmarks-legs-and-splits)
10. [The travel day](#10-the-travel-day)
11. [Calendar](#11-calendar)
12. [Climate and weather](#12-climate-and-weather)
13. [Health](#13-health)
14. [Illness, injury, and death](#14-illness-injury-and-death)
15. [Food and rations](#15-food-and-rations)
16. [Pace and mileage](#16-pace-and-mileage)
17. [Random events](#17-random-events)
18. [Wagon breakdowns and oxen](#18-wagon-breakdowns-and-oxen)
19. [The action menu](#19-the-action-menu)
20. [Resting and lost days](#20-resting-and-lost-days)
21. [Hunting](#21-hunting)
22. [Trading](#22-trading)
23. [Fort stores](#23-fort-stores)
24. [Talking to people](#24-talking-to-people)
25. [River crossings](#25-river-crossings)
26. [The Dalles: Columbia River or Barlow Road](#26-the-dalles-columbia-river-or-barlow-road)
27. [Arrival and scoring](#27-arrival-and-scoring)
28. [Tombstones](#28-tombstones)

**Part III: Files, UI, platform**
29. [Disk layout and file formats](#29-disk-layout-and-file-formats)
30. [Inter-program memory map](#30-inter-program-memory-map)
31. [UI: screens, windows, and prompts](#31-ui-screens-windows-and-prompts)
32. [Input and controls](#32-input-and-controls)
33. [Sound and music](#33-sound-and-music)
34. [Management options](#34-management-options)
35. [Error handling and disk prompts](#35-error-handling-and-disk-prompts)

**Part IV: Quirks & open items**
36. [Known quirks and defects in the original](#36-known-quirks-and-defects-in-the-original)
37. [Open items](#37-open-items)
- [Appendix A, Complete data tables](#appendix-a-complete-data-tables)
- [Appendix B, Ampersand command table](#appendix-b-ampersand-command-table)
- [Appendix C, Extracted artwork](#appendix-c-extracted-artwork)
- [Document history](#document-history)

---

# Part I, Source & architecture

## 1. Provenance and source material

### 1.1 The original disks

- **REQ-001.** The source has been the game's two-sided disk (side 1 and side
  2). The disk images themselves are not part of this project; MECC's files
  from them are, in `legacy/extracted/sideA` and `sideB` (the BASIC programs
  as listings, the data files as their raw bodies), with two memory images of
  the booted game holding MECC's machine code (`legacy/memory/`: the
  ampersand extension and the hunting game, everything else blanked). Each
  disk's catalog has begun with a MECC disk-label pseudo-entry
  (`[OREGON TRAIL SIDE 1 /0414]`) that is not a file.
- **REQ-002.** The variable table (`VAR.BIN`, REQ-271) has been decoded into
  Appendix A; the machine code has been read by disassembly.

### 1.2 Authorship and revision

- **REQ-003.** Program headers have named the authors and revision dates:

| Program | Author (REM) | Last update |
|---|---|---|
| `OREGON TRAIL` | J. Krenz ("COPYRIGHT (C) MECC 1984") | 86/02/20 JJK |
| `MENU` | Krenz | 85/07/12 JJK |
| `MENU (side 2)` | John J. Krenz | 85/07/17 JJK |
| `BUY SUPPLIES` | John J. Krenz | 85/07/18 JJK |
| `MANAGEMENT` | John J. Krenz | 87/02/24 JJK |
| `WIN` | John J. Krenz | 85/07/23 JJK |
| `FLOAT` | Steven Splinter | 07/18/85 |
| `HELLO` | Craig W. Copley | 86/01/03 RMS |

- **REQ-004.** The `MANAGEMENT` title screen has reported "The Oregon Trail /
  Version 1.4" (`MANAGEMENT:1000`). The latest header date (87/02/24) has
  placed this image at a 1987 revision of the 1985 release.
- **REQ-005.** Side B has also carried `TEST FLOAT`, a developer harness that
  seeds the memory handoff area with fixed values (five members named Steve,
  Craig, John, Phillip, Roger) and chains straight to `FLOAT`. It is not
  reachable from the game.

### 1.3 Relation to other versions

- **REQ-006.** This specification has described the Apple II 1985 game only.
  Other editions (including the 1990 MS-DOS port) are out of scope.

---

## 2. Program architecture

### 2.1 Chained programs

- **REQ-010.** The game has consisted of separate BASIC programs that replace
  one another in memory via `& RNH,"name"` (`$89F2`: load the file; for a
  BASIC program, clear its variables and run it; for a binary, call its load
  address). State has crossed
  program boundaries only through fixed memory locations (§30) and disk files.
- **REQ-011.** The program chain has been:

```
HELLO ─► (boot machine code) ─► MENU
MENU ──(1. Travel the trail)──► BUY SUPPLIES ─► OREGON TRAIL
MENU ──(Ctrl-A)──────────────► MANAGEMENT ─► MENU
OREGON TRAIL ─(Dalles, float)─► FLOAT ─► WIN ─► MENU
OREGON TRAIL ─(Willamette)────► WIN ─► MENU
OREGON TRAIL ─(party dies)────► MENU
```

### 2.2 Disk sides

- **REQ-012.** Side A has held the front half of the game (menus, store, main
  program, landmarks 0–4 art and dialogue, the Top Ten). Side B has held the
  back half (landmarks 5–17 art and dialogue, `FLOAT`, `WIN`, `END.LIB`).
  Shared overlays have been duplicated on both sides.
- **REQ-012a.** Side B is not bootable into the game: booting it prints
  "Please insert side 1 of your Oregon Trail diskette. Press Space Bar to
  continue" and starts again.
- **REQ-013.** The main program has switched to side B on arrival at Fort
  Laramie (landmark 5): `OREGON TRAIL:1016` calls `FLIP.LIB`, which prompts a
  user to flip the disk when it carries the standard volume 254, or otherwise
  expects side 2 as volume + 1 on the same drive (REQ-331a).
  From then on `S = 1` and side-B files are used.

### 2.3 Overlays (`.LIB` files)

- **REQ-014.** Overlays have been appended into the running program with
  `& APP,"FILE.LIB"` (`$8A3C`): the file is loaded at the end of the resident
  program, the line chain is relinked (`$7C09`), and the end-of-program pointer
  (`$AF/$B0`) is moved past it. Overlay line numbers (50000+) are higher than
  any resident line, so the result is a valid program. It is then called by
  `GOSUB 50000` (or another entry line) and discarded with
  `& DBL,50000,60000` (delete line range; `OREGON TRAIL:190`). Only one overlay
  has been resident at a time.
- **REQ-015.** Overlay entry points:

| Overlay | Entry | Purpose |
|---|---|---|
| `COMMON.LIB` | 30000–41100 | shared helpers: Y/N prompt, space-bar wait, disk-side check, error trap, dialog box |
| `RIVER.LIB` | 50000 | river-crossing decision and outcome (§25) |
| `CROSS.LIB` | 50000 | river-crossing animation |
| `TOMB.LIB` | 50000 / 50100 | party member death / read a trailside tombstone (§28) |
| `PART.LIB` | 42000 | broken wagon part (§18.1) |
| `LF.LIB` | 50000 / 51000 / 52000 | fire / abandoned wagon / thief (§17) |
| `PACE.LIB` | 50000 | change pace (§16) |
| `RATION.LIB` | 50000 | change rations (§15) |
| `MAP.LIB` | 50000 | map screen |
| `TALK.LIB` | 50000 | talk to people (§24) |
| `BUY.LIB` | 50000 | fort store (§23) |
| `TRADE.LIB` | 50000 | trade (§22) |
| `HUNT.LIB` | 50000 | hunting wrapper around `& HUNT` (§21) |
| `END.LIB` | 50000 | The Dalles choice and hand-off to `FLOAT` / `WIN` (§26) |
| `FLIP.LIB` | 50000 | disk-side switch |

### 2.4 Line-number regions in `OREGON TRAIL`

- **REQ-016.** The main program's regions:

| Lines | Role |
|---|---|
| 100–110 | init: load `VAR.BIN`/`PT.BIN`, append `COMMON.LIB`, define functions, delete init code |
| 190–960 | utilities: overlay unload, money format, date string, status screen, lost-day helper, oxen/speed recompute, message boxes |
| 1000–1020 | **landmark loop** (arrive, look around, cross, choose route, travel) |
| 2100–2200 | trail split and leg announcement |
| 3000–3499 | **travel leg and daily tick** |
| 3500–3505 | river crossing driver |
| 4000–4900 | action menu and handlers |
| 8000 | death handler |
| 10000–11505 | random event handlers and party-member picker |
| 21000 | "no oxen" check |
| 29000–29020 | one-time state load from the memory handoff area (deleted after use) |

### 2.5 Boot and machine-code layout

- **REQ-017.** Side A's first tracks have been outside the catalog and hold
  the boot loader, the MECC title screen, the ampersand extension and the
  hunting game. Booting has shown the title (RLE data at `$7623`, the same
  format as §29.5 but with its own decoder, which ends only on exactly `$C0`),
  loaded the hunting game into language-card RAM bank 2 (`$D000–$FEFF`) and
  the ampersand extension into `$6000–$92FF` (initialised by `JSR $6000`),
  and handed over to a small post-boot loader at `$3000`.
- **REQ-018a.** The post-boot loader (`$3000`) has finished by running the
  BASIC program `MENU` (`$308A`); its alternate entry `$3003` runs a program
  named `ENTER` instead (`$308E`), which is not on these disks (presumably a
  hook for MECC menu or network installations).
- **REQ-018b.** Memory layout while the game runs: BASIC programs load at the
  normal `$0801` (the main program reaches past `$2000`), so the game
  **displays hi-res page 2** (`$4000`; `$E6 = $40`) and reuses page 1's memory
  as buffers: `$3000` (`PA`, dialog save-under) and `$3A00` (image shift
  tables, built by `& CST`, 1.5 KB). Variables start at `$9902` (LOMEM).
- **REQ-018c.** Language-card mapping (`$7BED`): addresses the BASIC passes as
  `$C000–$CFFF` are redirected to language-card RAM **bank 1** `$D000–$DFFF`,
  and `$D000+` to **bank 2**; so `& RFL,"PRAIRIE.IMA" AT 49152` loads into
  bank 1, not the I/O page.
- **REQ-019.** The ampersand extension has hooked the `&` vector (`$03F5`) and
  dispatched by name (`$602B`): it matches the text after `&` against a table
  of 46 command names at `$8FE6` (last character of each name has bit 7 set)
  and calls the entry in the address table at `$908A` **plus one**. `&&`
  restores the previous `&` vector. Appendix B lists every command.

---

## 3. Data types and conventions

- **REQ-020.** Numbers have been Applesoft 5-byte floats (≈9 significant
  digits); `%` variables are 16-bit signed integers; strings are ≤255 chars.
  Oxen can be fractional (half an ox after an injury, §18.2).
- **REQ-020a.** The arithmetic has been Applesoft's own, and it shows where a
  running total lands on a whole number (REQ-022). Its rules: the
  evaluator rounds each value it pushes, so in `A op B` the left operand is
  rounded and the right one may still carry its rounding byte;
  `INT` truncates the unrounded value; the multiply drops the rounding byte
  when a multiplier byte is zero; the divide rounds its divisor first; and
  a decimal constant is read as a whole number divided by 10 once per
  decimal place, so `.06`, `.02`, `.01`, `.007`, `.003` and `.16` have come
  out one unit above the nearest float (`src/engine/a2num.c`).
- **REQ-021.** Money has been kept in dollars as a float in-program (`MY`),
  displayed to two decimals by `OREGON TRAIL:200` (`INT(V*100+.5)`). Across
  program boundaries it has been encoded as described in §30.
- **REQ-022.** Distances have been in miles (float); `D` counts down to the
  next landmark and `M` counts up the total. The daily panel (3265) has
  rounded, `INT(x + .5)`, but the travel screen's status block
  (`OREGON TRAIL:315`) has truncated, `INT(D)` and `INT(M)`. At a landmark
  `M` is the trail's whole mileage give or take the rounding of every day's
  `M = M + V`, so whether it has shown, say, 1219 or 1218 has depended on
  Applesoft's arithmetic exactly (REQ-020a).
- **REQ-023.** Constants `C0..C4 = 0..4`, `P5 = .5`, `PQ = .25` have been
  preloaded variables (`VAR.BIN`) used for speed in the daily loop.

---

## 4. Game state, complete variable map

### 4.1 Party and supplies

| Var | Meaning |
|---|---|
| `NP` | living party members, 5 at start |
| `N$(0..4)` | member names; slot 0 is the leader; slots ≥ `NP` hold the dead |
| `H1(i)` | member condition: `0` well, `>0` affliction index into `IL$`, `-1` dead, `-2` drowned (pending removal) |
| `H2(i)` | days of affliction remaining |
| `I(2)` | oxen (may be fractional) |
| `I(3)` | sets of clothing |
| `I(4)` | bullets (individual rounds) |
| `I(5..7)` | spare wagon wheels, axles, tongues |
| `I(8)` | food, pounds (synchronised with `PF`) |
| `PF` | food, working copy used by the daily tick |
| `MY` | money, dollars |
| `B` | pending blocker: `0` none, `2` no oxen, `5..7` broken part needing replacement; also set to `1` when the player pressed Return during a message (§32.2) |

### 4.2 Travel and time

| Var | Meaning |
|---|---|
| `LM` | index of the last landmark reached (0..17) |
| `NM` | index of the landmark being travelled to |
| `D`, `DD` | miles remaining on the current leg; leg length at start |
| `M` | total miles travelled |
| `MD` | leg base miles/day (`VAR.BIN:LM(*,1)`) |
| `P` | pace 1 steady, 2 strenuous, 3 grueling (0 while resting) |
| `R` | rations 1 filling, 2 meager, 3 bare bones |
| `AM`, `AD`, `AY` | month (1–12), day, year |
| `ZO` | climate zone 0..4 (§12.1) |
| `SD` | "stopped days" counter; non-zero means the day is a rest day |
| `LL` | `1` while on the trail, `0` while looking around a landmark |
| `Q()`, `Q1` | landmarks visited, for the map |
| `SN`, `SN()`, `ML()`, `DL`, `LN` | tombstone segment matching (§28.2) |

### 4.3 Health and environment

| Var | Meaning |
|---|---|
| `H` | party health score, 0 best … 139 worst (§13) |
| `H0` | number of members with an active affliction (per day) |
| `HR` | event health penalty for the day |
| `FS` | accumulated starvation/exposure stress |
| `TM` | temperature **category** 0..5 after each weather roll (§12.3) |
| `W` | weather display index into `W$` (0..9) |
| `PP` | precipitation flag, persists until the next weather roll |
| `QT`, `QP` | this month's base temperature (°F) and precipitation chance |
| `AR` | accumulated rain (ground moisture) |
| `AS` | snow depth, inches |
| `TR`, `TS` | today's rain and snow increments |
| `OP` | clothing per living member, `I(3)/NP` |
| `FC` | food eaten per day, `NP*(4-R)` |
| `F0` | ration stress, `2*(R-1)` |
| `BS` | base miles/day for this leg with current oxen and pace |
| `RE(0..14)` | per-event daily probabilities (§17) |

---

# Part II, Gameplay

## 5. Game model and lifecycle

### 5.1 Top-level flow

- **REQ-030.** Main menu (`MENU:1010`): `1` travel the trail, `2` learn about
  the trail (§31.3), `3` see the Oregon Top Ten (§27.4), `4` toggle sound.
  Ctrl-A opens the hidden management menu (§34).
- **REQ-031.** A new game has run, in order: occupation (§7.1), party names
  (§7.2), `BUY SUPPLIES`: departure month (§7.3), Matt's store (§8); then
  `OREGON TRAIL`: the landmark loop from Independence (§9) until arrival (§27)
  or the death of the whole party (§14.4).

### 5.2 Landmark loop

- **REQ-032.** On reaching landmark `LM` (`OREGON TRAIL:1000–1016`) the game
  has, in order:
  1. drawn `A = INT(RND*3)` (the starting Talk speaker; it advances each
     time the action menu is drawn, §24) and set `ZO` (§12.1);
  2. if `LM > 0`: loaded tune `MS{LM}.BIN`, shown "You are now at *name*.
     Would you like to look around?", and recorded `LM` in the visited list;
     at Independence the answer has been forced to Yes;
  3. on Yes: shown the landmark picture `L{LM}.PCK` with the status strip and
     opened the action menu with `LL = 0` (§19);
  4. if the landmark is a river (`VAR.BIN:LM$(LM,1) = "2"`): run the crossing
     (§25);
  5. checked for oxen (§18.3) and, if blocked, forced the action menu;
  6. at The Dalles: the float/Barlow choice (§26);
  7. at a trail split: the route choice (§9.3); choosing the map redisplays it
     and repeats from step 4;
  8. announced the next leg (§9.2) and travelled it (§10);
  9. on arrival at Fort Laramie, switched disk side (§2.2);
  10. repeated until `LM = 17` (Willamette Valley), then ran `END.LIB` (§27).

### 5.3 Game over

- **REQ-033.** The game has ended in exactly three ways: arrival at the
  Willamette Valley by Barlow Road (§26.2), arrival by the Columbia River
  (§26.3), or the death of every party member (§14.4, §26.4). There has been
  **no calendar deadline**; winter makes travel slow or impossible (§16.3) but
  never ends the game by itself.

---

## 6. Randomness

- **REQ-040.** All randomness has come from Applesoft `RND(1)`. `MENU:1015` has
  reseeded it once per menu choice with `RND(-(PEEK(78)+PEEK(79)*256))`, the
  keyboard-wait counter, so runs are not reproducible.
- **REQ-041.** A reimplementation may use any uniform PRNG. Every probability in
  this document is expressed as "`RND < p`", i.e. an event with probability
  `p`, and every range as its inclusive integer bounds.

---

## 7. Setup: occupation, party, departure month

### 7.1 Occupation

- **REQ-050.** The player has chosen (`MENU:4000–4030`):

| # | Occupation | Starting cash | Score multiplier |
|---|---|---|---|
| 1 | Banker from Boston | $1600 | ×1 |
| 2 | Carpenter from Ohio | $800 | ×2 |
| 3 | Farmer from Illinois | $400 | ×3 |

  Choice 4 has explained the difference and re-asked. The multiplier is the
  occupation number itself (`WIN:625`).

### 7.2 Party names

- **REQ-051.** The leader's first name has been required (≤ 9 characters from
  `A–Z a–z space ' . -`). Four more names have followed; pressing Return on a
  blank entry has filled that slot and all later slots with default names
  (`MENU:6000–6042`).
- **REQ-052.** Default names have been the ten names in `MENU:29000` (Zeke,
  Jed, Anna, Mary, Joey, Beth, John, Sara, Henry, Emily) placed into ten slots
  at random; slots 1–4 have supplied the defaults.
- **REQ-053.** "Are these names correct?" has allowed editing any one of the
  five names by number until the player answered Yes.
- **REQ-054.** The year has been fixed at **1848** (`MENU:9000`).

### 7.3 Departure month

- **REQ-055.** The player has chosen March, April, May, June, or July
  (choice 1–5 → month 3–7); choice 6 has shown advice and re-asked
  (`BUY SUPPLIES:6000–6110`). The departure day has always been the 1st.

---

## 8. Matt's General Store

- **REQ-060.** The player has started with the occupation cash and no
  supplies. The wagon has been free and implicit (`BUY SUPPLIES:900`).
- **REQ-061.** Items and prices (`BUY SUPPLIES:400–950`):

| # | Item | Unit | Price | Input limit |
|---|---|---|---|---|
| 1 | Oxen | yoke (2 oxen) | $40 | 1–9 yoke |
| 2 | Food | pound | $0.20 | 0–2000 lb (over 2000 rejected) |
| 3 | Clothing | set | $10 | 0–99 |
| 4 | Ammunition | box of 20 bullets | $2 | 0–99 boxes |
| 5 | Spare parts | wheel / axle / tongue | $10 each | 0–3 of each (over 3 rejected) |

- **REQ-062.** Each item has been set, not added: revisiting an item replaces
  its quantity. The running bill has been recomputed from all quantities
  (`BUY SUPPLIES:4000`).
- **REQ-063.** Leaving (space bar) has been refused if the bill exceeds cash
  ("We'd better go over the list again") or if no oxen were bought ("Don't
  forget, you'll need oxen to pull your wagon."). Otherwise the bill has been
  deducted and `OREGON TRAIL` started (`BUY SUPPLIES:5000–5020`).
- **REQ-064.** Matt's advice text has recommended ≥ 3 yoke, 200 lb of food per
  person, 2 sets of clothing per person; the advice is flavour only.

---

## 9. The trail: landmarks, legs, and splits

### 9.1 Landmarks

- **REQ-070.** There have been 18 landmarks, `VAR.BIN:LM$(0..17,0)`, each with a
  type `LM$(i,1)`: `"1"` fort/town with a store, `"2"` river crossing, `"0"`
  other. See Appendix A.1.
- **REQ-071.** River landmarks have mapped to river records (§25):
  Kansas River crossing (1) → 0, Big Blue River crossing (2) → 1, Green River
  crossing (9) → 2, Snake River crossing (12) → 3 (`RIVER.LIB:50000`).

### 9.2 Legs

- **REQ-072.** A leg has been a record in `VAR.BIN:LM(0..18,*)`: length in
  miles `LM(k,0)`, base miles/day `LM(k,1)`, destination landmark `LM(k,2)`.
  Landmark `i` has departed on leg `VAL(LM$(i,2))` by default and on leg
  `VAL(LM$(i,3))` for the alternate route (`OREGON TRAIL:2200`).
- **REQ-073.** Base miles/day has been **20** for the five legs up to Fort
  Laramie and **12** thereafter (Appendix A.2).
- **REQ-074.** Before each leg the game has announced "From *A* it is *n* miles
  to *B*." and set `D = LM(k,0)`, `MD = LM(k,1)`, `NM = LM(k,2)`.

### 9.3 Splits

- **REQ-075.** Two landmarks have offered a route choice (`OREGON TRAIL:2110`),
  listing "head for *X*", "head for *Y*", "see the map":

| At | Choice 1 | Choice 2 |
|---|---|---|
| South Pass (7) | Green River crossing, 57 mi (then Soda Springs 144) | Fort Bridger, 125 mi (then Soda Springs 162) |
| The Blue Mountains (14) | Fort Walla Walla, 55 mi (then The Dalles 120) | The Dalles, 125 mi |

- **REQ-076.** The Fort Bridger route has avoided the Green River crossing and
  reached a store; the direct Dalles route has skipped Fort Walla Walla.
- **REQ-077.** Total distances Independence → The Dalles: 1771 (Green River,
  direct), 1821 (Green River, Walla Walla), 1857 (Bridger, direct),
  1907 (Bridger, Walla Walla). The Barlow Road adds 100 miles.

---

## 10. The travel day

### 10.1 Leg setup

- **REQ-080.** At the start of each leg (`OREGON TRAIL:3000–3060`) the game has
  set the tombstone segment id `SN = NM*100 + LM`, recomputed speed and food
  figures (§15.2, §16.1), found the next tombstone on the leg (§28.2), and set
  the leg-constant event probabilities `RE(8)`, `RE(10)`, `RE(11)` (§17.1).
  Right after drawing the scene (`GOSUB 300`) it has cleared the keyboard
  strobe (`X = USR(1)`), so the Return that dismissed "Press SPACE BAR" (950,
  whose `USR(2)` leaves the strobe set) has not opened the menu on the first
  day's poll.

### 10.2 Order of operations per day

- **REQ-081.** Each day (`OREGON TRAIL:3100–3499`) has run exactly these steps
  in order. Steps marked † are skipped on rest days (`SD > 0`):
  1. `QT, QP` ← this month's climate for zone `ZO` (§12.2).
  2. † Poll the keyboard; Return opens the action menu (§32.2).
  3. Clamp `H ≤ 139`.
  4. Set the per-day event probabilities `RE(0)`, `RE(3)`, `RE(4)`, `RE(5)`,
     `RE(6)`, `RE(14)` (§17.1).
  5. `HR = 0`. † For each event `k = 0..14` in order, if `RND < RE(k)`, run its
     handler (§17.2). Several events can fire on one day, but if a handler
     leaves `B > 0` (an unrepaired broken part, no oxen, or Return pressed
     during a message) the action menu opens at once and the remaining events
     that day are skipped (`L8 = 20`, `OREGON TRAIL:3180`).
  6. † Poll the keyboard again.
  7. Affliction countdown (§14.1), giving `H0`.
  8. With probability ½, roll new weather (§12.3).
  9. Apply precipitation (§12.4).
  10. Update health and eat food (§13.2, §15.1).
  11. If `H > 139` and no river crossing is in progress, force an illness roll
      (§14.3).
  12. Update ground moisture and snow (§12.5).
  13. Move the wagon (§16.2).
  14. Clamp `H ≤ 139`.
  15. Advance the calendar (§11).
  16. Refresh the status panel (`:3260–3265`): date, weather, health and
      food only when `SR` is set; next landmark and miles travelled only on
      travel days. Rests started from the menu (where `SR = 0`) therefore
      print only the date, which `:4505` writes in the menu window.
- **REQ-082.** The day loop has repeated until `D = 0` (`OREGON TRAIL:3499`),
  then the landmark loop has resumed with `LM = NM`.

---

## 11. Calendar

- **REQ-090.** Each day has added one to `AD`. Month lengths have been 31,
  except February 28 and April, June, September, November 30; **there are no
  leap years** (1848 is treated as non-leap). December rolls to January of the
  next year. On each new month `QT`, `QP` are re-read (`OREGON TRAIL:3255`).
- **REQ-091.** The date has been shown as "*Month* *d*, *yyyy*".

---

## 12. Climate and weather

### 12.1 Zones

- **REQ-100.** The climate zone has been
  `ZO = (LM > 2) + (LM > 5) + (LM > 10) + (LM > 13)`, recomputed on reaching
  each landmark: zone 0 up to Big Blue, 1 to Fort Laramie, 2 to Soda Springs,
  3 to Fort Boise, 4 beyond. The manual's six stations (Kansas City, North
  Platte, Casper, Lander, Boise, Portland) correspond to table rows 0–5; row 5
  is never selected (§36).

### 12.2 Monthly climate

- **REQ-101.** `VAR.BIN:WC$(zone)` has held 24 characters, two per month.
  `FN W` (`OREGON TRAIL:105`) has decoded month `AM`:
  - base temperature `QT = ASC(char 2*AM-1) - 50` (°F);
  - precipitation chance `QP = .003 * (ASC(char 2*AM) - 30)`.
  Appendix A.4 lists the decoded table.

### 12.3 Weather roll

- **REQ-102.** On a roll day (probability ½), the game has drawn
  `T = QT + INT(RND*41)` (°F, uniform over `QT..QT+40`), `PP = (RND < QP)`, and
  set `W = TM = INT((T + 10) / 20)`:

| `TM` | Temperature | `W$` |
|---|---|---|
| 0 | below 10 °F | very cold |
| 1 | 10–29 °F | cold |
| 2 | 30–49 °F | cool |
| 3 | 50–69 °F | warm |
| 4 | 70–89 °F | hot |
| 5 | 90 °F and above | very hot |

- **REQ-103.** On non-roll days `TM` and `PP` have carried over from the
  previous roll.
- **REQ-104.** At game start `W = TM = INT((QT + 10) / 20)` for the departure
  month (the base temperature, no random term) and `PP = 0`.

### 12.4 Precipitation

- **REQ-105.** Every day with `PP = 1` (`OREGON TRAIL:3206`): heavy with
  probability .3. Rain: `W = 6` rainy, `TR = .2`; heavy: `W = 8` very rainy,
  `TR = .8`. If `TM < 2` it falls as snow instead: `W = 7` snowy / `9` very
  snowy, `TS = 8*TR` (1.6 or 6.4 inches), `TR = 0`. Without precipitation
  `TR = TS = 0`.

### 12.5 Ground moisture and snow

- **REQ-106.** Daily: `AR = .9*AR + TR`; `AS = .97*AS + TS`. If `AS > 0` and
  (`TM > 2` or `W = 8`): melt, `AR += .5`, `AS -= 5` (floor 0)
  (`OREGON TRAIL:3240`).
- **REQ-107.** Initial values: `AR = 7 - month + RND` (wetter the earlier the
  start); `AS = 12*RND` if departing in March, else 0 (`OREGON TRAIL:29000–29004`).
- **REQ-108.** The trail-scene ground colour has shown the state: white if
  `AS ≥ 1`, else orange if `AR ≤ .2` (drought), else green
  (`OREGON TRAIL:310`).
- **REQ-109.** `AR` has driven river depth, width, and swiftness (§25.1) and the
  bad-water event (§17). `AS` has driven the snowbound event and snow slowdown.

---

## 13. Health

### 13.1 Scale

- **REQ-110.** Party health has been one score `H`, 0 best, clamped above at
  139 (`:3150`, `:3250`, `:4010`; every stress term is ≥ 0, so it never goes
  negative). Displayed as `H$(INT(H/35))`: 0–34 good, 35–69 fair, 70–104 poor,
  105–139 very poor.

### 13.2 Daily update

- **REQ-111.** Each day (`OREGON TRAIL:3207–3230`):

```
ZT = temperature stress = TM - 3, or 2 - TM if that is negative
     → TM 0,1,2,3,4,5 give 2,1,0,0,1,2
ZC = clothing stress = max(0, 5 - 2*TM - OP)          (OP = clothing / NP)
ZF = ration stress   = 2*(R - 1)  → 0 filling, 2 meager, 4 bare bones
     but 8 if there is no food (PF = 0)
ZP = pace/weather stress = 2*P + (W > 5) + (W > 7)    (P = 0 on rest days)
FS = FS + .8   if ZC > .5 or PF = 0                   (exposure / starvation)
     FS * .5   otherwise
H  = .9*H + ZT + ZC + ZF + ZP + FS + H0 + HR
```

  where `H0` is the number of afflicted members (§14.1) and `HR` the day's
  event penalty (§17).
- **REQ-112.** The 10% daily recovery (`.9*H`) has made `H` converge to
  ten times the day's stress. Steady pace, filling rations, mild weather, and
  enough clothing give a stress of 2 (pace alone), i.e. `H → 20` (good).
- **REQ-113.** Health has not reduced speed directly; only afflicted members
  have (§16.2).

---

## 14. Illness, injury, and death

### 14.1 Afflictions

- **REQ-120.** Afflictions have been indexed into `VAR.BIN:IL$`: 0 a broken
  arm, 1 a broken leg, 2 a snakebite, 3 exhaustion, 4 typhoid, 5 cholera,
  6 measles, 7 dysentery, 8 a fever.
- **REQ-121.** A member with `H1 > 0` has counted toward `H0`; each day `H2` is
  decremented and at `H2 < 1` the member recovers (`H1 = 0`)
  (`OREGON TRAIL:3200`). Durations: illness and snakebite 10 days, injury 30.
- **REQ-122.** Afflictions have been assigned by overwriting `H1`/`H2`
  (`:10100`, `:10830`), so a new injury or snakebite **replaces** whatever the
  member had and restarts the timer. Because "well" is `H1 = 0`, a **broken
  arm** leaves the member counted as well: it has no effect of its own and it
  **cures** any existing illness or injury (§36).

### 14.2 Picking a member

- **REQ-123.** Events that affect "someone" have used `OREGON TRAIL:11500`:
  `Z = INT(RND*NP)`, advance to `(Z+1) mod NP` skipping members with `H1 < 0`,
  then if `Z = 0` and `NP > 1`, use `Z = 1`. So the **leader is never picked
  while anyone else lives**, member 1 is twice as likely as the others, and the
  leader is always the last to die.

### 14.3 Illness and death roll

- **REQ-124.** The illness handler (`OREGON TRAIL:10300`) has picked a member
  (§14.2) and set `HR = 20` (effective only when it runs as random event 3,
  before the day's health update; when forced at step 11 the next day's
  `HR = 0` discards it):
  - if the member already has an affliction (`H1 ≠ 0`), they die ("*name*
    has died.");
  - otherwise they catch illness `3 + INT(RND*6)` (exhaustion … fever) for 10
    days ("*name* has *illness*.").
- **REQ-125.** It has run as random event 3 with probability `.01 + H/1500`
  (1% at perfect health, ≈10.3% at 139), and **every day** that `H` exceeds
  139 after the health update (step 11 of §10.2), except during river
  crossings.

### 14.4 Death

- **REQ-126.** On death (`TOMB.LIB:50000`): `H` has been capped at 105 (a death
  relieves the party), `NP` decremented, and the dead member swapped into the
  last slot so slots `0..NP-1` stay the living.
- **REQ-127.** If that leaves `NP = 0`, the game has shown a tombstone
  "Here lies *leader*", offered an epitaph (≤ 29 characters), saved it to the
  current side's tombstone file (§28), shown "All of the people in your party
  have died.", and returned to the main menu.

---

## 15. Food and rations

### 15.1 Consumption

- **REQ-130.** Each day, including rest days, the party has eaten
  `FC = NP * (4 - R)` pounds: **3 lb per person filling, 2 meager, 1 bare
  bones**. Food is floored at 0 (`OREGON TRAIL:3230`).
- **REQ-131.** At 0 food, ration stress has been 8 and exposure stress `FS`
  has accumulated (§13.2).

### 15.2 Changing rations

- **REQ-132.** `RATION.LIB` has set `R` to 1, 2, or 3 (Return keeps the current
  value) and recomputed `FC`, `F0`, `BS`, `OP` (`OREGON TRAIL:650–660`).
- **REQ-133.** Food has been capped at 2000 lb for purchase, hunting, and trade.

---

## 16. Pace and mileage

### 16.1 Base speed

- **REQ-140.** `OREGON TRAIL:650–660` has computed
  `BS = MD * min(oxen/4, 1) * (P + 1) / 2`: pace steady ×1, strenuous ×1.5,
  grueling ×2; fewer than 4 oxen reduce speed proportionally. `BS`, `FC`,
  `F0`, and `OP` (`OREGON TRAIL:650`) have been recomputed at each leg start
  and after changing pace or rations, an accepted trade, an ox injury, a theft,
  and a death. They have **not** been recomputed after a fire, an abandoned wagon, or a
  fort purchase; only the clothing figure `OP` can go stale that way (speed
  depends on oxen and pace alone), and a fort purchase is followed by a leg
  start anyway (§36).
- **REQ-141.** `PACE.LIB` has set `P` to 1–3 (Return keeps it); option 4
  explains the paces (8, 12, 16 hours a day).

### 16.2 Daily movement

- **REQ-142.** Each travel day (`OREGON TRAIL:3244–3246`):

```
snow factor  S = max(0, 1 - AS/40)
miles        V = BS * (1 - .1*H0) * S          (0 on rest days)
if 1.1*V > D: V = D                            (snap to the landmark)
D = D - V;  M = M + V
```

- **REQ-143.** Each afflicted member has cost 10% of speed; 40 inches of snow
  stops the wagon entirely.

### 16.3 Winter

- **REQ-144.** Nothing has forbidden travelling into winter. Cold months give
  low `TM`, which raises clothing and temperature stress, turns precipitation
  into snow, raises `AS` (slowing and eventually stopping the wagon), and above
  30 inches makes the snowbound event certain (§17).

---

## 17. Random events

### 17.1 Probabilities

- **REQ-150.** Event `k` has fired when `RND < RE(k)`, evaluated in index order
  once per travel day (never on rest days):

| k | Event | `RE(k)` | Set |
|---|---|---|---|
| 0 | Snowbound | `AS > 30` (certain) | daily |
| 1 | Snakebite | .007 | start |
| 2 | *(unused)* | 0 | start |
| 3 | Illness / death | `.01 + H/1500` | daily |
| 4 | Gravesite | `D < DL` (certain when passing a tombstone) | daily |
| 5 | Indians help find food | `.05` if food = 0, else 0 | daily |
| 6 | Severe weather | `(W > 7) + .15*(TM < 2)` | daily |
| 7 | Fog / hail | .06 | start |
| 8 | Wagon/oxen/injury mishap | `.04 + .03*(ZO > 2)` | per leg |
| 9 | Lost / wrong trail | .02 | start |
| 10 | Rough / impassable trail | `.05*(ZO > 2)` | per leg |
| 11 | Wild fruit | `.04` if month is May–Sept, else 0 | per leg |
| 12 | Fire / member lost / ox wanders | .01 | start |
| 13 | Abandoned wagon / thief | .02 | start |
| 14 | Water / grass trouble | `.5*(AR < .1)` | daily |

  "Start" values are set once in `OREGON TRAIL:29001`; "per leg" at the start
  of each leg (`:3060`, so the fruit month is the month the leg began);
  "daily" at step 4 of §10.2 (`:3160`).

### 17.2 Handlers

- **REQ-151.** "Lose *n* days" events have shown "*A$*. Lose *n* day(s)." with
  `n = 1 + INT(RND*Z)`, then run `n` rest days (§20.2) (`OREGON TRAIL:550–570`).
- **REQ-152.** Handler outcomes (`OREGON TRAIL:10000–11420`, `LF.LIB`):

| k | Outcome |
|---|---|
| 0 | "Snow bound", lose 1–10 days. |
| 1 | Only if `TM > 2`: a member (§14.2) gets a snakebite, 10 days. No `HR`. |
| 3 | Illness or death, `HR = 20` (§14.3). |
| 4 | "You pass a gravesite. Would you like to look closer?" Yes shows the tombstone (§28.2). |
| 5 | +30 lb food, "Indians help find food." |
| 6 | Nothing if `TM` is 2 or 3. Hot: "Severe thunderstorm", lose 1 day, `AR += 1`. Cold: "Severe blizzard", lose 1 day, `AS += 8` (`AR` unchanged: the code adds 1 then subtracts 1). |
| 7 | Past Fort Hall (`LM > 11`) and `TM < 5`: "Heavy fog", 50% message only, 50% lose 1 day. At or before Fort Hall and `TM = 5`: "Hail storm." (message only). Otherwise nothing. |
| 8 | Probability .33: broken wagon part (§18.1). Else ½: "One of the oxen is injured" / "has died" (§18.2). Else: a member breaks an arm or a leg (½ each), 30 days. |
| 9 | "Lose trail" or "Wrong trail" (½ each), lose 1–5 days. |
| 10 | ½ "Rough trail", `HR = 10`; ½ "Impassible trail" [sic], lose 1–10 days. |
| 11 | "Find wild fruit.", +20 lb food. |
| 12 | ⅓ each: fire; "*name* is lost", lose 1–5 days; "Ox wanders off", lose 1–3 days. |
| 13 | ½ abandoned wagon; ½ thief. |
| 14 | 20% "Bad water", `HR = 20`; else 50% "Very little water", `HR = 10`; else "Inadequate grass" (no effect). |

- **REQ-153.** **Fire** (`LF.LIB:50000`): for each of clothing, bullets,
  wheels, axles, tongues, with probability ½ lose `1..count` (uniform); then
  food likewise. Lists the losses under "A fire in the wagon results in loss
  of:"; if nothing was lost the fire passes with no message at all
  (`OREGON TRAIL:11210`).
- **REQ-154.** **Abandoned wagon** (`LF.LIB:51000`): for each of clothing,
  bullets, wheels, axles, tongues, with probability ½ find `1..3` (bullets ×21,
  so 21/42/63); spare parts only if the total stays ≤ 3. "…but it is empty." if
  nothing was found. It never yields food, oxen, or money.
- **REQ-155.** **Thief** (`LF.LIB:52000`): picks oxen, clothing, bullets, or
  food (¼ each) and steals `1..min(count, 100)`. The money branch is
  unreachable (§36).
- **REQ-156.** After a theft (and after an ox injury, §18.2) the game has run
  the oxen check (§18.3); fire and the abandoned wagon have not.

---

## 18. Wagon breakdowns and oxen

### 18.1 Broken parts

- **REQ-160.** A broken part is a wheel, axle, or tongue (⅓ each)
  (`OREGON TRAIL:10800`, `PART.LIB`). "Broken wagon *part*. Would you like to
  try to repair it?": Yes succeeds with probability ½. Otherwise a spare is used
  if held. Otherwise "Since you don't have a spare *part*, you must trade for
  one." and `B` stays set; the repair attempt costs one day either way.
- **REQ-161.** While `B` is a part, "Continue on trail" has been refused ("You
  must trade for a *part* to be able to continue."). After any menu action that
  leaves a spare of that part in stock, one has been consumed and `B` cleared
  (`OREGON TRAIL:4090`).

### 18.2 Oxen

- **REQ-162.** An ox injury has removed **half an ox** ("One of the oxen is
  injured."); if the count becomes a whole number the message is "…has died."
- **REQ-163.** Oxen have otherwise been lost only to theft (§17.2), river
  crossings (§25), and the raft (§26.4). Fire never burns oxen, and "Ox wanders
  off" costs days but removes no ox.

### 18.3 No oxen

- **REQ-164.** With zero oxen (`OREGON TRAIL:21000`): "You are unable to continue
  your journey. You have no oxen to pull the wagon." and `B = 2`; "Continue"
  is refused ("You must trade for an ox…") until oxen are obtained.

---

## 19. The action menu

- **REQ-170.** The menu (`OREGON TRAIL:4000–4095`) has shown the date, weather,
  health, pace, rations, and these choices:

| # | Choice | Available |
|---|---|---|
| 1 | Continue on trail | always |
| 2 | Check supplies | always |
| 3 | Look at map | always |
| 4 | Change pace | always |
| 5 | Change food rations | always |
| 6 | Stop to rest | always |
| 7 | Attempt to trade | always |
| 8 | Talk to people | at a landmark (`LL = 0`) |
| 9 | Buy supplies | at a landmark of type 1 |
| 8 | Hunt for food | on the trail (`LL = 1`) |

- **REQ-171.** On the trail the menu has been opened by pressing Return during
  travel (§32.2); at a landmark by answering Yes to "look around".
- **REQ-172.** "Check supplies" has listed oxen, clothing, bullets, wheels,
  axles, tongues, food, and money, each shown as `INT(x + .51)`, so 4.5 oxen
  display as 5 (`:4110`).
- **REQ-173.** "Look at map" has drawn the visited route and the current
  position on the leg, interpolated linearly between `VAR.BIN:MP%` coordinates.

---

## 20. Resting and lost days

### 20.1 Rest

- **REQ-180.** "Stop to rest" has asked "How many days would you like to
  rest?" (0–9) and run that many rest days (`OREGON TRAIL:4500–4505`).

### 20.2 Rest-day semantics

- **REQ-181.** A rest day has been a normal day (§10.2) with pace `P = 0` (no
  pace stress), no movement, no random events, and no keyboard polling. Food is
  eaten, weather evolves, afflictions count down, health updates, and the
  calendar advances.
- **REQ-182.** Every lost-day penalty (events, stuck in mud, wet supplies,
  camping or waiting for a ferry, hunting, trading) has been implemented as
  rest days, so lost days carry no pace stress and usually let the party
  recover; weather, clothing, ration and starvation stress still apply, and the
  forced illness roll can still fire.

---

## 21. Hunting

- **REQ-190.** Hunting has been available only on the trail and has cost one
  rest day (`OREGON TRAIL:4600`), even if the player backs out. (Trading
  likewise always costs a day, `:4900`.)
- **REQ-191.** The minigame has been the machine-language command
  `& HUNT, bullets, (LM>3 AND LM<13), (LM>6), (LM<7), 1, ZO, L, bullets`
  (handler `$8A9A`). It packs a 9-byte block at `$8AE9` (bullets as 16 bits,
  the five bytes, then 16-bit pounds written back by the hunt, `$E130`), switches in language-card bank 2, and calls the hunting game at
  `$E003` (entry `$E098`), which returns pounds shot in `L` and writes back the
  bullets left.
- **REQ-192.** Controls (`HUNT.LIB:50000`, sprite `OREGON.IMA` 1; key handler
  `$EC68`):
  - **Return** starts or stops walking. Stopping resets the step interval to
    8 ticks and the walk frame to 0; starting keeps the interval as it is
    (`$EC7F`).
  - **Arrows** (novice): left/up turn the aim one step counter-clockwise,
    right/down one step clockwise; turns are applied on the 3-tick aim poll.
  - **`; P O I K , . /`** (expert, `$ED1E`) point at E, NE, N, NW, W, SW, S, SE:
    the hunter turns there by the shorter way. Pressing the key of the
    direction already faced while walking **speeds up**: the step interval
    drops by 2 ticks, to a minimum of 4 (`$ED08`). The floor is tested after
    the subtraction as an unsigned byte, so an interval of 1 becomes 255
    (REQ-192l).
  - **Space** fires (REQ-192e); **Ctrl-P** pauses until any key.
  - **Esc twice** in a row (`$EC35`) abandons the hunt *and the game*: it plants
    a stub at `$0200` that runs BASIC `GOTO 31000`, which returns to the main
    menu ("Getting the Oregon Trail menu...").
  - All keys are read only on the 3-tick poll (`$EB5C` → `$EC21`). Expert keys
    queue a shortest-way rotation of one 45° step per poll (`$ECD3`); they do
    not snap.
- **REQ-192a.** Animals have been six types, each an 8-byte record at `$E068`
  and a 3-frame sprite in the hunt image set (frames `3t+1..3t+3`, §29.5).
  Pounds per kill = `base + random(0..range)` (`$EAD3`):

| Type | Sprite | Animal | Pounds (raw) | After halving (§21 REQ-193) | Available when |
|---|---|---|---|---|---|
| 0 | frames 1–3 | deer (antlered buck) | 100–140 | 50–70 | last landmark 4–12 (Chimney Rock … Snake River crossing) |
| 1 | frames 4–6 | bear | 200–400 | 100–200 | last landmark ≥ 7 (from South Pass on) |
| 2 | frames 7–9 | buffalo | 1700–2000 | 850–1000 | last landmark ≤ 6 (legs up to South Pass) |
| 3 | frames 10–12 | deer (no antlers) | 120–150 | 60–75 | always |
| 4 | frames 13–15 | rabbit | 2–10 | 1–5 | always |
| 5 | frames 16–18 | squirrel | 3–7 | 1–3 | always |

  Names have been checked against external sources: the designer reduced the
  hunt to six species for memory, and a player guide gives rabbit 1–5, deer
  50–80, bear 100–250, buffalo 800–1000 lb, which matches the halved yields
  (the rabbit's 1–5 exactly). Both deer-like types appear to the player as
  "deer". The squirrel is identified by its sprite and by elimination.

- **REQ-192b.** The allowed list has been types 3, 4, 5 plus 0, 1, 2 for each
  set region flag (`$E593`); a spawn draws uniformly from the list.
- **REQ-192c.** Two animal slots have run concurrently. An empty slot has
  counted down a timer; on expiry it has spawned with probability **0.8%**
  (below), else re-armed the timer to `random(0..6) OR 1` ticks: 1, 3 or 5 (probability 2/7
  each) or 7 (1/7) (`$E16C`; the initial timers use the same draw, `$E6C0`). After 4
  kills no new animals spawn (`$E153`), but an animal already on the field can
  still be shot, so up to 5 kills (and 5 carcasses) are possible. A spawn
  leaves the slot's timer at 0, so the next decrement wraps it to 255: a new
  animal waits **256 ticks** off the field before its first step.
- **REQ-192d.** The spawn test has drawn `r = random(0..999)` against a
  threshold of 4, halved to 2 when parameter 5 is non-zero (the game always
  passes 1). The comparison checks only the **low byte** of `r` (the high-byte
  compare against 0 always sets carry), so a spawn happens for
  `r mod 256 < 2`: 8 values of 1000. With threshold 4 it would be 16/1000.
- **REQ-192e.** The hunt has lasted **2500 main-loop ticks** (`$E6E4`), not
  wall-clock time; running out of bullets prevents firing but does not end it.
  Each shot costs **one bullet** and only one shot can be in flight
  (`$EC68`, `$E6F3`).
- **REQ-192f.** A shot has hit when its bounding box overlaps an animal's
  (`$E39D`, `$E4BB`). The test order is the hunter's own box, then the
  per-zone scenery objects (trees, rocks, chosen from `$E04A` by climate zone),
  then live animals, then carcasses; the hunter, scenery and carcasses all
  block shots. Defect: when a lit pixel stops the shot but no box contains it
  (possible when a shot fired west or south-west from the left edge starts
  at a negative x, whose column the divide-by-7 at `$EF1A` wraps to the far
  right), the index left in X by the last loop is treated as a hit animal:
  slot 1, or the last carcass's index. That slot is "killed" even if it is
  elsewhere or empty; an empty slot's type byte then indexes past the animal
  table, giving garbage pounds and carcass art. The reimplementation skips
  the empty-slot case.
- **REQ-192g.** Aiming has used eight directions, 0 = east counting
  counter-clockwise. Per direction (`$E006`–`$E03E`):

| Dir | 0 E | 1 NE | 2 N | 3 NW | 4 W | 5 SW | 6 S | 7 SE |
|---|---|---|---|---|---|---|---|---|
| bullet dx per step | 2 | 2 | 0 | −2 | −2 | −2 | 0 | 2 |
| bullet dy per step | 0 | −1 | −1 | −1 | 0 | 1 | 1 | 1 |
| muzzle x offset | 21 | 19 | 13 | 0 | −2 | −2 | 7 | 21 |
| muzzle y offset | 10 | 0 | 0 | 0 | 10 | 15 | 19 | 15 |

  The bullet has advanced **twice the step per tick** (so 4 pixels
  horizontally and 2 rows vertically). At each position it samples the two
  pixels (x, y) and (x, y+1) (`$E9C6`, `$E9D6`), so thin features can be
  skipped; on a lit pixel the hit test of REQ-192f runs, and at the field edge
  the shot simply ends (`$EA29`). The aim
  keys are polled every 3 ticks (`$EB5C`); walking advances on its own timer
  with a 4-frame hunter animation (`$EB7C`). The hunter's pose image per
  direction is at `$E03E` (bit 7 = mirrored).
- **REQ-192i.** Hunter walking (`$EB7C`): every step interval the hunter moves
  one direction step (the bullet table's dx/dy, not doubled: 2 pixels and/or 1
  row) and advances a walk cycle of frames 0, 1, 0, 2 of the pose (`$E046`). A
  step that would leave the field or touch scenery, an animal, or a carcass
  stops walking (the interval resets to 8).
- **REQ-192j.** Image sets (`$E537`), selected by drawing ID:

| IDs | Set | Contents |
|---|---|---|
| `$01–$0F` | `$F780` | hunter: 5 aiming poses (up, up-left, left, down-left, down; drawn facing west) × 3 walk frames; east-facing directions mirror them |
| `$10–$21` | `$D000`, image ID − `$0F` | the six animals × 3 frames |
| `$22–$27` | `$D430`, image ID − `$21` | carcasses (upside-down animals), `$22 + type` |
| `$28+` | `$D430`, image ID − `$21` | scenery: deciduous trees 7–9, pines 10–12, grass/brush 13–15, cacti 16–18, rocks 19–21 |

- **REQ-192k.** Scenery (`$E2A7`): 4 + random(0..3) objects. Each kind is
  drawn from a 6-entry list per climate zone at `$E04A`:

| Zone | Stretch | Objects |
|---|---|---|
| 0 | to the Big Blue | deciduous trees, grass |
| 1 | to Fort Laramie | grass only |
| 2 | to Soda Springs | pines, rocks |
| 3 | to Fort Boise | cacti, rocks |
| 4 | beyond | pines only |

  Positions are random (x even, the whole object on screen); a spot that
  overlaps the hunter or an earlier object is re-rolled.
- **REQ-192h.** Animal motion (`$E150` spawn, `$E7A0` move):
  - **Pace.** Each animal steps once every *k* ticks, `k = rec[0] +
    random(0..rec[1])` drawn at spawn: 3–7 ticks for deer, rabbit, and
    squirrel, 3–11 for bear, 11–15 for buffalo (fewer ticks = faster).
  - **Entry.** Side chosen 50/50: from the left edge (x = −width, dx = +4
    pixels per step) or the right edge (x = 280, dx = −4, sprite mirrored).
    Vertical drift `dy = random(0..4) − 2` rows per step. The top row was meant
    to be `random(0 .. 191 − height)`, but `$E239` reads the record byte
    after an `INY` left over from the drift's sign extension: the row is
    `random(0 .. 191 − rec[1])` (the pace range) when dy ≥ 0 and
    `random(0 .. 191 − rec[0])` (the pace base) when dy < 0, so tall animals
    can start partly below the field. A spawn that would overlap an obstacle
    is re-rolled (side, drift, row; not the type or pace).
  - **Animation.** A 3-frame cycle, advancing one frame per step.
  - **Obstacles** (`$E382`/`$E39D`, tested in this order): the hunter, the
    zone's scenery objects, the other live animal, and the carcasses of
    animals already shot (their boxes are kept; up to 5). Bullets use the same
    test, so scenery and carcasses both stop shots.
  - **Blocked move.** Up to three tries per step: after the first and second
    collision the animal steers away, adding the smaller overlap with the
    obstacle to dx (clamped to ±4, flipping the sprite if the sign changes)
    and to dy (clamped at +7 above; the lower clamp is inverted, `$E92D`: a
    dy of −6..−1 is forced to −7 and a dy of −8 or less is left alone). On the
    third it reverses (`$E81D`): dx becomes the negation of the dx saved at the
    start of the step, facing is set from the new dx's high byte, a new `dy =
    random(0..4) − 2` is drawn, and the animal is redrawn in place without
    moving. Defect: the negation inverts the high byte without carrying, so an
    old dx of 0 becomes −256 instead of ±4, and the animal leaps 256 pixels
    (usually off the field) on its next step.
  - **Exit.** When the animal's box has moved wholly outside the 280×192
    field, the slot is freed and resumes spawn attempts (`$EDC8`).
  - **Kill.** A shot animal is drawn upside down in place, its box joins the
    carcass list, and the slot re-arms with a timer of `1 + 2·random(0..1)`
    ticks (`$EB1B`).
- **REQ-192l.** Machine details (`src/hunt.c` reproduces them; verified pixel
  for pixel against the original in `tests/test_hunt.c`):
  - **Random numbers** (`$F508`–`$F5DA`): a 48-bit linear congruential
    generator, `S = S × (2^27 + 3) + 1` (big-endian at `$F5CF`). `random(n)`
    is `(S[1]·256 + S[0]) mod n`, its top two bytes taken low byte first. Each
    hunt seeds it (`$E5BB`) from `($4E xor $4F) or 1` (the keyboard wait
    counter) and `$C9`–`$CD` (Applesoft's RND seed) xor `$4E`.
  - **Drawing**: sprites are opaque rectangles of raw row-order bytes, shifted
    through tables the hunt builds at `$3A00` (`$F15A`); moving sprites erase
    the strip they uncover to black (`$ED26`, `$EF82`); the bullet is a 2×2
    exclusive-or. Mirroring (`$E4FB`) reverses the image data **in place**
    and records the orientation per image at `$EE9D`.
  - **Silence**: the hunt never touches the speaker.
  - **Time**: each tick an animal slot that is not due spends a fixed delay
    (`$E0EF`, about 3,850 cycles), so a hunt lasts about 25 million cycles,
    roughly 25 seconds.
  - **Memory between hunts**: the language card is loaded once at boot and
    the hunt re-initialises only part of its state, so the mirrored sprites,
    the walk interval (1 after boot, until the first stop), a pending turn,
    and each slot's animation frame carry over into the next hunt.
- **REQ-193.** Post-processing (`HUNT.LIB:50011–50020`): if `L ≥ 3`,
  `L = INT(L/2)`. If the wagon already holds 2000 lb: "However, your wagon is
  full." and `L = 0`. Otherwise `L` is limited to the free space, then to
  **100 lb** ("However, you were only able to carry 100 pounds back to the
  wagon."). `L = 0`: "You were unable to shoot any food." Messages: "From the animals you shot, you got *L* pound(s) of meat." and, when the
  wagon has 100 lb or less of room, "However, your wagon will only hold another
  *n* pounds of food." (`HUNT.LIB:50012–50014`).

---

## 22. Trading

- **REQ-200.** Trading has cost one rest day whatever the outcome
  (`OREGON TRAIL:4900`).
- **REQ-201.** The offer (`TRADE.LIB:50010–50035`): pick `X` (wanted from you)
  uniformly and `Y` (offered) with `Y = INT(RND*7)`, bumped to the next item
  (mod 7) if equal to `X`, so `Y ≠ X` and item `(X+1) mod 7` is twice as likely
  as each other. Both come from the seven store items
  (oxen, clothing, ammunition, wheel, axle, tongue, food). With per-unit values
  `v` from `VAR.BIN:S$(*,1)` (ammunition per bullet, $0.10):

```
Z = v(X) / (v(Y) * (1 + 1.2*RND))
if Z ≥ 1: give 1 of X, get INT(Z) of Y
else:     give INT(1/Z) of X, get 1 of Y
if {X,Y} = {bullets, food}: multiply both quantities by 50
```

  so trades have always been worth between 1× and 2.2× less than store value
  to the player.
- **REQ-202.** "No one wants to trade with you today." with probability .05,
  or if accepting would exceed a cap (spare parts 3, food 2000, oxen 20).
- **REQ-203.** "You meet another emigrant who wants *n item*." If the player
  lacks the quantity: "You don't have this." Otherwise "He/She will trade you
  *m item*. Are you willing to trade?" (She with probability .33).

---

## 23. Fort stores

- **REQ-210.** Stores have existed at landmarks of type 1: Independence,
  Fort Kearney, Fort Laramie, Fort Bridger, Fort Hall, Fort Boise, Fort Walla
  Walla (menu gate `OREGON TRAIL:4040`; store `BUY.LIB`).
- **REQ-211.** Price = base × (1 + .25·Q), where
  `Q = (LM>2)+(LM>4)+(LM>7)+(LM>10)+(LM>12)+(LM>13)`: Independence ×1.00,
  Kearney ×1.25, Laramie ×1.50, Bridger ×1.75, Hall ×2.00, Boise ×2.25,
  Walla Walla ×2.50. Base prices `VAR.BIN:S$(*,1)`: ox $20, clothing $10,
  ammunition $2 per box of 20, wheel/axle/tongue $10, food $0.20/lb.
- **REQ-212.** Purchases add to stock, one item at a time. Refusals: "You
  cannot afford that." (checked for one unit before the quantity prompt) and
  "You cannot afford that many." (after it), "You may only take 20 oxen.", "Your wagon may only carry
  2000 pounds of food.", "Your wagon may only carry 3 *parts*."
- **REQ-213.** Oxen here have been sold singly (unlike Matt's yokes).

---

## 24. Talking to people

- **REQ-220.** Talk has been available only while looking around a landmark. It
  has shown one of three fixed speakers per landmark, chosen by `A` (§5.2).
  `A` advances (mod 3) every time the action menu is drawn (`OREGON
  TRAIL:4030`), so talking repeatedly cycles through all three speakers
  (`TALK.LIB:50000`).
- **REQ-221.** Records live in `OREGON1.SEQ` (landmarks 0–4) and `OREGON2.SEQ`
  (5–17), 768 bytes per landmark, 256 per speaker: speaker line, CR, quote, CR.
  Shown as "*speaker* tells you: "*quote*"". See §29.3.

---

## 25. River crossings

### 25.1 River conditions

- **REQ-230.** Each river has a record in `VAR.BIN:RC(r,*)` (Appendix A.5).
  Conditions have been derived from moisture `AR` whenever shown
  (`RIVER.LIB:50150`):

```
depth RD = round1(RC(r,0) + 2*AR)          feet, one decimal
width RW = INT(RC(r,1) + 15*AR)            feet
swift RS = RC(r,2) + AR
bottom RB = RC(r,3)                        0 none, 1 muddy, 2 rough
extra option RC(r,5): 0 none, 1 Indian guide, 2 ferry
```

- **REQ-231.** The screen: "You must cross the river in order to continue. The
  river at this point is currently *RW* feet across, and *RD* feet deep in the
  middle." Then weather, width, depth, and the options: 1 attempt to ford,
  2 caulk wagon and float it across, [3 take a ferry across / hire an Indian to
  help], then wait to see if conditions improve, get more information.
- **REQ-232.** "Wait" has run one rest day ("You camp near the river for a
  day.") and redisplayed the conditions. "Get more information" has explained
  fording, caulking, and ferries.

### 25.2 Loss primitives

- **REQ-233.** Crossing outcomes have been composed of three loss rolls
  (`RIVER.LIB:50175–50210`), each listing its losses:
  - **items(p):** for each of clothing, bullets, wheels, axles, tongues, food
    held, with probability `p` lose `1..count`;
  - **oxen(p):** each ox independently lost with probability `p`;
  - **drown(p):** each member except the leader (unless the leader is alone)
    independently drowns with probability `p`; drownings are processed as
    deaths after the crossing (`OREGON TRAIL:3504`).

### 25.3 Ford

- **REQ-234.** Ford (`RIVER.LIB:50035–50075`), with guide divisor `IX = 1`
  (5 with a guide):
  - `RD < 2.5`: bottom 0 → success; muddy → "You become stuck in the mud.
    Lose 1 day." with probability `.4/IX`, else "It was a muddy crossing, but
    you did not get stuck."; rough → "The wagon tipped over" with probability
    `.16/IX`, then items(`.1 + .3*RND`), else "It was a rough crossing, but you
    did not overturn."
  - `2.5 ≤ RD < 3`: "Your supplies got wet. Lose 1 day."
  - `RD ≥ 3`: items(`RD/10/IX`), oxen(`(RD-1)/10/IX`),
    drown(`(RD-2.5)/10/IX`); if anything was lost, "The river is too deep to
    ford. You lose:".

### 25.4 Caulk and float

- **REQ-234a.** Details: a tipped rough ford that loses nothing reads "The
  wagon tipped over but you did not lose anything."; the rough-ford loss
  fraction `.1 + .3*RND` is not divided by `IX`.
- **REQ-235.** Float (`RIVER.LIB:50080–50090`): refused if `RD < 1.5` ("The
  river is too shallow to float across."). Otherwise costs one day. Tips over
  with probability `(RD > 2.5) * (RS/20) / IX`; on tipping: items(`(.4 +
  RS/25)/IX`), drown(`((RS - 3)/15)/IX`), "The wagon tipped over while
  floating. You lose:". Oxen are never lost floating.

### 25.5 Ferry (Kansas, Green)

- **REQ-236.** Ferry (`RIVER.LIB:50100–50125`): refused if `RD < 2.5` ("The ferry
  is not operating today because the river is to shallow." [sic]). Costs **$5**
  and a wait of 2–6 rest days, both accepted up front; refused without the
  money. Accident probability `.05*(RS > 5) + .1*(RS > 10)`: items(.8),
  drown(.2), oxen(.5), "The ferry broke loose from moorings. You lose:" (or
  "Some trouble in crossing but nothing was lost.").

### 25.6 Indian guide (Snake)

- **REQ-237.** "A Shoshoni guide says that he will take your wagon across the
  river in exchange for *n* sets of clothing." with `n = 2..3`
  (`RIVER.LIB:50130–50145`). On acceptance the guide fords (`RD ≤ 2.4`) or
  floats (`RD > 2.4`) with `IX = 5`, dividing the listed risks by 5. Without
  enough clothing: "You don't have *n* sets of clothing." and back to the
  choices. Notes: a guided float with `2.4 < RD ≤ 2.5` cannot tip (the float
  risk needs `RD > 2.5`); the guided-ford branch is effectively unreachable,
  since the Snake's depth is `6 + 2·AR` (below 2.4 only if `AR < −1.8`).

### 25.7 Presentation

- **REQ-238.** `CROSS.LIB` has animated the crossing (wagon image, water lines);
  then the outcome message and loss list, then tombstone handling for the
  drowned (`OREGON TRAIL:3504`). Each line of the water (`CROSS.LIB:50010`, 53
  lines), of the far bank (50030, 61 lines, no losses) or of a ford gone wrong
  (50060, 22 lines) has called 50070 or 50080, which pick a splash point with
  two `RND` draws and never plot it (50100 is never called); 50060 first draws
  which of the two to call. These draws have still advanced the shared `RND`
  stream, so every later random outcome depends on them. The heading has
  printed `TD$` as `OREGON TRAIL:250` last built it (RIVER.LIB:50000, or the
  camp at 50025), so a day lost floating (`GOSUB 500` at 50085) or fording
  has not shown yet: the date is the day the party reached the river.

---

## 26. The Dalles: Columbia River or Barlow Road

### 26.1 Choice

- **REQ-240.** At The Dalles (`END.LIB:50010`): "The trail divides here. You
  may: 1. float down the Columbia River 2. take the Barlow Toll Road".

### 26.2 Barlow Road

- **REQ-241.** Toll `= $5 + $0.50 × round(oxen)`. "You must pay $*x* to travel
  the Barlow road. Are you willing to do this?" Yes with enough cash pays and
  continues the final 100-mile leg to the Willamette Valley at 12 miles/day
  base; with too little cash: "You do not have enough cash." and back to the
  choice. The comparison is strict (§36).

### 26.3 Columbia River hand-off

- **REQ-242.** Choosing to float has saved state to the handoff area (§30) and
  run `FLOAT`.

### 26.4 The raft game (`FLOAT`)

- **REQ-243.** Instructions: "Use the arrow keys to guide your raft through the
  rushing waters of the Columbia River." and "After passing the third
  direction sign, land your raft at the trail to the Willamette Valley."
- **REQ-244.** The raft occupies lane `HP` 0..17 across the river, drawn at
  `(84 + 8*HP, 5*HP - 6)`, starting at 16 with drift `DIR = -1`. Each tick:
  left/up arrow `DIR -= 1` (only while `HP > 0`), right/down arrow
  `DIR += 1` (only while `HP < 17`), clamped −1..1; then, if `DIR ≠ 0`,
  `HP += DIR` and the shore test runs (`FLOAT:1090–1110`). Collision boxes:
  raft `(X+6, Y+18)–(X+26, Y+25)`, rock `(RX+11, RY+1)–(RX+31, RY+7)`. Keys: codes 136/139 (left/up), 149/138 (right/down).
- **REQ-245.** Up to two rocks exist. Each absent rock spawns with probability
  .15 per tick: 13% at the bottom (`y = 175`, `x = 0..9`), else on the left
  edge (`x = 0`, `y = 51..170`). Rocks move `(+8, -4)` per tick and vanish
  past `x ≥ 240` or `y ≤ 10`.
- **REQ-246.** Direction signs have scrolled past at ticks 60, 120, 170; the
  landing marker at tick 175.
- **REQ-247.** Collisions (`FLOAT:200–770`): lane < 1 or > 16 is the shore:
  reverse `DIR`, then drown(.15), oxen(.3), items(.5), "The raft has hit the
  shore." A rock (box overlap): drown(.6), oxen(.6), items(.7), "The raft has
  hit a rock.", rock removed. If more than 9 loss lines result: "The raft is
  destroyed, everything has been lost." and the party is dead.
- **REQ-247a.** Defect: the shore/rock message and losses depend on `ROCK`,
  which is reset to −1 only when a rock test runs and misses (`FLOAT:200`).
  After a rock hit, a later shore hit with no rock test in between (tests are
  skipped on shore-hit ticks and when no rock is alive) is reported as "The
  raft has hit a rock." with rock losses (.6/.6/.7).
- **REQ-247b.** Defect: removing drowned members (`FLOAT:750`) moves names
  (`N$(L) = N$(NP)`) but not `H1`, and does not re-test the moved slot, so a
  drowned member's name can replace a survivor's in later loss messages; the
  count `NP` stays correct.
- **REQ-248.** Landing: after tick 205, reaching lane 17 has succeeded. After
  tick 225 without landing: items(.5), with "The raft has missed the
  landing. You have lost:" shown only if something was lost; then a forced
  landing. Both proceed to `WIN`. The raft trip takes **no in-game time**:
  `FLOAT` never advances the date, so the arrival date and health band are
  those recorded at The Dalles.
- **REQ-249.** If every member drowns: "Everyone in the party has died." and
  return to the menu, with no tombstone.

---

## 27. Arrival and scoring

### 27.1 Arrival

- **REQ-250.** `WIN` has shown the Willamette Valley picture (`L17.PCK`), the
  arrival date, "Congratulations! You have made it to Oregon! Let's see how many
  points you have received.", and the points table.

### 27.2 Points

- **REQ-251.** Points (`WIN:130–140`), health band `h = INT(H/35)` at departure
  from The Dalles or arrival:

| Item | Points |
|---|---|
| each surviving member | 500 good, 400 fair, 300 poor, 200 very poor (`500 - 100h`) |
| wagon | 50 |
| each ox | 4 |
| each spare part | 2 |
| each set of clothing | 2 |
| bullets | 1 per 50 (floor) |
| food | 1 per 25 lb (floor) |
| cash | 1 per $5 (floor) |

- **REQ-252.** The total has then been multiplied by the occupation multiplier
  (§7.1): "For going as a carpenter, your points are doubled." / "farmer …
  tripled."

### 27.3 Rating

- **REQ-253.** Rating: **Trail guide** ≥ 6000, **Adventurer** 3000–5999,
  **Greenhorn** < 3000 (`WIN:630`).

### 27.4 Oregon Top Ten

- **REQ-254.** A score strictly greater than the 10th entry qualifies. The
  player enters a name (≤ 15 characters); the entry is inserted below any equal
  scores and the list saved to `HISCORE.SEQ` on side 1 (flip prompt if needed)
  (`WIN:500–540`).
- **REQ-255.** The original list (`MANAGEMENT:20000`): Stephen Meek 7650 Trail
  guide; David Hastings 5694, Andrew Sublette 4138 Adventurer; Celinda Hines
  2945, Ezra Meeker 2052, William Vaughn 1401, Mary Bartlett 937, William
  Wiggins 615, Charles Hopper 396, Elijah White 250 Greenhorn.
- **REQ-256.** The menu's Top Ten screen has offered "how points are earned",
  showing the tables of REQ-251/252 (`MENU:2200–2230`).

---

## 28. Tombstones

### 28.1 Writing

- **REQ-260.** When the whole party dies (§14.4) the tombstone has been
  written to the current side's `TOMB.SEQ` at record 0: segment id
  `NM*100 + LM`, miles remaining `D`, name, epitaph (`TOMB.LIB:50035`). Each
  side therefore holds the latest death on its half of the trail.

### 28.2 Reading

- **REQ-261.** At game start and after the side switch, records 0 and 1
  (49-byte slots) have been read into `SN()`, `ML()` (`OREGON TRAIL:29004`,
  `FLIP.LIB:50030`).
- **REQ-262.** On each leg the game has selected the record in this segment
  with the largest miles-remaining below the current `D` (`OREGON TRAIL:450`).
  The gravesite event fires once `D` drops below it; afterwards the next one is
  selected. "Here lies *name*" with the epitaph.
- **REQ-262a.** Tombstones are processed after a river crossing only if the
  crossing listed losses (`OREGON TRAIL:3504`); drowning stops once 11 loss
  lines exist (`RIVER.LIB:50175`).
- **REQ-263.** Management option "Erase the tombstone messages" has written
  `"0" CR "0" CR` to record 0 on both sides (§34).

---

# Part III, Files, UI, platform

## 29. Disk layout and file formats

### 29.1 Catalog

- **REQ-270.** Files, by role:

| Files | Side | Type | Content |
|---|---|---|---|
| `HELLO`, `MENU`, `MANAGEMENT`, `BUY SUPPLIES`, `OREGON TRAIL` | A | BASIC | programs |
| `FLOAT`, `WIN`, `MENU` (side 2), `HELLO` (side 2), `TEST FLOAT` | B | BASIC | programs |
| `*.LIB` | A/B | BASIC | overlays (§2.3) |
| `VAR.BIN` | A | binary @ $9902 | preloaded variable table (§29.2) |
| `PT.BIN` | A | binary @ $0069 | Applesoft pointers VARTAB, ARYTAB, STREND, FRETOP, FRESPC, MEMSIZ |
| `OREGON1.SEQ`, `OREGON2.SEQ` | A, B | text | Talk dialogue |
| `HISCORE.SEQ` | A | text | Top Ten |
| `TOMB.SEQ` | A, B | text | tombstones |
| `L0..L17.PCK`, `M0.PCK`, `TS.PCK`, `RIVER.PCK` | A/B | packed hi-res | landmark pictures, map, tombstone, river backdrop |
| `*.IMA` (`OREGON`, `PRAIRIE`, `FIRST`, `EVENTS`, `RIVER`, `ORSPRITE`, `MOUNTAINS`) | A/B | image sets | sprites/icons drawn by `& IMAGE` |
| `MS0..MS17.BIN` | A/B | music (§29.6) | one tune per landmark (`MS0` departure, `MS16` float, `MS17` win) |
| `TS.BIN` | A/B | music (§29.6) | tombstone tune |
| `DALLESUTIL.OBJ` | B | machine code @ $3000 | raft-game sprite extension (§29.7) |
| `MECC$$DISK` | A/B | binary | vestigial disk-identity record (§29.8) |

### 29.2 `VAR.BIN`

- **REQ-271.** `VAR.BIN` has been a raw memory image from VARTAB to MEMSIZ:
  7-byte simple variables, then arrays (Applesoft layout, first subscript
  varying fastest), then the string pool, so every string descriptor resolves
  inside the file. `PT.BIN` restores the matching zero-page pointers. Loading
  both (`OREGON TRAIL:100`) has recreated every table without running any
  initialisation code. Appendix A reproduces it.

### 29.3 Text databases

- **REQ-272.** `.SEQ` files have been DOS random-access text: CR-terminated
  fields in fixed-size records, with high bits clear. Unused bytes after the
  last field of a record have been leftovers and must be ignored.
  - `OREGON{1,2}.SEQ`: 256-byte records at `(LM - 5*S)*768 + speaker*256`:
    speaker, quote (a quote longer than 255 characters continues in a third
    field).
  - `HISCORE.SEQ`: 30 fields, name / points / rating × 10.
  - `TOMB.SEQ`: 49-byte records: segment id, miles remaining, name, epitaph.

### 29.4 Graphics

- **REQ-273.** The screen has been Apple II hi-res page 2 (`$4000`), 280×192
  (40 bytes × 192 rows, 7 pixels per byte, bit 7 selecting the colour group),
  with buffers at $3000 (`PA`, dialog save-under), $3A00 (shift tables), and
  $9204 (`SA`, sprite sets) (REQ-018b).

### 29.5 Picture and sprite formats

- **REQ-274.** One run-length code has served both formats (`& DUN` at
  `$87D4`, `& IMAGE` blitter at `$8511`):

| Byte | Meaning |
|---|---|
| `00nnnnnn` | copy the next n+1 bytes |
| `01nnnnnn v` | write v, n+1 times |
| `10nnnnnn a b` | write the pair a,b, n+1 times |
| `11xxxxxx` | end (see below) |

  Output fills **columns**: all rows of one byte-column, top to bottom, then
  the next column. The two decoders differ on the end code: `& DUN` has
  stopped at any byte `$C0`–`$FF` (`$8879`), while the image blitter has
  stopped only at exactly `$C0` and has treated `$C1`–`$FF` as pair runs
  (`$8577`). Every shipped stream ends in `$C0`, so the difference is latent.
- **REQ-275.** `.PCK` (full-screen picture): the RLE stream for a 40×192 screen.
  `& DUN` streams it from disk through a buffer (`AT addr, pages`).
- **REQ-276.** `.IMA` (sprite set): byte 1 is the offset of an index of 2-byte
  little-endian entries. Entry 0 holds the slot count (true of every set on
  the disks, though the lookup `$7B38` never reads it); entry *n* is the offset of
  image *n* in the low 14 bits, and bit 15 selects the format. An image record is
  width (byte-columns) and height (rows), then either the RLE stream in column
  order (bit 15 clear, blitter `$8511`) or raw bytes in **row** order (bit 15
  set, blitter `$80D5`, used by `ORSPRITE.IMA` and the hunt sprites). Empty
  slots have offset 0; images are numbered 1 to the count. `& DFT n,addr`
  registers a set in slot *n* of the table at `$0310` (slot 0 is the font,
  slot 7 the image set `& IMAGE` and `& UIM` read, slot 15 the tune); `& IMAGE
  n,x,y` draws image `n` at pixel `x`, row `y` of the page at `$E6`.
- **REQ-276a.** The blitters have **replaced** the pixels an image covers
  (dark pixels included) and have kept the rest of each screen byte: at a
  shift *s* = `x mod 7`, each image byte's seven pixels land in bits *s*–6 of
  one screen byte and bits 0–(*s*−1) of the next, through the `& CST` shift
  tables (`$7E5E` for RLE images, `$80D5` for raw ones). Every screen byte an
  image touches takes the image byte's high bit (palette bit); where two image
  bytes share a screen byte, the later one's wins. `x` may be negative
  (rounded down to a byte column, `$8475`); byte columns past 39 are skipped.
  The row counter is 8 bits: rows 192–255 are skipped and an image that runs
  past row 255 wraps to row 0. Raw images keep the pixels left of `x` in their
  first byte (`$8132`).
- **REQ-277.** Both formats are drawn by `src/legacy/gfx.c`, and shown with an
  NTSC-artifact palette (black, white, violet, green, blue, orange).
  Appendix C lists the artwork.

### 29.6 Music

- **REQ-278.** A tune file (`& PT` at `$8AF2`, lookup `$8B58`) has held a tune
  count (byte 0), then 16-bit offsets (tune *n* at bytes 2n–2n+1), then per
  tune a list of `(pitch, duration)` byte pairs ended by a byte with bit 7 set.
  Every shipped file holds one tune. `& PT,1,1` plays the whole tune (a key
  press stops it); `& PN` plays the next single note.
- **REQ-279.** The tone generator (`$7F2B`) has toggled the speaker with
  half-periods from a table at `$7FC8` (51 entries, pitches 0–`$32`; two counts
  t0, t1 per pitch, skewed for duty cycle as h1 = t0 − (t0 >> 5),
  h2 = t1 + (t0 >> 5), which keeps h1 + h2 = t0 + t1).
  Counted from the code, each loop pass is 23 cycles (`INC` 5, `BNE` 3, `NOP`
  2, `JMP` 3, `DEY` 2, `BEQ` 2, `JMP` 3, `BNE` 3), plus 2 cycles once every 256
  passes when the duration counter carries; each half adds 2 cycles of
  overhead. So one period is `(23 + 2/256)·(h1 + h2) + 4` cycles.
- **REQ-279f.** The Apple II CPU has run at 14.31818 MHz ÷ 14 (1.0227 MHz)
  with one cycle in 65 stretched by two master ticks, averaging
  `14,318,180 × 65 / 912 = 1,020,484` cycles per second. Hence
  `f = 1,020,484 / ((23 + 2/256)·(h1 + h2) + 4)` Hz: pitch `$1C` = 429.9 Hz,
  pitch `$13` = 254.7 Hz. Every pitch used by the tunes (`$11`–`$24`) sits about
  36–49 cents (mean 44) flat of equal temperament at A440: **MIDI note =
  pitch + 41, ~44 cents flat**, i.e. the table is tuned to A4 ≈ 430 Hz. Unused
  entries above `$26` drift (23–68 cents flat). Pitch 0 is a rest.
- **REQ-279g.** A note has lasted `duration × 256` passes = **5.772 ms per
  unit**; the common values are 45 (≈ an eighth note) and 90 (a quarter), with
  others including 15, 20, 22, 30, 40, 60, 67, 80, 112, 120, 135, 150, 160,
  180, 225 and 240.
  `MS0`, the departure tune, is "Yankee Doodle".
- **REQ-279a.** `build/music_wav OUTDIR` renders every tune to WAV, as the
  speaker played it and as modern mode's orchestra plays it.

### 29.7 Raft-game sprite extension (`DALLESUTIL.OBJ`)

- **REQ-279b.** `FLOAT` has loaded a second ampersand extension at `$3000`
  (`& RNH,"DALLESUTIL.OBJ"`), installed in front of the main one with the same
  dispatcher design (names at `$3BCF`, addresses at `$3BE8`, handler =
  address + 1). Its six commands:

| Command | Handler | Meaning |
|---|---|---|
| `CST` | `$3AD8` | build shift tables (as the main `CST`) |
| `IMAGE` | `$3AEF` | local copy of the image blitter |
| `DPB` | `$3B55` | `DPB n,addr`: give sprite slot *n* a background-save buffer |
| `KILP` | `$3B72` | remove sprite *n* |
| `PUTP` | `$3B7F` | `PUTP slot,image AT x,y`: restore the old background, save the new, draw |
| `RMOVP` | `$3BBC` | restore a sprite's saved background |

- **REQ-279c.** `PUTP` has drawn with **two consecutive images**, *n* as the
  mask and *n+1* as the picture (`$37FA`): raft = `ORSPRITE` 1+2, rock = 3+4.
  Images 5 (direction sign) and 6 (landing trail) are drawn with plain
  `& IMAGE`.

### 29.8 `MECC$$DISK`

- **REQ-279d.** `MECC$$DISK` has been a 40-byte identity record: byte 0 = 0,
  bytes 1–12 = the product name "Oregon Trail", byte 35 = side number (1 on
  side A, 2 on side B), byte 36 = `01` (meaning unknown), bytes 37–39 =
  `06 01 FE`: slot 6, drive 1, volume 254, the standard boot device (the
  same fields as the game's disk requests, REQ-331a).
- **REQ-279e.** It has been a standard MECC file, not specific to this game:
  the Internet Archive's copy of *Odell Lake* v1.2 (MECC, 1986) shows a catalog
  of only `HELLO` and `MECC$$DISK` on an otherwise hidden, full disk.
- **REQ-279h.** No shipped code in this game has read it. The only
  references are two copies of an older line of MECC's shared BASIC framework,
  `40010 ZS = 704: & SET,1: & RFL,"MECC$$DISK" AT 704: GOSUB 33200: QN$ = ZN$:
  RETURN`, left in slack space (past the end of side B's `MENU`, and in a free
  sector): the framework loaded the record at 704 (`$02C0`) and took the
  product name `QN$` from it. The shipped programs hard-code `QN$ = "OREGON
  TRAIL"` (`COMMON.LIB:40005`) and identify disks by the catalog label
  (REQ-331).

### 29.9 Text font

- **REQ-279i.** Text has been drawn from a **proportional font** built into the
  extension at `$6BEB` (the default pointer set by `& HOOK`, `$60CD`). Layout:
  byte 0 = the input cursor character (`$7F`), byte 1 = first character
  (`$20`), byte 2 = last (`$7F`), bytes 3–7 the font's metrics (cell width 6,
  line height 8, horizontal spacing 1, vertical spacing 1, top spacing 0;
  `& HSP`, `& VSP` and `& TSP` rewrite bytes 5–7 in place), then one 5-byte
  record per character: width in pixels, flags (bits 0–4 the top offset in rows,
  bits 5–7 the extra bytes per bitmap row), height in rows (0 for the space,
  which is never drawn), and a 16-bit offset of the bitmap from `$6BEB`. Bitmaps are one byte per row (bit 0 leftmost); the two
  glyphs wider than 7 pixels (`$`, `*`) use two bytes per row, the second
  holding pixels 0–6 and the first pixel 7. Widths run from 2 (`!`) to 8;
  lowercase descenders (g, p, q, y) sit 2 rows lower; `$7F` is a checkerboard
  block. `assets/FONT.bin` is this font, cut from the extension.

---

## 30. Inter-program memory map

- **REQ-280.** Decimal addresses used to hand state between programs:

| Addr | Writer → reader | Meaning |
|---|---|---|
| 900 | END.LIB/FLOAT → FLOAT/WIN | living members `NP` |
| 901 | MENU → all | year − 1800 (48) |
| 902 | BUY SUPPLIES, END.LIB | month |
| 903 | BUY SUPPLIES, END.LIB | day |
| 904 | BUY SUPPLIES → | wagon flag (1) at start; **bullets low byte** at end |
| 905 | BUY SUPPLIES → | **yokes** at start; bullets high byte at end |
| 906–907 | all | food, 16-bit |
| 908 | all | clothing |
| 909 | BUY SUPPLIES → OREGON TRAIL | **ammo boxes** at start; **oxen** at end |
| 910–912 | all | wheels, axles, tongues |
| 913–914 | MENU → BUY SUPPLIES → OREGON TRAIL | money in **tenths of a dollar** at start; **whole dollars** at end |
| 915 | MENU → WIN | occupation 1–3 |
| 916 | END.LIB → WIN | health band `INT(H/35)` |
| 917 | END.LIB → WIN | cents |
| 919 | all | "program initialised" flag for the error trap |
| 955 | boot → | DOS volume number of the boot disk (REQ-331a) |
| 975 | all | sound on (non-zero) / off |
| 1920+ | MENU/END.LIB → | member names, NUL-terminated, in slot order |

- **REQ-281.** The meaning of 904/905/909/913–914 has changed between the start
  and end of the trail, as shown; a reimplementation that persists state must
  not conflate them.

---

## 31. UI: screens, windows, and prompts

### 31.1 Travel screen

- **REQ-290.** The travel screen has shown a scenery strip (per leg,
  `VAR.BIN:L%`), the wagon and oxen animating as it advances, the ground colour
  (§12.5), and a status panel: Date, Weather, Health, Food, Next landmark
  (miles), Miles traveled, with "Press RETURN to size up the situation".

### 31.2 Dialog boxes

- **REQ-291.** Messages have appeared in framed boxes with a triple-line border
  (`COMMON.LIB:41000`), dismissed by the space bar ("Press SPACE BAR to
  continue"). `& SPACE` (`$679A`) has accepted **Space or Return**; the key
  stays in the keyboard latch, so `PEEK(-16384) = 13` afterwards means Return
  was used. Esc twice in a row at `& SPACE`, `& INP` or a `USR` key read has run
  `GOTO 31000` (the main menu).
- **REQ-291a.** The text driver (`$6128`–`$6500`) has buffered each line and
  drawn it only when a control character, an `&` command or a full line ends
  it. A line that would reach the window's right edge has been broken at its
  last space (the space dropped) or, with no space, before the character that
  did not fit. `& IN,n` has indented the start of each buffered segment by `n`
  pixels; a negative `n` has indented only the lines produced by wrapping.
  `CHR$(3)` has centred the buffered text in the window (ignored when an
  indent is pending); `CHR$(5)` and `CHR$(6)` have cleared to the end of the
  line and of the window; `CHR$(2)` has cleared the window and homed the
  cursor; `CHR$(12)` has cleared the **whole hi-res page**, not just the
  window. A line feed past the bottom of a window has wrapped to its top line;
  nothing scrolls. Glyphs replace the pixels of their own columns and rows only
  (a space draws nothing); INVERSE inverts those cells.

### 31.3 Learn about the trail

- **REQ-292.** Five pages of text (`MENU:7000–7041`) and a colour-calibration
  page (orange, green, blue, violet) and a note on Ctrl-S.

---

## 32. Input and controls

### 32.1 Keys

- **REQ-300.** Menus have taken a single digit within range; Y/N prompts
  accepted Y/N in either case; names and epitaphs used filtered line input.
  Ctrl-S toggles sound at any "Press SPACE BAR" prompt.

### 32.2 Interrupting travel

- **REQ-301.** Return pressed during travel has opened the action menu at the
  next poll. Dismissing an event message with Return instead of Space
  (REQ-291) has set `B = 1`, which
  opens the menu and **skips the remaining events for that day**
  (`OREGON TRAIL:3180`, `710`; other blockers do the same, §10.2).

### 32.1 Pace

- **REQ-302.** The original's pace has been set by the 1 MHz 6502 running
  the BASIC, not by any timer, so most of its time has gone on the BASIC
  between screens and on loading from disk. Measured on the original (22
  games): about 4 s of booting in text mode, then the title for about 12 s; a travel poll (one step of the wagon) about
  0.36 s of BASIC beyond the drawing; the action menu about 1.5 s; a river
  crossing's animation about 10 s; loading `OREGON TRAIL` after the store
  about 14 s, and `MENU` again after a game about 12 s (7 s for a party that
  dies on side 1, which skips `COMMON.LIB:31000`'s disk check).
  *(OpenTrail does not keep this pace: drawing and loading are instant and
  the title shows for 2 s. It keeps the game's own timing: music, the hunt,
  the raft, the crossing animation, and about 2 s per travel day.)*

---

## 33. Sound and music

- **REQ-310.** Tunes have been played by `& PT` from the `MS*.BIN` buffers
  (format §29.6); sound follows memory 975. Each landmark has its own tune,
  loaded on arrival (`OREGON TRAIL:1005`); `MS0` plays on departure, `MS16`
  after the raft landing, `MS17` on arrival, `TS.BIN` at a tombstone.

---

## 34. Management options

- **REQ-319.** Before rewriting `HISCORE.SEQ`, `WIN` and `MANAGEMENT` first
  write `"L"` + CR as a write-protect probe (`WIN:535`, `MANAGEMENT:705`).
- **REQ-320.** Ctrl-A at the main menu has run `MANAGEMENT`: 1 see the current
  Top Ten, 2 see the original Top Ten, 3 erase the current Top Ten (restores the
  original), 4 erase the tombstone messages, 5 return to the main menu.
  Writes prompt to remove the write-protect sticker on error 4.

---

## 35. Error handling and disk prompts

### 35.1 Error trap

- **REQ-330.** `ONERR` has routed errors to `COMMON.LIB:32000`: I/O errors
  1–15 resume (error 12 closes the file first); error 255 (break) returns to
  the menu if `PEEK(919) <> 0`, else re-`RUN`s; on side A's `MENU` a break at
  the main prompt (`JF`) shows "Insert another diskette." and reboots
  (`:31000`, `MENU:5000`); others show "Error *n* at line #*l* in *program*.
  Please report this error to MECC." `FLOAT` and side 2's `MENU` guard their
  resume with a flag `ZU` that is never set, so there only errors 12 (and 8 in
  `FLOAT`) resume.

### 35.2 Disk-side checks

- **REQ-331.** Before disk access `COMMON.LIB:33000` has read the disk label
  with `& CDN` and prompted "Please insert side *n* of the OREGON TRAIL
  diskette." until it matches. `& CDN` (`$873F`) reads catalog track 17
  sector 15 and returns the 30-character name of the first entry, the MECC
  label pseudo-entry `[OREGON TRAIL SIDE 1    /0414]` (REQ-001): characters
  2–13 must equal `QN$` ("OREGON TRAIL") and character 20 the side digit.
- **REQ-331a.** After every disk read or write, the loader (`$31E7`) has
  copied the volume number found on the disk to `$03B2`.
  The `$3000` loader then builds two device records from the same slot and
  drive (`$3043–$3062`): record 1 = `$03B9–$03BB` (volume as booted; `$03BB` is
  memory 955) and record 2 = `$03BC–$03BE` (volume + 1, unless the volume is
  254). `& SET,n` (`$8DC6`) copies record *n* into `$03B0–$03B2`. `FLIP.LIB`
  tests `PEEK(955) <> 254`: a disk with the default volume 254 gets the "flip
  the diskette" prompt; with any other volume there is no prompt and side 2 is
  expected as volume + 1 on the **same slot and drive**, which fits an
  installation where both sides are volumes on one device. Disk images do not
  keep the volume numbers, so the shipped value is not known.
---

# Part IV, Quirks & open items

## 36. Known quirks and defects in the original

These are faithful behaviours of the original code. A faithful reimplementation
has reproduced them; a "fixed" mode, if any, must flag them.

- **REQ-340.** **A broken arm cures.** Its affliction index is 0, the "well"
  code, and afflictions overwrite: a broken arm has no effect of its own and
  erases any illness or injury the member had (§14.1).
- **REQ-341.** **The leader is immune to random selection** while others live,
  and member 1 is picked twice as often as members 2–4 (§14.2).
- **REQ-342.** **Event slot 2 is dead.** `RE(2) = 0` and its handler line
  10200 does not exist.
- **REQ-343.** **The thief never steals money.** The money branch requires an
  item index of 0, which the picker never produces (`LF.LIB:52020`).
- **REQ-344.** **"Inadequate grass" has no effect**, and "Ox wanders off" loses
  days but no ox.
- **REQ-345.** **Climate row 5 (Portland) is never used**; zones stop at 4.
- **REQ-346.** **`RC(*,4)` is unused** river data (values 3, 2, 12, 9).
- **REQ-347.** **Barlow toll with exact cash is refused silently**: the payment
  test is `MY > toll` and the "not enough" test is `MY < toll`, so exact change
  loops back to the choice.
- **REQ-348.** **No leap years**; February always has 28 days.
- **REQ-349.** **Lost days are rest days**: every day penalty runs rest days
  without pace stress (§20.2).
- **REQ-350.** **Any blocker skips the rest of the day's events**: Return
  during a message, an unrepaired part, or losing the last ox opens the menu
  and abandons the remaining events (§10.2).
- **REQ-356.** **Unreadable tombstones**: a tombstone written while `D` is
  temporarily 0 (during rest days or at a river) stores 0 miles, and the
  gravesite trigger `D < DL` can then never fire.
- **REQ-357.** **Hunting offered at a landmark**: declining "look around"
  leaves `LL = 1`, so a menu forced at that landmark (no oxen, river) offers
  Hunt instead of Talk and Buy (`OREGON TRAIL:1010`).
- **REQ-358.** **Tombstone overflow**: record 0 can exceed its 49-byte slot (up
  to about 56 bytes with a long epitaph and mileage) and overwrite the start of
  record 1; the shipped side-A `TOMB.SEQ` shows such leftovers.
- **REQ-351.** **Fruit season is fixed per leg**: `RE(11)` uses the month when
  the leg began (§17.1).
- **REQ-352.** **Tombstone record 1 is read but never written directly** by
  the game; only record 0 is written (but see REQ-358).
- **REQ-353.** **Misspellings** in shipped text: "Impassible trail", "river is
  to shallow".
- **REQ-354.** **Stale clothing figure**: after a fire or an abandoned wagon,
  clothing-per-person (`OP`) keeps its old value until the next recompute
  (§16.1). Speed is unaffected (it depends only on oxen and pace).
- **REQ-355.** `FN W` in `MENU` and `BUY SUPPLIES` has been defined with a
  different, vestigial formula and never called.

---

## 37. Open items

- **REQ-360.** This specification has been derived by static analysis of the
  original's files and checked by running the original.
- **REQ-361.** `MECC$$DISK` byte 36 (`01`) has an unknown meaning; it does not
  affect gameplay.

---

## Appendix A, Complete data tables

All values come from `VAR.BIN` unless noted.

### A.1 Landmarks, `LM$(i, 0..3)`

| i | Name | Type | Default leg | Alt leg |
|---|---|---|---|---|
| 0 | Independence | 1 store | 0 | – |
| 1 | the Kansas River crossing | 2 river | 1 | – |
| 2 | the Big Blue River crossing | 2 river | 2 | – |
| 3 | Fort Kearney | 1 store | 3 | – |
| 4 | Chimney Rock | 0 | 4 | – |
| 5 | Fort Laramie | 1 store | 5 | – |
| 6 | Independence Rock | 0 | 6 | – |
| 7 | South Pass | 0 | 7 | 8 |
| 8 | Fort Bridger | 1 store | 9 | – |
| 9 | Green River crossing | 2 river | 10 | – |
| 10 | Soda Springs | 0 | 11 | – |
| 11 | Fort Hall | 1 store | 12 | – |
| 12 | the Snake River crossing | 2 river | 13 | – |
| 13 | Fort Boise | 1 store | 14 | – |
| 14 | the Blue Mountains | 0 | 15 | 16 |
| 15 | Fort Walla Walla | 1 store | 17 | – |
| 16 | The Dalles | 0 | 18 | – |
| 17 | the Willamette Valley | 0 | 99 (end) | – |

### A.2 Legs, `LM(k, 0..2)`

| k | From → To | Miles | Base mi/day |
|---|---|---|---|
| 0 | Independence → Kansas River | 102 | 20 |
| 1 | Kansas River → Big Blue River | 83 | 20 |
| 2 | Big Blue River → Fort Kearney | 119 | 20 |
| 3 | Fort Kearney → Chimney Rock | 250 | 20 |
| 4 | Chimney Rock → Fort Laramie | 86 | 20 |
| 5 | Fort Laramie → Independence Rock | 190 | 12 |
| 6 | Independence Rock → South Pass | 102 | 12 |
| 7 | South Pass → Green River | 57 | 12 |
| 8 | South Pass → Fort Bridger | 125 | 12 |
| 9 | Fort Bridger → Soda Springs | 162 | 12 |
| 10 | Green River → Soda Springs | 144 | 12 |
| 11 | Soda Springs → Fort Hall | 57 | 12 |
| 12 | Fort Hall → Snake River | 182 | 12 |
| 13 | Snake River → Fort Boise | 114 | 12 |
| 14 | Fort Boise → Blue Mountains | 160 | 12 |
| 15 | Blue Mountains → Fort Walla Walla | 55 | 12 |
| 16 | Blue Mountains → The Dalles | 125 | 12 |
| 17 | Fort Walla Walla → The Dalles | 120 | 12 |
| 18 | The Dalles → Willamette Valley (Barlow Road) | 100 | 12 |

### A.3 Store items, `S$(i, 0..3)`

| i | Name | Base price | Unit | Plural suffix |
|---|---|---|---|---|
| 0 | Oxen | 20 | ox | en |
| 1 | Clothing | 10 | set | s |
| 2 | Ammunition | 2.00 | box | es |
| 3 | Wagon wheels | 10 | wheel | s |
| 4 | Wagon axles | 10 | axle | s |
| 5 | Wagon tongues | 10 | tongue | s |
| 6 | Food | 0.20 | pound | s |

### A.4 Climate, `WC$(zone)` decoded: base °F / precipitation chance

Daily temperature is base + 0..40 °F (§12.3).

| Zone | Jan | Feb | Mar | Apr | May | Jun | Jul | Aug | Sep | Oct | Nov | Dec |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 0 | 9 / .039 | 13 / .042 | 23 / .078 | 36 / .099 | 45 / .144 | 55 / .144 | 60 / .117 | 58 / .120 | 50 / .126 | 39 / .090 | 25 / .057 | 14 / .045 |
| 1 | 3 / .015 | 8 / .015 | 16 / .030 | 29 / .063 | 39 / .090 | 49 / .099 | 55 / .081 | 53 / .066 | 43 / .048 | 31 / .030 | 17 / .015 | 7 / .015 |
| 2 | 3 / .015 | 7 / .015 | 12 / .027 | 22 / .048 | 32 / .063 | 42 / .039 | 51 / .030 | 49 / .018 | 38 / .027 | 27 / .027 | 13 / .021 | 6 / .015 |
| 3 | −1 / .015 | 4 / .021 | 12 / .036 | 23 / .069 | 32 / .075 | 41 / .039 | 49 / .024 | 47 / .015 | 37 / .033 | 26 / .042 | 11 / .024 | 1 / .018 |
| 4 | 10 / .045 | 16 / .039 | 22 / .039 | 30 / .036 | 38 / .036 | 45 / .027 | 54 / .009 | 52 / .009 | 43 / .018 | 32 / .030 | 20 / .039 | 12 / .042 |
| 5 *(unused)* | 18 / .171 | 23 / .123 | 26 / .108 | 31 / .069 | 37 / .063 | 41 / .048 | 46 / .015 | 46 / .030 | 43 / .051 | 34 / .099 | 26 / .162 | 21 / .192 |

### A.5 Rivers, `RC(r, 0..5)`

| r | River | Base depth ft | Base width ft | Base swiftness | Bottom | `RC(r,4)` | Extra option |
|---|---|---|---|---|---|---|---|
| 0 | Kansas | 1 | 600 | 3 | 0 none | 3 | 2 ferry |
| 1 | Big Blue | 1 | 220 | 2 | 1 muddy | 2 | 0 none |
| 2 | Green | 20 | 400 | 5 | 2 rough | 12 | 2 ferry |
| 3 | Snake | 6 | 1000 | 7 | 2 rough | 9 | 1 Indian guide |

### A.6 Word lists

- `W$`: very cold, cold, cool, warm, hot, very hot, rainy, snowy, very rainy,
  very snowy.
- `H$`: good, fair, poor, very poor.
- `P$`: steady, strenuous, grueling. `R$`: filling, meager, bare bones.
- `IL$`: a broken arm, a broken leg, a snakebite, exhaustion, typhoid, cholera,
  measles, dysentery, a fever.
- `I$(1..8)`: Wagon, oxen, sets of clothing, bullets, wagon wheels, wagon axles,
  wagon tongues, pounds of food.
- `AQ$`: Continue on trail, Check supplies, Look at map, Change pace, Change
  food rations, Stop to rest, Attempt to trade, Talk to people, Buy supplies,
  Hunt for food.
- `M$`: January … December.

### A.7 Map coordinates, `MP%(i, 0..1)` (hi-res x, y)

| i | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 | 14 | 15 | 16 | 17 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| x | 245 | 235 | 226 | 207 | 185 | 170 | 155 | 143 | 132 | 134 | 124 | 110 | 93 | 80 | 70 | 69 | 55 | 45 |
| y | 139 | 136 | 126 | 122 | 120 | 114 | 100 | 108 | 117 | 108 | 104 | 101 | 102 | 87 | 76 | 66 | 71 | 70 |

### A.8 Other data blocks

- **`L%(k, 0..1)`**: per-leg scenery image number and strip width (17 rows).
- **`B(0..5)`**: status-panel label columns.
- **Constants**: `NP = 5`, `P = 1`, `R = 1`, `H = 0`, `M = 0`, `LM = 0`,
  `RE = 14` (last event index), control-character strings `D$ CC$ CL$ CM$ CE$
  CF$`.

---

## Appendix B, Ampersand command table

The 46 commands of the extension, in table order (`$8FE6` names, `$908A`
addresses; the handler is the table address + 1, §2.5). Semantics marked
*verified* have been traced in full; the others have been identified from
their entry code plus how the BASIC calls them. `& PUTP`, `& DPB`, `& KILP`,
and `& RMOVP` (raft game) come from `DALLESUTIL.OBJ` (§29.7); `USR(n)`
supplies keyboard helpers.

| Command | Handler | Meaning |
|---|---|---|
| `BOX` | `$697A` | `BOX x1,y1[,x2,y2[,colour]]`: filled rectangle, sent to the text driver as control 12 (fill at `$76A9`) — *verified*. Coordinates are relative to the **current window**, whose far corner is the default `x2,y2`. Colour 0–7 as HCOLOR (4–7 set the high bit); the default is the text attribute `$030B`, with colour 3 OR'd in when INVERSE. Row by row, the bytes are merged with a colour pattern that starts at the window's left byte; the partial bytes at each end also take the pattern's high bit. Rows come from the window's row-address table (`$6540`/`$6600`), which selecting a window rewrites only for the window's own rows: a box reaching below the window draws on the rows an earlier, taller window left there. `WIN:1005` relies on it without meaning to: its `BOX 0,170,279,191` runs in the 57-row status window (5) and so clears the bottom of the screen. `BOX` with no arguments sends control character 2 to the text driver (`$697C`), which the BASIC uses to clear the current window |
| `DFT` | `$69CD` | register image set `n` at address (§29.5) — *verified* |
| `AFT` | `$6A22` | text-driver control 11: select font `n` (0–6) from the `& DFT` slots |
| `PARSE` | `$6A2E` | text-driver control 0: buffer PRINTed lines (word wrap, centring; the default) |
| `NOPARSE` | `$6A33` | text-driver control 1: draw each PRINTed character at once, without the line buffer |
| `GCP` | `$6A38` | get text cursor position into variables |
| `VSP` | `$6A6B` | `VSP[,n[,font]]`: rows between lines (font byte 6; default 1) |
| `HSP` | `$6A71` | `HSP[,n[,font]]`: pixels between characters (font byte 5; default 2) |
| `TSP` | `$6A77` | `TSP[,n[,font]]`: rows above each glyph (font byte 7; default 0) |
| `CEL` | `$6AB4` | clear to end of line |
| `CEW` | `$6AB9` | clear to end of window |
| `SPACE` | `$6A9E` | `SPACE[,n]`: "Press SPACE BAR to continue" on the bottom line of window 0 (of the current window if `n` ≥ 1); waits for Space or Return (REQ-291) — *verified* |
| `CO` | `$6ABE` | move text cursor `x,y` |
| `HT` | `$6AF9` | text-driver control 14: horizontal tab |
| `VT` | `$6AFB` | text-driver control 13: vertical tab |
| `DFW` | `$69E7` | define text window `n AT x,y,w,h` |
| `WIND` | `$6B33` | select text window |
| `INP` | `$6B42` | `INP len,"ranges",min,var$`: line input of up to `len` characters (0: one key, no echo) and at least `min` before Return; `ranges` lists characters and `-xy` ranges, lower case falling back to upper case; `CHR$(1)`, `?` and `CHR$(27)` in it accept Ctrl-A, `?` or `/`, and Esc as an immediate answer; DELETE / left arrow erase — *verified* |
| `IN` | `$6B0A` | indent text |
| `CHIGH` | `$6509` | draw text with bit 7 clear (violet/green group) |
| `SHIGH` | `$6506` | draw text with bit 7 set (blue/orange group) |
| `HOOK` | `$60CD` | route PRINT to hi-res text, select the built-in font (`$6BEB`) and the display page |
| `UNHOOK` | `$6BC0` | restore normal output |
| `CDN` | `$873F` | read disk volume name |
| `CLOSE` | `$877B` | close text file |
| `CST` | `$8787` | `CST addr`: build the image shift tables at addr (6 shifts × 256 bytes, `$7A96`) and point the blitters at them — *verified* |
| `DBL` | `$879E` | delete BASIC line range |
| `DUN` | `$87D4` | `DUN "file"[ AT addr,pages]`: load and unpack a `.PCK` onto the page at `$E6` (§29.5); the file streams through a buffer, by default one page of DOS's own — *verified* |
| `IMAGE` | `$891E` | draw image `n,x,y` from current set (§29.5) — *verified* |
| `DPC` | `$8988` | `DPC x,y,var`: return the 2-bit colour (0–3) of the even-aligned pixel pair at (x AND $FE, y) — *verified* |
| `RFL` | `$8998` | load a file: a binary `AT addr` (or its own address), an Applesoft program relinked in place. After the first sector, whole sectors are read straight into the destination, so up to 255 bytes past the end of the file receive the rest of its last sector (`$7CD7`); the file body is *verified* |
| `RNH` | `$89F2` | load and run: Applesoft program (CLEAR + run) or binary (call its load address) — *verified* |
| `APP` | `$8A3C` | append a BASIC file after the resident program and relink — *verified* |
| `OPEN` | `$8A71` | open text file |
| `HUNT` | `$8A9A` | hunting game (§21) — *verified* |
| `PT` | `$8AF2` | play tune (§29.6) — *verified* |
| `PN` | `$8B3A` | play the next note of the current tune (§29.6) |
| `PUT` | `$8BCC` | `PUT x,y,addr`: restore a rectangle saved by `TAKE` at byte column `floor(x/7)`, row `y`; columns past 39 and rows off the page are skipped — *verified* |
| `QFH` | `$8C03` | fast string garbage collection: marks live string descriptors from the variable table, compacts strings to the top of memory, and sets FRETOP (`$6F/$70`) — *verified* |
| `SET` | `$8DC6` | select drive/side |
| `TAKE` | `$8E22` | `TAKE x1,y1,x2,y2,addr`: save the byte columns `x1/7`–`x2/7` of rows `y1`–`y2` to addr as width, height, then the rows — *verified* |
| `GTP` | `$8E99` | read back the text-file position set by `TPTR` |
| `TREED` | `$8EA3` | read field from text file |
| `TWRITE` | `$8F57` | write field to text file |
| `TPTR` | `$8F46` | set text-file position |
| `UIM` | `$8FB5` | `UIM n,addr[,var]`: unpack image *n* into memory at addr as a one-image set (header from `$7F21`, then the decoded bytes, via `$7E93`/`$8511`) and return its byte length — used to cache the wagon frames (`OREGON TRAIL:312`) — *verified* |

---

## Appendix C, Artwork

The original's pictures and tunes, as `assets/` holds them (built by
`tools/art.py assets` from `legacy/`):

| Source | Content |
|---|---|
| `L0–L17.PCK` | the 18 landmark pictures (Independence … Willamette Valley) |
| `M0.PCK` | "Map of the Oregon Trail" |
| `TS.PCK` | tombstone |
| `RIVER.PCK` | Columbia River raft-game backdrop |
| boot (`$7623`) | MECC boot title: "The Oregon Trail", MECC logo, copyright 1985 (`TITLE.RLE`) |
| `PRAIRIE.IMA` | wagon walk frames 1–3, prairie horizon, fort / Chimney Rock / river map icons |
| `MOUNTAINS.IMA` | wagon frames, mountain horizon, 12 landmark icons for the travel strip |
| `EVENTS.IMA` | thunderstorm, rain, hail/snow clouds, wagon on fire, wild fruit, lost member, snowdrift |
| `OREGON.IMA` | expert-key diagram, supply icons (food, wheel, clothing, ox, wagon), ornament |
| `FIRST.IMA` | title family and wagon, "The Oregon Trail" logo, Matt the storekeeper, "Top Ten" and "On Arriving in Oregon" banners |
| `RIVER.IMA` | crossing outcomes: ford, float, ferry, tipped wagon; river bank |
| `ORSPRITE.IMA` | raft game: raft, rock, direction sign, landing trail, and masks |
| `MS0–MS17.BIN`, `TS.BIN` | the 19 tunes (landmarks, departure, float, arrival, tombstone) |
| language card `$F780` | the hunter: 5 aiming poses × 3 walk frames (`HUNT_HUNTER.IMA`) |
| language card `$D430` | 6 carcasses, deciduous trees, pines, grass, cacti, rocks (`HUNT_SCENERY.IMA`) |
| extension `$6BEB` | the 96-glyph proportional text font (`FONT`) |
| language card `$D000` | hunting animals, 3 frames each: deer (buck), bear, buffalo, deer (no antlers), rabbit, squirrel (`HUNT_ANIMALS.IMA`) |

---

## Document history

- **2026-09.** Written from a full reading of every BASIC program and overlay
  on both disk sides, the decoded `VAR.BIN` table and the `.SEQ` databases,
  then the machine code by disassembly: the boot, the 46 ampersand commands
  (Appendix B), the picture, music and font formats (§29), the text driver
  and the hunting game (§21). Three independent audits checked every
  requirement against the BASIC, the data tables and the disassembly.
- **2026-09 to 2026-10.** The graphics, the text driver and the hunting game
  were rebuilt in C and checked pixel for pixel against the original machine
  code, and whole games were replayed against the original: the leg's strobe
  clear (REQ-080), the crossing's unplotted splash draws and stale `TD$`
  (REQ-238), the travel screen's truncated mileage (REQ-022, which needs the
  original's arithmetic bit for bit, REQ-020a), the Barlow Road arrival's
  boxes (Appendix B, `BOX`) and the original's pace (REQ-302).
- **2026-10.** REQ-302 notes that OpenTrail keeps the game's own timing but
  not the original's machine pace. REQ-192f: the stray-pixel kill of an
  empty or dead slot is skipped.
