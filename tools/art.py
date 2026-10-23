#!/usr/bin/env python3
"""OpenTrail's one tool: the game's assets, and the modern-mode art (the review
page, Retro Diffusion jobs, and the art drawn here). Run from anywhere; paths
are the repository's.

    tools/art.py assets                           assets/ from legacy/: the original
                                                  pictures, image sets, tunes and Talk
                                                  text, the boot title, the hunt's sprite
                                                  sets and the font
    tools/art.py embed [--art] OUT.c FILE...      the legacy assets (or --art, modern's
                                                  pictures) as a C file, so the binary
                                                  carries them (src/host/embed.h;
                                                  run by the Makefile)
    tools/art.py gallery [--port N] [--no-open]   the review page (localhost)
    tools/art.py originals                        cut the Apple art out to build/originals/
    tools/art.py local [--check]                  the art drawn here, into assets/modern/:
                                                  the map, the two river scenes, the
                                                  travel backdrops (plains and mountains;
                                                  grass, dry, snow), the divider and the
                                                  text font (from Press Start 2P)
    tools/art.py anchors [--check]                the hunt sprites' feet points, measured
                                                  from assets/modern/hunt_*.png, into
                                                  src/modern/hunt_anchor.h
    tools/art.py balance                          Retro Diffusion credit left
    tools/art.py cost JOB                         free price check of a job
    tools/art.py plan                             free price check of every job; art/PLAN.md
                                                  (each job, its images, its run, its price)
    tools/art.py run JOB [--dry-run]              PAID: generate a job
    tools/art.py seam ID IMAGE [--seam-width N] [--estimate]
                                                  PAID: seam_tiling, so IMAGE loops left to
                                                  right (--estimate is free)
    tools/art.py keep IN OUT... [--magenta|--edge] background to transparent: keep only the
                                                  blue (river icons), drop the magenta and
                                                  any frame drawn round the picture, or
                                                  drop the edge-joined background colour
    tools/art.py check IMAGE [REFERENCE]          a backdrop: does it loop, where are its lines

Retro Diffusion:
  * Everything is under art/: manifest.json (every image, its size and its
    source), jobs/<id>.json, inputs/ (the images jobs start from) and
    runs/<id>/runNN/ (a new directory per run, with the request, the task id,
    written before any polling so a paid result can always be fetched, and the
    poll result). All of it is committed. Once an image is approved, its job
    keeps that one run, holding only the file that went into the game
    (01_raw.png, frame_NN.png, or 02_edited.png with edit.json when the
    leftover background was removed after generation).
    Approved art is copied into assets/modern/ by hand.
  * A job can start from an image (input_image_path, strength) or pass images
    that guide it without being redrawn (reference_image_paths, RD Pro only).
  * A job's "_accept" list is its acceptance checklist, shown on the review page
    under the art (keys beginning with "_" are notes and are never sent).
  * The token is read from ~/.config/retrodiffusion/token.
  * Never run a paid job without the user's go-ahead on the prompt and price.
"""
import argparse
import base64
import glob
import html
import http.server
import io
import json
import os
import random
import shutil
import subprocess
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
import webbrowser

try:                                    # the art's; assets and embed (the build) need neither
    import numpy as np
    from PIL import Image, ImageDraw, ImageSequence
except ImportError:
    np = Image = None

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MANIFEST = "art/manifest.json"
JOBS = "art/jobs"
ASSETS = "assets/modern"
ORIG = "build/originals"                 # made from legacy/ by `originals`, not kept
OUT_ROOT = "art/runs"


def hexc(h):
    return tuple(int(h[i:i + 2], 16) for i in (1, 3, 5))


def assets():
    return json.load(open(MANIFEST))["assets"]


# ============================================================================
# The originals
# ============================================================================

# Original image sets, as the game loads them (index n is 1-based).
SETS = ["FIRST.IMA", "OREGON.IMA", "EVENTS.IMA", "PRAIRIE.IMA", "MOUNTAINS.IMA",
        "RIVER.IMA", "ORSPRITE.IMA"]
RAW_SETS = ["HUNT_ANIMALS.IMA", "HUNT_SCENERY.IMA", "HUNT_HUNTER.IMA"]

# Each leg's landmark icon on the travel strip: SCENERY[leg][0] (data.c), from
# PRAIRIE.IMA before the disk flips at Fort Laramie, MOUNTAINS.IMA after.
ICON = {1: "PRAIRIE.IMA 8", 2: "PRAIRIE.IMA 8", 3: "PRAIRIE.IMA 5", 4: "PRAIRIE.IMA 6",
        5: "PRAIRIE.IMA 7", 6: "MOUNTAINS.IMA 6", 7: "MOUNTAINS.IMA 7", 8: "MOUNTAINS.IMA 8",
        9: "MOUNTAINS.IMA 14", 10: "MOUNTAINS.IMA 9", 11: "MOUNTAINS.IMA 10",
        12: "MOUNTAINS.IMA 14", 13: "MOUNTAINS.IMA 11", 14: "MOUNTAINS.IMA 15",
        15: "MOUNTAINS.IMA 12", 16: "MOUNTAINS.IMA 13", 17: "MOUNTAINS.IMA 16"}

# The hunt's sets (hunt.c): animal n = type*3 + frame (buck, bear, buffalo,
# deer, rabbit, squirrel); scenery 1-6 the six shot animals, then tree, pine,
# grass, cactus, rock, three each; hunter = 5 aims x 3 walking frames. The
# modern sprites run 4 frames (frame 4 is the original's frame 2 again), and the
# modern hunter faces right (the game mirrors him), so its aims are the
# originals' mirrored: upright is the original's upleft, and so on.
ANIMALS = ["deer_buck", "bear", "buffalo", "deer", "rabbit", "squirrel"]
HUNT_SCENERY = ["tree", "pine", "grass", "cactus", "rock"]
AIMS = ["up", "upright", "right", "downright", "down"]
FRAME_OF = {1: 1, 2: 2, 3: 3, 4: 2}
# The store's supply pictures (buy.c): the set at 37380 is OREGON.IMA.
STORE_ICON = {"oxen": 8, "food": 2, "clothing": 5, "bullets": 6,
              "wheel": 3, "axle": 3, "tongue": 3}

# The assets drawn here (cmd_local) rather than generated.
LOCAL = {"ui_divider", "ui_font", "scene_map", "scene_tombstone",
         "scene_columbia", "scene_river_crossing",
         "travel_bg_prairie_grass", "travel_bg_prairie_dry", "travel_bg_prairie_snow",
         "travel_bg_mountains_grass", "travel_bg_mountains_dry", "travel_bg_mountains_snow"}


def original_of(a):
    """The original art an asset redraws: ('pck', name) or ('ima', set, n), or None."""
    i = a["id"]
    if i.startswith("travel_icon_"):
        s, n = ICON[int(i[-2:])].split()
        return ("ima", s, int(n))
    if i.startswith("hunt_"):
        name, last = i[5:].rsplit("_", 1)
        if name in ANIMALS:
            t = ANIMALS.index(name)
            if last == "dead":
                return ("ima", "HUNT_SCENERY.IMA", t + 1)
            return ("ima", "HUNT_ANIMALS.IMA", t * 3 + FRAME_OF[int(last)])
        if name in HUNT_SCENERY:
            return ("ima", "HUNT_SCENERY.IMA", 7 + HUNT_SCENERY.index(name) * 3 + int(last) - 1)
        if name.startswith("hunter_"):
            return ("ima", "HUNT_HUNTER.IMA", AIMS.index(name[7:]) * 3 + FRAME_OF[int(last)])
    if i.startswith("ui_icon_"):
        n = STORE_ICON.get(i[8:])
        return ("ima", "OREGON.IMA", n) if n else None
    o = a.get("original")
    if not o:
        return None
    if o.endswith(".PCK"):
        return ("pck", o)
    s, _, n = o.partition(" ")
    return ("ima", s, int(n)) if n else None


def orig_path(o):
    if o[0] == "pck":
        return f"{ORIG}/pck/{o[1]}.png"
    return f"{ORIG}/ima/{o[1]}_{o[2]:02d}.png"


def hires_row(y):
    return (y & 7) * 0x400 + ((y >> 3) & 7) * 0x80 + (y >> 6) * 0x28


def unpack_pck(buf):
    """& DUN's unpacker (gfx.c amp_dun): column by column, 192 rows each."""
    page = bytearray(8192)
    r = c = p = 0

    def out(v):
        nonlocal r, c
        if c < 40:
            page[hires_row(r) + c] = v
        r += 1
        if r == 192:
            r, c = 0, c + 1
    while p < len(buf):
        b = buf[p]; p += 1
        if b & 0xC0 == 0:
            for _ in range(b + 1):
                if p >= len(buf):
                    break
                out(buf[p]); p += 1
        elif b >= 0xC0:
            break
        elif b < 0x80:
            v = buf[p] if p < len(buf) else 0; p += 1
            for _ in range((b & 0x3F) + 1):
                out(v)
        else:
            x, y = buf[p], buf[p + 1]; p += 2
            for _ in range((b & 0x3F) + 1):
                out(x); out(y)
    return bytes(page)


def cmd_originals(_=None):
    subprocess.run(["make", "-s", "build/ima2png", "build/scr2png"], check=True)
    os.makedirs(f"{ORIG}/ima", exist_ok=True)
    os.makedirs(f"{ORIG}/pck", exist_ok=True)
    for s in SETS:
        subprocess.run(["build/ima2png", s, f"{ORIG}/ima/{s}"], check=True)
    for s in RAW_SETS:
        subprocess.run(["build/ima2png", s, f"{ORIG}/ima/{s}", "raw"], check=True)
    pcks = sorted({o[1] for a in assets() for o in [original_of(a)] if o and o[0] == "pck"})
    for name in pcks:
        page = f"{ORIG}/pck/{name}.page"
        open(page, "wb").write(unpack_pck(open(f"assets/{name}.bin", "rb").read()))
        full = f"{ORIG}/pck/{name}.full.png"
        subprocess.run(["build/scr2png", page, full], check=True)
        # the picture is 280x160 (the caption box sits below); one Apple pixel each
        im = Image.open(full).convert("RGB")
        bottom = max((y for y in range(0, im.height, 2)
                      if any(im.getpixel((x, y)) != (0, 0, 0) for x in range(0, im.width, 2))),
                     default=319)
        h = 160 if bottom < 320 else 192
        im.crop((0, 0, 560, h * 2)).resize((280, h), Image.NEAREST).save(orig_path(("pck", name)))
    print(f"originals in {ORIG}/")


def ensure_originals():
    if any(o and not os.path.exists(orig_path(o)) for o in map(original_of, assets())):
        print("originals missing; cutting them out")
        cmd_originals()


# ============================================================================
# The game's assets

def cmd_assets(_):
    """Assemble assets/ (the bundled original data the game loads) from legacy/.

    Pictures (.PCK), image sets (.IMA), tunes (MS*.BIN, TS.BIN) and the Talk
    text (OREGON1/2.SEQ) are copied as raw DOS file bodies from
    legacy/extracted, and the MECC boot title (RLE at $7623 on the
    uncatalogued boot tracks) and the extension's font ($6BEB) are cut out of
    the rebuilt boot memory images. Replace these files to reskin the game."""
    out = "assets"
    os.makedirs(out, exist_ok=True)
    n = 0
    for d in ("legacy/extracted/sideA", "legacy/extracted/sideB"):
        for f in sorted(os.listdir(d)):
            name = None
            if f.endswith(".PCK.bin") or f.endswith(".IMA.bin"):
                name = f
            elif f.startswith(("MS", "TS.BIN")) and f.endswith(".BIN.bin"):
                name = f
            elif f.startswith("OREGON") and f.endswith(".SEQ.txt"):
                name = f[:-4] + ".bin"          # OREGON1.SEQ.txt -> OREGON1.SEQ.bin
            if name:
                shutil.copyfile(os.path.join(d, f), os.path.join(out, name))
                n += 1
    mem = open("legacy/memory/A_title_stage.bin", "rb").read()
    p = 0x7623
    # the decoder ($7400) stops only on exactly $C0; walk the stream so a $C0 inside a literal run is not mistaken for the end
    q = p
    while mem[q] != 0xC0:
        c = mem[q]
        if c < 0x40:
            q += 2 + c
        elif c < 0x80:
            q += 2
        else:
            q += 3
    open(os.path.join(out, "TITLE.RLE.bin"), "wb").write(mem[p:q + 1])
    # The hunting game's three sprite sets live in language-card RAM (bank 2),
    # loaded by the boot: animals, carcasses and scenery, and the hunter.
    lc = open("legacy/memory/A_mem.bin", "rb").read()
    for base, name in ((0xD000, "HUNT_ANIMALS"), (0xD430, "HUNT_SCENERY"), (0xF780, "HUNT_HUNTER")):
        cnt, idx = lc[base], lc[base + 1]
        ext = 0
        for k in range(1, cnt + 1):
            lo, hi = lc[base + idx + 2 * k], lc[base + idx + 2 * k + 1]
            off = lo | (hi & 0x3F) << 8
            if off:
                ext = max(ext, off + 2 + lc[base + off] * lc[base + off + 1])
        open(os.path.join(out, name + ".IMA.bin"), "wb").write(lc[base:base + ext])
    # the extension's proportional font, $6BEB-$7023 (spec REQ-279i), loaded back at $6BEB
    open(os.path.join(out, "FONT.bin"), "wb").write(lc[0x6BEB:0x7024])
    print(f"{n} files + TITLE.RLE ({q + 1 - p} bytes) + 3 hunt sets -> {out}/")


def cmd_embed(args):
    """Files as a C table, so the game binary carries them and needs no files
    beside it (src/host/embed.h): the legacy assets (assets/*.bin, named by
    their DOS file name) as EMBEDDED, or with --art modern mode's pictures
    (assets/modern/*.png, named by their id) as EMBEDDED_ART."""
    files = sorted(args.files)
    table, ext = ("EMBEDDED_ART", ".png") if args.art else ("EMBEDDED", ".bin")
    lines = ["// Generated by tools/art.py embed. Do not edit.",
             '#include "embed.h"', ""]
    for i, path in enumerate(files):
        data = open(path, "rb").read()
        lines.append("static const unsigned char f%d[%d] = {" % (i, max(len(data), 1)))
        for p in range(0, len(data), 24):
            lines.append("    " + ",".join(str(b) for b in data[p:p + 24]) + ",")
        lines.append("};")
    lines.append("const EmbeddedFile %s[] = {" % table)
    for i, path in enumerate(files):
        name = os.path.basename(path)[:-len(ext)]
        lines.append('    { "%s", f%d, %d },' % (name, i, os.path.getsize(path)))
    if not files:
        lines.append("    { 0, 0, 0 },")
    lines.append("};")
    lines.append("const int %s_COUNT = %d;" % (table, len(files)))
    open(args.c_file, "w").write("\n".join(lines) + "\n")


# ============================================================================
# Art drawn here: the map, the river scenes, the travel backdrop
# ============================================================================

MAP_X, MAP_Y = 4, 12    # the map's place on the canvas (src/modern/modern.c)


def draw_map():
    """The original map at one Apple pixel per pixel, its lettering and marks
    taken out by hand (art/inputs/map_clean.png), framed on the panels' dark
    brown (view_modern.c), the legend's room to its right."""
    m = Image.open("art/inputs/map_clean.png").convert("RGB")
    out = Image.new("RGB", (384, 216), (12, 10, 8))
    out.paste((60, 48, 36), (MAP_X - 1, MAP_Y - 1, MAP_X + m.width + 1, MAP_Y + m.height + 1))
    out.paste(m, (MAP_X, MAP_Y))
    return out


# ---- the river scenes --------------------------------------------------------
# The Columbia (the raft run, FLOAT) and the river crossing (CROSS.LIB). Their
# originals are flat geometry -- cyan water, black banks, green land -- and the
# game places its sprites on that geometry, so it is kept exactly and given
# texture: green steps down to a sandy edge on the shores, current streaks
# along the river, foam at the edges, shadows from light at the upper left.
# Seeded: every run draws the same pictures.

RCW, RCH = 384, 216
RW, RB, RG = 0, 1, 2                             # water, bank, land

# Ramps, dark to light: water The Dalles' (scene_landmark_16, the same river),
# grass the landmarks'.
WATER = [hexc(h) for h in ("#083c69", "#035b89", "#0c86ad", "#1ab3de", "#14d9fd", "#76e1f8", "#9df2fc", "#fcfdfd")]
GRASS = [hexc(h) for h in ("#032a10", "#044424", "#1a7a2c", "#0aef34", "#17fc3a", "#47e864", "#a8f87c")]
STEPS = [hexc(h) for h in ("#0b5e26", "#0d8a30", "#10b836")]    # the shore, from the water up
STEP_LIP = hexc("#47e864")
SAND = [hexc("#e8d86a"), hexc("#c8b84a")]
G_GRASS, G_WATER, G_STEPS, G_STEP_LIP, G_SAND = GRASS, WATER, STEPS, STEP_LIP, SAND

if np is not None:
    BAYER = np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]) / 16.0 - 0.5
    LIGHT = np.array([-1.0, -1.0, 1.1]) / np.linalg.norm([-1.0, -1.0, 1.1])     # upper left, raised

RIVERS = {
    # The whole FLOAT screen, mapped as modern.c's rx()/ry() map the sprites.
    "scene_columbia": {"orig": ("pck", "RIVER.PCK"), "streaks": 700, "foam": 0.5, "seed": 1847, "pines": 46, "palette": "set"},
    # CROSS.LIB's picture (RIVER.IMA 5, at 58,24 in its box), filling the canvas.
    "scene_river_crossing": {"orig": ("ima", "RIVER.IMA", 5), "streaks": 380, "foam": 0.2, "seed": 1848},
}
SHORE, STEP, BANK_H = 14, 5, 10                  # shore width, step width, bank height


def regions(orig):
    """Each canvas pixel's class, from the original stretched to the canvas,
    each pixel then taking the commonest class around it (colour fringes out)."""
    im = np.asarray(Image.open(orig).convert("RGB")).astype(int)
    if im.shape[1] == 280:                      # a full screen: its last column is the
        im[:, -1] = im[:, -2]                    # Apple's colour fringe, not the picture
    h, w, _ = im.shape
    px = im[(np.arange(RCH) * h / RCH).astype(int)][:, (np.arange(RCW) * w / RCW).astype(int)]
    r, g, b = px[..., 0], px[..., 1], px[..., 2]
    cls = np.full((RCH, RCW), RB)
    cls[(b > 150) & (b > r + 60)] = RW
    cls[(g > 150) & (g > b + 60) & (g > r + 60)] = RG
    pad = np.pad(cls, 2, mode="edge")
    counts = np.zeros((3, RCH, RCW), int)
    for dy in range(5):
        for dx in range(5):
            win = pad[dy:dy + RCH, dx:dx + RCW]
            for c in range(3):
                counts[c] += win == c
    return counts.argmax(0)


def distance(mask, limit=80):
    """Octagonal distance from each pixel to the nearest pixel of mask."""
    d = np.where(mask, 0.0, np.inf)
    front = mask.copy()
    for step in range(1, limit):
        p = np.pad(front, 1)
        grow = p[1:-1, :-2] | p[1:-1, 2:] | p[:-2, 1:-1] | p[2:, 1:-1]
        if step % 2:
            grow |= p[:-2, :-2] | p[:-2, 2:] | p[2:, :-2] | p[2:, 2:]
        new = grow & np.isinf(d)
        if not new.any():
            break
        d[new] = step
        front = front | new
    return np.minimum(d, limit)


def flow_axis(cls):
    """The river's direction: the main axes of its edges (each fitted alone,
    the two banks being far apart), averaged by length."""
    water = cls == RW
    total = np.zeros(2)
    for edge in (water[1:] & ~water[:-1], water[:-1] & ~water[1:]):    # upper bank, lower bank
        ys, xs = np.nonzero(edge)
        if len(xs) < 10:
            continue
        pts = np.stack([xs - xs.mean(), ys - ys.mean()])
        vals, vecs = np.linalg.eigh(pts @ pts.T)
        v = vecs[:, vals.argmax()]
        total += (v if v[0] >= 0 else -v) * len(xs)
    return total / np.linalg.norm(total)


def smooth_noise(rng, cell):
    """Value noise: random values on a coarse grid, bilinearly interpolated."""
    grid = np.array([[rng.random() for _ in range(RCW // cell + 2)]
                     for _ in range(RCH // cell + 2)]) - 0.5
    y, x = np.arange(RCH) / cell, np.arange(RCW) / cell
    y0, x0 = y.astype(int), x.astype(int)
    fy, fx = (y - y0)[:, None], (x - x0)[None, :]
    a, b = grid[y0][:, x0], grid[y0][:, x0 + 1]
    c, d = grid[y0 + 1][:, x0], grid[y0 + 1][:, x0 + 1]
    return (a * (1 - fx) + b * fx) * (1 - fy) + (c * (1 - fx) + d * fx) * fy


def ramp_index(v, ramp):
    """v in 0..1 to a ramp index, ordered-dithered."""
    yy, xx = np.mgrid[0:RCH, 0:RCW]
    return np.clip(np.floor(v * (len(ramp) - 1) + 0.5 + BAYER[yy % 4, xx % 4] * 0.9),
                   0, len(ramp) - 1).astype(int)


def draw_river(p):
    rng = random.Random(p["seed"])
    # the colours: the original ramps, or ("set") the generated scenes' own, as the crossing has
    if p.get("palette") == "set":
        GRASS, WATER, STEPS = X_GRASS + [X_GRASS[-1]], X_WATER, X_GRASS[:3]
        STEP_LIP, SAND = X_GRASS[4], [X_EARTH[4], X_EARTH[3]]
    else:
        GRASS, WATER, STEPS, STEP_LIP, SAND = G_GRASS, G_WATER, G_STEPS, G_STEP_LIP, G_SAND
    cls = regions(orig_path(p["orig"]))
    edge = smooth_noise(rng, 9) * 10             # the shore, then grass beyond it
    cls = np.where((cls == RB) & (distance(cls == RW) > SHORE + edge), RG, cls)
    dw = distance(cls == RW)                     # to the water
    dg = distance(cls == RG)                     # to the land on top
    dland = distance(cls != RW)                  # into the water, from the bank
    noise = smooth_noise(rng, 6) * 1.5
    rise = BANK_H * np.sqrt(dw / np.maximum(dw + dg, 1))   # the bank: steep at the water
    h = np.where(cls == RW, -np.minimum(dland, 30) * 0.15, rise)
    h = np.where(cls == RG, BANK_H + noise, h)
    gy, gx = np.gradient(h)                      # light: the slope's facing
    n = np.stack([-gx, -gy, np.ones_like(h)])
    lit = np.tensordot(LIGHT, n / np.linalg.norm(n, axis=0), axes=1)
    dark = np.zeros_like(h, bool)                # shadow: a higher pixel up and to the left
    step_rise = LIGHT[2] / np.hypot(LIGHT[0], LIGHT[1]) * np.sqrt(2)
    for k in range(1, 40):
        src = np.full_like(h, -np.inf)
        src[k:, k:] = h[:-k, :-k]
        dark |= src > h + k * step_rise
    tex = smooth_noise(rng, 3) + smooth_noise(rng, 11) * 0.8
    for _ in range(RCW * RCH):                   # draws a rock grain once took; kept so
        rng.random()                             # the approved pictures stay the same
    out = np.zeros((RCH, RCW, 3), np.uint8)
    yy, xx = np.mgrid[0:RCH, 0:RCW]

    # the shores: green steps down to a sandy edge
    shore = cls == RB
    k = np.clip((dw + tex * 2) // STEP, 0, len(STEPS) - 1).astype(int)
    out[shore] = np.array(STEPS)[k][shore]
    out[shore & (np.roll(k, 1, 0) < k)] = STEP_LIP          # the lit face of each step
    sand = shore & (dw <= 1)
    out[sand] = np.where((BAYER[yy % 4, xx % 4] > 0)[..., None], SAND[1], SAND[0])[sand]

    # the land: grass by its light, tufts
    gi = ramp_index(np.clip(0.62 + (lit - 0.62) * 0.8 + tex * 0.2 - dark * 0.3, 0, 1), GRASS)
    out[cls == RG] = np.array(GRASS)[gi][cls == RG]
    for _ in range(int(RCW * RCH * (cls == RG).mean() / 16)):
        x, y = rng.randrange(RCW), rng.randrange(1, RCH)
        if cls[y, x] == RG and cls[y - 1, x] == RG:
            out[y, x] = GRASS[max(0, gi[y, x] - 2)]
            out[y - 1, x] = GRASS[min(len(GRASS) - 1, gi[y, x] + 1)]

    # the water: lighter at the banks, deep mid-river, shaded under the banks
    depth = np.clip(dland / 26.0, 0, 1)
    wi = ramp_index(np.clip(0.72 - depth * 0.2 + tex * 0.06 - dark * 0.3, 0, 1), WATER)
    water = cls == RW
    out[water] = np.array(WATER)[wi][water]

    # current streaks along the river: a light crest over a dark trough
    fx, fy = flow_axis(cls)
    for _ in range(p["streaks"]):
        x0, y0 = rng.uniform(0, RCW), rng.uniform(0, RCH)
        if not (0 <= int(y0) < RCH and cls[int(y0), int(x0)] == RW):
            continue
        n = rng.randint(4, 22)
        white = rng.random() < p["foam"] * (1.2 - depth[int(y0), int(x0)])
        for t in range(n):
            x, y = int(x0 + fx * t), int(y0 + fy * t)
            if not (0 <= x < RCW and 1 <= y < RCH - 1) or cls[y, x] != RW:
                break
            base = wi[y, x]
            out[y, x] = WATER[-1] if white and 1 < t < n - 2 else WATER[min(base + 2, 6)]
            if cls[y + 1, x] == RW:
                out[y + 1, x] = WATER[max(base - 2, 0)]

    # foam where the water meets the bank
    edge = water & (dland <= 1)
    out[edge] = WATER[-1]
    out[water & (dland + tex * 3 <= 2.5) & ~edge & (BAYER[yy % 4, xx % 4] > 0)] = WATER[-2]

    # conifers on the land, well back from the water (the Columbia's gorge),
    # lit from the upper left, drawn back to front
    trees = []
    for _ in range(p.get("pines", 0) * 6):
        x, y = rng.randrange(4, RCW - 4), rng.randrange(14, RCH)
        if cls[y, x] == RG and dw[y, x] > 9:
            trees.append((y, x, rng.randint(9, 17)))
        if len(trees) >= p.get("pines", 0):
            break
    for y, x, h in sorted(trees):
        for row in range(h):
            ty = y - h + row
            half = int(1 + row * 0.42) - (row % 4 == 0)          # tiers
            for dx in range(-half, half + 1):
                tx = x + dx
                if 0 <= tx < RCW and 0 <= ty < RCH:
                    out[ty, tx] = X_LEAF[4] if dx < -half // 2 else X_LEAF[2] if dx <= half // 3 else X_LEAF[1]
        for ty in range(y, min(y + 2, RCH)):
            out[ty, x] = X_EARTH[0]
    return Image.fromarray(out)


# The river crossing (CROSS.LIB's scene), drawn here in the generated scenes'
# own colours (sampled from scene_landmark_01, the Kansas River): sky, a far
# meadow with a line of cottonwoods (open where the wagon comes out), the river
# across the middle with its current, the near bank's grass and reeds; no
# trail on either bank. The horizon is y 26: cross() in modern.c takes the
# wagon from (250, 196) on the near bank to (150, 62) on the far one, smaller
# with the distance (CROSS_HORIZON).
X_SKY = [hexc(h) for h in ("#3fd3fd", "#96f8fd", "#fdfefe")]
X_WATER = [hexc(h) for h in ("#0e3864", "#16508c", "#1c63aa", "#2a80c8", "#349fe0", "#3fd3fd", "#96f8fd", "#fdfefe")]
X_GRASS = [hexc(h) for h in ("#196917", "#257729", "#39932c", "#4dab47", "#5cc35a", "#6cd580")]
X_LEAF = [hexc(h) for h in ("#02332d", "#024532", "#145b29", "#196917", "#39932c", "#5cc35a")]
X_EARTH = [hexc(h) for h in ("#4c2734", "#72444e", "#834e4b", "#9c6d73", "#aa817b")]
FAR_EDGE, NEAR_EDGE = 58, 174                    # the water between these rows


def dither(v, ramp, yy, xx):
    """v in [0, 1] to the ramp, ordered-dithered between its steps."""
    f = np.clip(v, 0, 1) * (len(ramp) - 1)
    i = np.floor(f).astype(int)
    up = (f - i) * 16 > BAYER[yy % 4, xx % 4]
    return np.array(ramp)[np.clip(i + up, 0, len(ramp) - 1)]


def draw_crossing():
    rng = random.Random(1848)
    W, H = RCW, RCH
    yy, xx = np.mgrid[0:H, 0:W]
    tex = smooth_noise(rng, 3) + smooth_noise(rng, 9) * 0.8
    far = FAR_EDGE + (smooth_noise(rng, 14) * 3).astype(int)[0]          # the far edge, along x
    near = NEAR_EDGE + (smooth_noise(rng, 14) * 3).astype(int)[0]
    out = np.zeros((H, W, 3), np.uint8)
    # the sky, lighter down to the horizon, and two clouds
    sky = yy < 26
    out[sky] = dither(yy / 26.0 * 0.9 + tex * 0.05, X_SKY, yy, xx)[sky]
    for cx, cy, r in ((70, 9, 14), (300, 12, 18)):
        cl = ((xx - cx) / (r * 1.6)) ** 2 + ((yy - cy) / (r * 0.45)) ** 2 < 1
        out[cl & sky] = X_SKY[2]
    # the far meadow, darker toward the horizon
    mead = (yy >= 26) & (yy < far[None, :])
    out[mead] = dither(0.35 + (yy - 26) / 40.0 + tex * 0.12, X_GRASS, yy, xx)[mead]
    # the cottonwoods along it: crowns lit from the upper left, trunks under them
    for i in range(11):
        cx = 18 + i * 34 + rng.randint(-8, 8)
        if 120 < cx < 180:                       # open where the wagon comes out
            continue
        r = rng.randint(8, 13)
        cy = 40 - r // 2
        trunk = (abs(xx - cx) <= 1) & (yy > cy) & (yy < far[cx] - 1)
        out[trunk] = X_EARTH[0]
        d = np.hypot((xx - cx) / 1.15, yy - cy) + smooth_noise(rng, 2)[:H, :W] * 2.5
        crown = d < r
        light = 0.75 - ((xx - cx) + (yy - cy)) / (2.4 * r) * 0.5 - d / r * 0.25
        out[crown] = dither(light + tex * 0.1, X_LEAF, yy, xx)[crown]
    # the water: darker mid-river, lighter at the banks, the current in streaks
    water = (yy >= far[None, :]) & (yy < near[None, :])
    across = np.clip((yy - far[None, :]) / np.maximum(near[None, :] - far[None, :], 1), 0, 1)
    depth = np.sin(across * np.pi)                    # 0 at the banks, 1 mid-river
    out[water] = dither(0.68 - depth * 0.55 + tex * 0.06, X_WATER[:6], yy, xx)[water]
    for _ in range(240):                              # the current: a light crest over a dark trough,
        x0, y0 = rng.randrange(W), rng.randrange(FAR_EDGE + 4, NEAR_EDGE - 4)
        near_k = (y0 - 26) / (NEAR_EDGE - 26)         # shorter and thinner with the distance (the horizon y 26)
        n = max(2, round(rng.randint(5, 16) * near_k * 1.3))
        for t in range(n):
            x = x0 + t
            if x >= W or not water[y0, x] or not water[y0 + 1, x]:
                break
            base = int(np.clip(0.68 - depth[y0, x] * 0.55, 0, 1) * 5)
            out[y0, x] = X_WATER[7] if 1 < t < n - 2 and rng.random() < 0.08 else X_WATER[min(base + 2, 6)]
            if near_k > 0.5:                          # the trough only near enough to show
                out[y0 + 1, x] = X_WATER[max(base - 1, 0)]
    edge = water & ((yy - far[None, :] <= 1) | (near[None, :] - yy <= 1))
    out[edge] = X_WATER[6]
    # the banks' edges: mud
    for e, up in ((far, True), (near, False)):
        band = (yy >= e[None, :] - 3) & (yy < e[None, :]) if up else (yy >= e[None, :]) & (yy < e[None, :] + 4)
        out[band] = dither(0.55 + tex * 0.15, X_EARTH, yy, xx)[band]
    # the near bank: grass, lighter toward us, reeds along the water
    bank = yy >= near[None, :] + 4
    out[bank] = dither(0.5 + (yy - NEAR_EDGE) / 80.0 + tex * 0.12, X_GRASS, yy, xx)[bank]
    for _ in range(90):
        x = rng.randrange(W)
        top = near[x] + 4 - rng.randint(4, 9)
        for y in range(top, near[x] + 6):
            out[y, x] = X_LEAF[1] if y < top + 2 else X_LEAF[2]
    return Image.fromarray(out)


# The tombstone (TOMB.LIB's picture): one rounded headstone, whole in the
# frame, with a flat face for the words modern.c cuts in it (CARVE_X/Y in
# view_modern.c: the face's middle at x 192, the words from y 64), on dark
# earth with grass at its base, under a night sky.
X_STONE = [hexc(h) for h in ("#2b2b33", "#45454f", "#62626c", "#80808a", "#a0a0a8", "#c4c4ca", "#e4e4e8")]


def draw_tombstone():
    rng = random.Random(1848)
    W, H = RCW, RCH
    yy, xx = np.mgrid[0:H, 0:W]
    tex = smooth_noise(rng, 2) + smooth_noise(rng, 7) * 0.8
    grain = np.array([[rng.random() for _ in range(W)] for _ in range(H)])
    out = np.zeros((H, W, 3), np.uint8)
    night = [hexc(h) for h in ("#000105", "#030d1b", "#0a1730")]
    out[:] = dither(yy / H * 0.8, night, yy, xx)                        # the night: black to deep blue
    for _ in range(70):                                                 # stars
        x, y = rng.randrange(W), rng.randrange(150)
        out[y, x] = X_SKY[2] if rng.random() < 0.25 else X_WATER[3]
    base = 184 + (smooth_noise(rng, 20) * 2).astype(int)
    ground = yy >= base
    out[ground] = dither(0.02 + (yy - 184) / 160.0 + tex * 0.1, X_EARTH[:3], yy, xx)[ground]
    sprig = ground & (grain_g := np.array([[rng.random() for _ in range(W)] for _ in range(H)])) .__lt__(0.06)
    out[sprig] = np.array(X_GRASS[1])
    for _ in range(40):                                                 # pebbles
        x, y = rng.randrange(W), rng.randrange(190, H)
        out[y, x] = X_EARTH[3]; out[y, min(x + 1, W - 1)] = X_EARTH[2]
    # the stone: a rectangle under a half-disc, bevelled, lit from the upper left
    cx, top, bot, half = 192, 26, 186, 80
    arch_y = top + half
    d_arch = np.hypot(xx - cx, yy - arch_y)
    inside = ((np.abs(xx - cx) <= half) & (yy >= arch_y) & (yy <= bot)) | ((d_arch <= half) & (yy < arch_y))
    rim = np.where(yy < arch_y, half - d_arch, half - np.abs(xx - cx))
    rim = np.minimum(rim, bot - yy + 4)
    side = np.where(yy < arch_y, -((xx - cx) + (yy - arch_y)) / np.maximum(d_arch, 1), -np.sign(xx - cx))  # +1 lit, -1 shaded
    lit = 0.55 + tex * 0.06 + (grain - 0.5) * 0.18                     # granite speckle
    bevel = np.clip(1 - rim / 7, 0, 1)
    lit = lit + bevel * side * 0.32
    # the face: a slightly recessed panel for the words, its edge a thin shadow line and a lit line
    face = inside & (np.abs(xx - cx) <= half - 14) & (yy >= arch_y - 10) & (yy <= bot - 26)
    face_edge = inside & ~face & (np.abs(xx - cx) <= half - 12) & (yy >= arch_y - 12) & (yy <= bot - 24)
    lit = np.where(face, lit + 0.05, lit)
    moss = np.clip((yy - (bot - 30)) / 30.0, 0, 1) * (0.6 + tex * 0.4)  # weathering up from the ground
    out[inside] = dither(lit, X_STONE, yy, xx)[inside]
    mossy = inside & (grain < moss * 0.55)
    out[mossy] = np.where((grain < moss * 0.3)[..., None], np.array(X_LEAF[2]), np.array(X_LEAF[1]))[mossy]
    out[inside & (rim < 1)] = X_STONE[0]
    # cracks: thin dark lines with a lit pixel beside them, kept off the face
    for _ in range(9):
        x = cx + rng.randint(-half + 4, half - 4)
        y = top + rng.randint(6, 30) if abs(x - cx) < half / 2 else arch_y + rng.randint(-10, 80)
        for _ in range(rng.randint(8, 22)):
            if 0 <= y < H and inside[y, x] and not face[y, x]:
                out[y, x] = X_STONE[0]
                if x + 1 < W and inside[y, x + 1]:
                    out[y, x + 1] = X_STONE[5]
            x += rng.choice((-1, 0, 0, 1)); y += 1
            if y >= bot:
                break
    # its shadow on the ground, to the right
    shadow = ground & (xx > cx - half) & (xx < cx + half + 30) & (yy < base + 8) & ~inside
    out[shadow & (BAYER[yy % 4, xx % 4] > 5)] = X_EARTH[0]
    # grass at the base: a tuft on the left, a spiky clump on the right
    for bx, n, tall in ((cx - half - 4, 18, 12), (cx + half + 8, 34, 22)):
        for _ in range(n):
            x0 = bx + rng.randint(-14, 14)
            h0 = rng.randint(tall // 3, tall)
            lean = rng.choice((-2, -1, 0, 1, 2))
            for t in range(h0):
                x, y = x0 + (lean * t) // 5, 187 - t
                if 0 <= x < W:
                    out[y, x] = X_GRASS[4] if t > h0 * 2 // 3 else X_GRASS[2] if t > h0 // 3 else X_GRASS[0]
    return Image.fromarray(out)

def river_check(img):
    """The Columbia with the raft's track over it: the original raft's 42x28
    footprint at hp 0..17 (finish.c:59), placed as modern.c places sprites;
    hp 0 and 17 (red) are where FLOAT says the raft hit the bank."""
    im = img.convert("RGB").resize((RCW * 2, RCH * 2), Image.NEAREST)
    d = ImageDraw.Draw(im)
    for hp in range(0, 18):
        x, y = 84 + 8 * hp, 5 * hp - 6
        box = [x * RCW / 280 * 2, y * RCH / 192 * 2, (x + 42) * RCW / 280 * 2, (y + 28) * RCH / 192 * 2]
        d.rectangle(box, outline=(255, 0, 0) if hp in (0, 17) else (255, 255, 0))
    im.save("build/river_check.png")
    print("build/river_check.png")


# ---- the travel backdrops ----------------------------------------------------
# Plains, 192x64, looping left to right by construction (every shape repeats
# within 192), drawn at 2x (384x128) with the wagon walking in place: one
# layout, coloured for grass, drought and snow.
# Rows: 0-27 sky (a flat horizon), 28 the horizon (landmarks stand there),
# 28-45 meadow, 38 the wagon's ground line, 46-49 hedge row, 50-63 foreground.

TW, TH = 192, 64
T_HORIZON, T_HEDGE, T_FORE = 28, 46, 50
SKY_HIGH, SKY_LOW, HAZE, CLOUD = map(hexc, ("#03e3e9", "#4febed", "#b9f9f9", "#e8fcfd"))
# horizon line, meadow, fleck, dark fleck, hedge, dark hedge, deep hedge, foreground:
# green from the first approved backdrop, drought and snow from the generated ones
PLAINS = {
    "grass": ("#73ac66", "#91cf6c", "#7bbb67", "#73ac66", "#649960", "#5c935c", "#3b6c44", "#beeab7"),
    "dry":   ("#bc9942", "#d5b552", "#c9ac4b", "#bc9942", "#b69440", "#93752a", "#6e5420", "#eccf60"),
    "snow":  ("#b0d3df", "#e6fbfc", "#caebf5", "#b0d3df", "#b4d7e2", "#a6c9d4", "#6a8c99", "#f2fdfe"),
}


# The mountains: snow-capped peaks along the horizon, lit from the upper left,
# a distant tree line at their foot. Peaks are (centre x, height, half width),
# repeating every 192 like the rest.
PEAKS = ((18, 19, 30), (66, 13, 24), (108, 21, 34), (158, 15, 28))
ROCK_LIT, ROCK_SHADE, ROCK_DEEP = map(hexc, ("#afc2ce", "#76939f", "#5f7b88"))
SNOW_LIT, SNOW_SHADE = map(hexc, ("#f9fdfd", "#dcebec"))
TREES = {"grass": ("#5d976b", "#3b6c44"), "dry": ("#6e7d4a", "#4b5530"), "snow": ("#3d6663", "#2b4a48")}


def draw_mountains(im, ground):
    rng = random.Random(1849)
    px = im.load()
    rough = [rng.choice((0, 0, 1, -1)) for _ in range(TW)]       # a ragged ridge
    cap = [rng.choice((0, 1, 1, 2)) for _ in range(TW)]          # a ragged snow line
    tree = [rng.choice((1, 2, 2, 3)) for _ in range(TW)]         # the tree line's crowns
    tree_lit, tree_dark = map(hexc, TREES[ground])
    for x in range(TW):
        best, lit = 0, True
        for cx, ph, hw in PEAKS:
            dx = (x - cx + TW // 2) % TW - TW // 2                # wrapped: the strip loops
            hgt = ph * (1 - abs(dx) / hw)
            if hgt > best:
                best, lit = hgt, dx < 0                          # the left flank faces the light
        if best <= 0:
            continue
        top = T_HORIZON - round(best) + rough[x]
        snow_to = top + round(best * 0.3) + cap[x] if best > 8 else top - 1
        for y in range(max(top, 0), T_HORIZON):
            if y <= snow_to:
                c = SNOW_LIT if lit else SNOW_SHADE
            else:
                c = ROCK_LIT if lit else ROCK_SHADE
            px[x, y] = ROCK_DEEP if y == top and not lit else c
    for x in range(TW):                                          # the tree line
        for y in range(T_HORIZON - tree[x], T_HORIZON):
            px[x, y] = tree_dark if y >= T_HORIZON - 1 or (x + y) % 3 == 0 else tree_lit
    return im


def draw_divider():
    """ui_divider, 320x16: the title page's ornament line (FIRST.IMA 3) in the
    modern panel's parchment gold, on transparency: a centre diamond between
    two small ones, lines that taper to a dot at each end."""
    W, H, cy = 320, 16, 8
    hi, mid, lo = (240, 220, 170, 255), (200, 168, 110, 255), (120, 92, 58, 255)
    im = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    px = im.load()

    def diamond(cx, r):
        for dy in range(-r, r + 1):
            for dx in range(-(r - abs(dy)), r - abs(dy) + 1):
                edge = abs(dx) + abs(dy) == r
                px[cx + dx, cy + dy] = lo if edge else (hi if dy < 0 or (dy == 0 and dx < 0) else mid)

    for side in (-1, 1):
        for i in range(12, 150):                        # the line, 3 rows near the centre, 1 at the ends
            x = W // 2 + side * i
            px[x, cy] = mid
            if i < 110:
                px[x, cy - 1] = hi
                px[x, cy + 1] = lo
        px[W // 2 + side * 152, cy] = hi                 # the end dot
        px[W // 2 + side * 156, cy] = mid
        diamond(W // 2 + side * 24, 3)                   # the small diamonds either side
    diamond(W // 2, 6)                                   # the centre one
    return im


def draw_travel(ground):
    line, meadow, fleck, fleck_dark, hedge, hedge_dark, hedge_deep, fore = map(hexc, PLAINS[ground])
    rng = random.Random(1848)
    im = Image.new("RGB", (TW, TH))
    px = im.load()
    for x in range(TW):
        for y in range(TH):
            px[x, y] = (SKY_HIGH if y < 8 else SKY_LOW if y < 14 else HAZE if y < T_HORIZON
                        else line if y == T_HORIZON else meadow if y < T_HEDGE
                        else hedge if y < T_FORE else fore)
        if rng.random() < 0.5:                   # the hedge's top edge, uneven
            px[x, T_HEDGE - 1] = hedge_dark
        for y in range(T_HEDGE, T_FORE):
            r = rng.random()
            px[x, y] = hedge_deep if r < 0.15 else hedge_dark if r < 0.45 else hedge
    for _ in range(140):                         # flecks, fewer toward the horizon
        x, y = rng.randrange(TW), rng.randrange(T_HORIZON + 2, T_HEDGE - 1)
        if rng.random() < (y - T_HORIZON) / (T_HEDGE - T_HORIZON):
            px[x, y] = fleck_dark if rng.random() < 0.3 else fleck
    for cx, cy, cw in ((40, 4, 14), (130, 9, 10)):   # two small clouds
        for dx in range(cw):
            px[(cx + dx) % TW, cy] = CLOUD
        for dx in range(2, cw - 2):
            px[(cx + dx) % TW, cy - 1] = CLOUD
    return im


# The hunt's sprites stand on the field by their feet: for each image, the
# column of the body and the row under the feet, measured from the art. The
# hunter's four walk frames and an animal's four run frames share one anchor
# (their mean column, their lowest row), so a walk stays in place and the feet
# keep the image's own step; a dead pose and terrain have their own. Written to
# src/modern/hunt_anchor.h, which hunt_frame() in modern.c places sprites by.
ANCHOR_H = "src/modern/hunt_anchor.h"


def hunt_anchors():
    import glob, re
    groups = {}
    for f in sorted(glob.glob(os.path.join(ROOT, ASSETS, "hunt_*.png"))):
        name = os.path.basename(f)[:-4]
        m = re.fullmatch(r"(hunt_(?!tree|pine|grass|cactus|rock)[a-z_]+?)_([1-4])", name)
        groups.setdefault(m.group(1) if m else name, []).append(name)
    out = {}
    for key, names in groups.items():
        xs, bottoms = [], []
        for n in names:
            a = np.array(Image.open(os.path.join(ROOT, ASSETS, n + ".png")).convert("RGBA"))[:, :, 3] > 0
            rows = np.nonzero(a.any(axis=1))[0]
            y0, y1 = rows[0], rows[-1]
            band = a[max(y0, y1 - max(2, (y1 - y0 + 1) // 4)):y1 + 1]       # the lowest quarter: the feet
            cols = np.nonzero(band)[1] if key.startswith("hunt_hunter") else np.nonzero(a.any(axis=0))[0]
            xs.append((cols.min() + cols.max() + 1) / 2 if not key.startswith("hunt_hunter") else cols.mean() + 0.5)
            bottoms.append(y1 + 1)
        for n in names: out[n] = (round(float(np.mean(xs)) * 2) / 2, int(max(bottoms)))
    return out


def cmd_anchors(args):
    an = hunt_anchors()
    lines = ["// Generated by tools/art.py anchors from assets/modern/hunt_*.png: each hunt",
             "// image's feet point in image pixels (x the body's column, y the row under",
             "// the feet). Sorted by id for bsearch.",
             "typedef struct { const char *id; float x, y; } HuntAnchor;",
             "static const HuntAnchor HUNT_ANCHOR[] = {"]
    lines += [f'    {{ "{k}", {x:.1f}f, {y}.0f }},' for k, (x, y) in sorted(an.items())]
    lines += ["};", ""]
    text = "\n".join(lines)
    path = os.path.join(ROOT, ANCHOR_H)
    if args.check:
        ok = os.path.exists(path) and open(path).read() == text
        print(f"{ANCHOR_H}: {'up to date' if ok else 'STALE (run tools/art.py anchors)'}")
        sys.exit(0 if ok else 1)
    open(path, "w").write(text)
    print(f"{ANCHOR_H}: {len(an)} images")


# The text font: Press Start 2P (third_party/pressstart2p, SIL Open Font
# License 1.1), a fixed-width pixel font, rendered at its own 8 px into a 16 x 6
# grid of 8 x 8 cells for characters 32-126, white on transparent. The game
# (view_modern.c) cuts its glyphs from the grid.
FONT_TTF = "third_party/pressstart2p/PressStart2P-Regular.ttf"


def draw_font():
    from PIL import ImageFont
    f = ImageFont.truetype(os.path.join(ROOT, FONT_TTF), 8)
    mask = Image.new("L", (16 * 8, 6 * 8), 0)
    d = ImageDraw.Draw(mask)
    for c in range(32, 127):
        i = c - 32
        d.text(((i % 16) * 8, (i // 16) * 8), chr(c), font=f, fill=255)
    im = Image.new("RGBA", mask.size, (255, 255, 255, 0))
    im.putalpha(mask.point(lambda v: 255 if v >= 128 else 0))
    return im


def cmd_local(args):
    ensure_originals()
    drawn = [("scene_map", draw_map()), ("ui_divider", draw_divider()), ("ui_font", draw_font()),
             ("scene_tombstone", draw_tombstone())] + [(f"travel_bg_prairie_{g}", draw_travel(g)) for g in PLAINS] + \
            [(f"travel_bg_mountains_{g}", draw_mountains(draw_travel(g), g)) for g in PLAINS]
    for i, im in drawn:
        im.save(f"{ASSETS}/{i}.png")
        print(f"{ASSETS}/{i}.png")
    for i, p in RIVERS.items():
        img = draw_crossing() if i == "scene_river_crossing" else draw_river(p)
        img.save(f"{ASSETS}/{i}.png")
        print(f"{ASSETS}/{i}.png")
        if i == "scene_columbia" and args.check:
            river_check(img)


# ============================================================================
# Post-processing and checks
# ============================================================================

def cmd_keep(args):
    a = np.asarray(Image.open(args.inp).convert("RGBA")).copy()
    r, g, b = (a[..., i].astype(int) for i in range(3))
    if args.magenta:
        bg = (r > 90) & (b > 70) & (g < np.minimum(r, b) // 2)   # magenta, light or dark
        # a frame the model drew around the picture: outer rows and columns that
        # are almost all one colour (the outer ring's commonest)
        ring = np.concatenate([a[0, :, :3], a[-1, :, :3], a[:, 0, :3], a[:, -1, :3]]).astype(int)
        cols, n = np.unique(ring, axis=0, return_counts=True)
        frame = np.abs(a[..., :3].astype(int) - cols[n.argmax()]).sum(2) < 60
        drop = np.zeros_like(bg)
        h, w = frame.shape
        for edge in ("top", "bottom", "left", "right"):
            for k in range(min(h, w) // 4):
                line = {"top": (k, slice(None)), "bottom": (h - 1 - k, slice(None)),
                        "left": (slice(None), k), "right": (slice(None), w - 1 - k)}[edge]
                if frame[line].mean() < 0.9:
                    break
                drop[line] = True
        a[drop | bg, 3] = 0
    elif args.edge:
        # the background the picture was set on: the corners' colour, taken away
        # wherever it joins the edge (so the same colour inside the subject stays)
        corners = np.array([a[0, 0, :3], a[0, -1, :3], a[-1, 0, :3], a[-1, -1, :3]]).astype(int)
        bgc = np.median(corners, axis=0)
        near = np.abs(a[..., :3].astype(int) - bgc).sum(2) < 48
        drop = np.zeros_like(near)
        h, w = near.shape
        todo = [(y, x) for y in range(h) for x in (0, w - 1)] + [(y, x) for x in range(w) for y in (0, h - 1)]
        while todo:
            y, x = todo.pop()
            if 0 <= y < h and 0 <= x < w and near[y, x] and not drop[y, x]:
                drop[y, x] = True
                todo += [(y + 1, x), (y - 1, x), (y, x + 1), (y, x - 1)]
        # the background colour shut inside the subject (between legs, spokes),
        # then a cast shadow: neutral greys joined to what has gone
        rgb = a[..., :3].astype(int)
        drop |= np.abs(rgb - bgc).sum(2) < 12
        grey = (rgb.max(2) - rgb.min(2) < 10) & (rgb.max(2) > 40) & (rgb.max(2) < 170)
        todo = list(zip(*np.nonzero(drop)))
        while todo:
            y, x = todo.pop()
            for yy, xx in ((y + 1, x), (y - 1, x), (y, x + 1), (y, x - 1)):
                if 0 <= yy < h and 0 <= xx < w and grey[yy, xx] and not drop[yy, xx]:
                    drop[yy, xx] = True
                    todo.append((yy, xx))
        a[drop, 3] = 0
    else:
        a[~((b > r + 40) & (b > 90)), 3] = 0
    for out in args.out:
        Image.fromarray(a).save(out)
        print(out)


def loop_ratio(a):
    """The difference across the join (last column to first) over the typical
    difference between neighbouring columns; 1.5 or less loops."""
    adj = np.median([np.abs(a[:, x] - a[:, x + 1]).sum() for x in range(a.shape[1] - 1)])
    return np.abs(a[:, -1] - a[:, 0]).sum() / adj


def lines(a):
    """The rows where the picture changes most from the row above (its bands)."""
    d = np.abs(np.diff(a.mean(1), axis=0)).sum(1)
    return sorted(int(y) + 1 for y in np.argsort(d)[-4:])


def cmd_check(args):
    a = np.asarray(Image.open(args.image).convert("RGB")).astype(int)
    lr = loop_ratio(a)
    print(f"{os.path.relpath(args.image)} {a.shape[1]}x{a.shape[0]}  loop {lr:.1f} "
          f"({'loops' if lr <= 1.5 else 'does not loop'})  lines {lines(a)}")
    if args.reference:
        r = np.asarray(Image.open(args.reference).convert("RGB")).astype(int)
        print(f"  reference lines {lines(r)}")


# ============================================================================
# Retro Diffusion
# ============================================================================

API = "https://api.retrodiffusion.ai/v1"
TOKEN_FILE = os.path.expanduser("~/.config/retrodiffusion/token")


def token():
    if not os.path.exists(TOKEN_FILE):
        sys.exit(f"art: no token at {TOKEN_FILE}\n  write your rdpk- key there, chmod 600.")
    tok = open(TOKEN_FILE).read().strip()
    if not tok.startswith("rdpk-"):
        sys.exit(f"art: {TOKEN_FILE} does not look like an rdpk- key")
    return tok


def api(tok, method, path, payload=None, timeout=60):
    data = json.dumps(payload).encode() if payload is not None else None
    req = urllib.request.Request(f"{API}{path}", data=data, method=method)
    req.add_header("X-RD-Token", tok)
    if data:
        req.add_header("Content-Type", "application/json")
    try:
        with urllib.request.urlopen(req, timeout=timeout) as r:
            return json.loads(r.read().decode())
    except urllib.error.HTTPError as e:
        body = e.read().decode(errors="replace")
        try:
            return json.loads(body)
        except Exception:
            return {"detail": {"code": str(e.code), "message": body[:400]}}


def save_json(path, obj):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w") as f:
        json.dump(obj, f, indent=1)


def new_run_dir(job_id):
    """A fresh numbered directory per run, after the highest (never in a gap a
    deleted one left), so the newest sorts last. Generated art is paid for and
    not reproducible: never reuse, never overwrite."""
    base = os.path.join(OUT_ROOT, job_id)
    os.makedirs(base, exist_ok=True)
    runs = [int(r[3:]) for r in os.listdir(base) if r.startswith("run") and r[3:].isdigit()]
    d = os.path.join(base, f"run{max(runs, default=0) + 1:02d}")
    os.makedirs(d)
    return d


def poll(tok, path, work):
    """Wait for a task; the result, its status saved to poll.json."""
    for i in range(120):
        time.sleep(4)
        s = api(tok, "GET", path)
        if s.get("status") in ("succeeded", "failed"):
            save_json(os.path.join(work, "poll.json"), {k: v for k, v in s.items() if k != "result"})
            print(f"[{(i + 1) * 4}s] {s['status']}")
            return s
    return None


# Every key a job may carry. Keys beginning with "_" are notes and never sent.
# Anything else is refused rather than ignored: a key the payload does not
# forward would send a paid request that is not the one the job describes.
JOB_KEYS = {
    "id", "prompt", "style", "width", "height", "figure", "num_images",
    "input_image_path", "input_image_keep_alpha", "reference_image_paths",
    # forwarded to the API by request_payload()
    "seed", "remove_bg", "return_non_bg_removed", "frames_duration", "return_spritesheet", "strength",
    "bypass_prompt_expansion", "tile_x", "tile_y",
}
FORWARDED = ("seed", "remove_bg", "return_non_bg_removed", "frames_duration", "return_spritesheet", "strength",
             "bypass_prompt_expansion", "tile_x", "tile_y")


def load_job(path):
    job = json.load(open(path))
    for k in ("id", "prompt", "style", "width", "height"):
        if k not in job:
            sys.exit(f"art: job {path} missing required key '{k}'")
    unknown = sorted(k for k in job if not k.startswith("_") and k not in JOB_KEYS)
    if unknown:
        sys.exit(f"art: job {path} has unknown key(s): {', '.join(unknown)}\n"
                 f"     Add each to JOB_KEYS (and FORWARDED if the API takes it), or\n"
                 f"     prefix it with '_' if it is only a note.")
    job["_path"] = path
    return job


def request_payload(job, *, check_cost=False):
    p = {"prompt": job["prompt"], "prompt_style": job["style"],
         "width": job["width"], "height": job["height"], "num_images": job.get("num_images", 1)}
    p.update({k: job[k] for k in FORWARDED if k in job})
    # img2img. The API wants RGB with no alpha, so the input is flattened onto
    # white, unless input_image_keep_alpha (the animations: a transparent start
    # frame gives transparent frames).
    if job.get("input_image_path"):
        src = Image.open(job["input_image_path"]).convert("RGBA")
        if not job.get("input_image_keep_alpha"):
            flat = Image.new("RGB", src.size, (255, 255, 255))
            flat.paste(src, (0, 0), src)
            src = flat
        buf = io.BytesIO()
        src.save(buf, format="PNG")
        p["input_image"] = base64.b64encode(buf.getvalue()).decode()
    # reference images (RD Pro styles, up to 9): they guide the subject and look
    # without being redrawn, unlike input_image, which is redrawn
    refs = job.get("reference_image_paths") or []
    if refs:
        p["reference_images"] = []
        for path in refs:
            buf = io.BytesIO()
            Image.open(path).convert("RGB").save(buf, format="PNG")
            p["reference_images"].append(base64.b64encode(buf.getvalue()).decode())
    p["check_cost" if check_cost else "async"] = True
    return p


def qa(im, job):
    """The done-criteria that can be measured. Returns (rows, ok)."""
    tw, th = job["width"], job["height"]
    im = im.convert("RGBA")
    px = im.load()
    w, h = im.size
    opaque = [(x, y) for y in range(h) for x in range(w) if px[x, y][3] == 255]
    partial = sum(1 for y in range(h) for x in range(w) if 0 < px[x, y][3] < 255)
    colours = {px[x, y][:3] for (x, y) in opaque}
    xs = [p[0] for p in opaque] or [0]
    ys = [p[1] for p in opaque] or [0]
    rows = [("dimensions", f"{w}x{h}", (w, h) == (tw, th)),
            ("partial-alpha px", partial, partial == 0),
            ("unique colours", len(colours), 12 <= len(colours) <= 40)]
    if job.get("figure", True):
        fw, fh = max(xs) - min(xs) + 1, max(ys) - min(ys) + 1
        rows += [("top row clear", f"starts row {min(ys)}", min(ys) >= 1),
                 ("fills height", f"{fh}/{th} rows", fh >= th - 3),
                 ("silhouette width", f"{fw}/{tw} cols", fw >= 30),
                 ("feet on bottom row", max(ys) == h - 1, max(ys) == h - 1)]
    return rows, all(ok for _, _, ok in rows)


def contact_sheet(final, job, work, frames=None):
    """The check that cannot be automated: the result at 1x, 2x and 4x over
    the prairie and over black; animation frames in a row beneath."""
    imgs = [Image.open(f).convert("RGBA") for f in frames] if frames else [final]
    grounds = [(112, 150, 70, 255), (0, 0, 0, 255)] if job.get("figure", True) \
        or job.get("remove_bg") else [(24, 24, 28, 255)]
    pad = 10
    w, h = imgs[0].size
    W = max(pad + len(grounds) * sum(w * z + pad for z in (1, 2, 4)), pad + len(imgs) * (w * 2 + pad))
    H = pad + h * 4 + pad + (h * 2 + pad if len(imgs) > 1 else 0)
    sheet = Image.new("RGBA", (W, H), (40, 40, 44, 255))
    x = pad
    for g in grounds:
        for z in (1, 2, 4):
            cell = Image.new("RGBA", (w, h), g)
            cell.alpha_composite(imgs[0])
            sheet.paste(cell.resize((w * z, h * z), Image.NEAREST), (x, pad))
            x += w * z + pad
    if len(imgs) > 1:
        x, y = pad, pad + h * 4 + pad
        for im in imgs:
            cell = Image.new("RGBA", (w, h), grounds[0])
            cell.alpha_composite(im)
            sheet.paste(cell.resize((w * 2, h * 2), Image.NEAREST), (x, y))
            x += w * 2 + pad
    out = os.path.join(work, "contact_sheet.png")
    sheet.save(out)
    return out


def write_frames(raw_p, job, work):
    """An animation's frames, one file each. The grid is derived from what
    came back (a 4-frame sheet is 2x2, a 6-frame one 3x2), never assumed; a
    GIF knows its own frames."""
    im = Image.open(raw_p)
    if getattr(im, "n_frames", 1) > 1:
        frames = [f.convert("RGBA") for f in ImageSequence.Iterator(im)]
    else:
        w, h = job["width"], job["height"]
        sw, sh = im.size
        if sw % w or sh % h:
            # RD can round the frame up (156x60 came back as 160x64 cells): the
            # grid of frames_duration cells whose size is nearest the job's
            n = job.get("frames_duration")
            grids = [(c, n // c) for c in range(1, (n or 0) + 1) if n % c == 0 and sw % c == 0 and sh % (n // c) == 0]
            if not grids:
                sys.exit(f"art: sheet {sw}x{sh} is not a whole number of {w}x{h} cells")
            c, r = min(grids, key=lambda g: abs(sw // g[0] - w) + abs(sh // g[1] - h))
            w, h = sw // c, sh // r
            print(f"  sheet {sw}x{sh}: {c}x{r} frames of {w}x{h} (asked {job['width']}x{job['height']})")
        cols, rows = sw // w, sh // h
        if cols * rows < 2:
            return []
        im = im.convert("RGBA")
        frames = [im.crop((c * w, r * h, c * w + w, r * h + h)) for r in range(rows) for c in range(cols)]
    out = []
    for i, f in enumerate(frames):
        fp = os.path.join(work, f"frame_{i:02d}.png")
        f.save(fp)
        out.append(fp)
    return out


# An attack grew the silhouette by 28-34% over the first frame in every run
# measured; a walk by under 7%.
MOTION_GATE_PCT = 25


def cmd_balance(_):
    print(json.dumps(api(token(), "GET", "/inferences/credits")))


def price(tok, job):
    return api(tok, "POST", "/inferences", request_payload(job, check_cost=True))


def cmd_cost(args):
    job = load_job(args.job)
    r = price(token(), job)
    print(f"{job['id']}: ${r.get('balance_cost')}  (remaining ${r.get('remaining_balance')})")


def cmd_plan(_):
    """The free price of every job, written to art/PLAN.md."""
    tok = token()
    rows, total = [], 0.0
    for f in sorted(glob.glob(f"{JOBS}/*.json")):
        job = load_job(f)
        ip = job.get("input_image_path")
        if ip and not os.path.exists(ip):         # an animation's still not made yet:
            blank = "/tmp/art_blank.png"           # priced on a blank of its size
            Image.new("RGBA", (job["width"], job["height"]), (0, 0, 0, 0)).save(blank)
            job["input_image_path"] = blank
        c = price(tok, job).get("balance_cost")
        if c is None:
            sys.exit(f"art: no price for {f}")
        total += c
        a = job.get("_asset")
        images = ", ".join(a) if isinstance(a, list) else (a or job["id"])
        runs = sorted(glob.glob(f"{OUT_ROOT}/{job['id']}/run*"))
        rows.append((job["id"], job["style"], f"{job['width']}x{job['height']}",
                     job.get("_original", "text"), c, images,
                     os.path.basename(runs[-1]) if runs else "none"))
        print(f"{job['id']:<32} ${c:.2f}")
    with open("art/PLAN.md", "w") as md:
        md.write("# Modern-mode art plan\n\nMade by `tools/art.py plan`: one row per job in art/jobs/, "
                 "the images it makes, its one run (art/runs/<job>/runNN/, the approved one) and the "
                 "price of running it again (Retro Diffusion's free check). The recipe is in "
                 "docs/ARCHITECTURE.md; each image's source file is in art/manifest.json.\n\n"
                 "| job | style | size | images | run | cost |\n|---|---|---|---|---|---|\n")
        for r in rows:
            md.write(f"| {r[0]} | {r[1]} | {r[2]} | {r[5]} | {r[6]} | ${r[4]:.2f} |\n")
        md.write(f"\n**Total: ${total:.2f}** to run all {len(rows)} jobs again, one attempt each.\n")
    print(f"\ntotal ${total:.2f} for {len(rows)} jobs (one attempt each); art/PLAN.md")


def cmd_run(args):
    tok = token()
    job = load_job(args.job)
    work = new_run_dir(job["id"])
    shutil.copy(job["_path"], os.path.join(work, "job.json"))
    print(f"run dir {work}")
    payload = request_payload(job)
    save_json(os.path.join(work, "request.json"), {k: v for k, v in payload.items() if k not in ("input_image", "reference_images")})
    cost = price(tok, job)
    print(f"cost ${cost.get('balance_cost')}  balance ${cost.get('remaining_balance')}")
    if args.dry_run:
        print("dry run; nothing submitted")
        return
    sub = api(tok, "POST", "/inferences", payload)
    save_json(os.path.join(work, "task.json"), sub)      # before any polling
    task = sub.get("task_id")
    if not task:
        sys.exit(f"art: submit failed: {json.dumps(sub)[:300]}")
    print(f"task {task} (saved to {work}/task.json)")
    result = poll(tok, f"/inferences/tasks/{task}", work)
    if not result or result.get("status") != "succeeded":
        sys.exit(f"art: job did not succeed: {json.dumps(result)[:300]}")
    res = result.get("result") or {}
    imgs = res.get("base64_images") or []
    if not imgs:
        sys.exit(f"art: no image returned: {json.dumps(res)[:300]}")
    print(f"charged ${res.get('balance_cost')}  request {res.get('request_id')}")
    raw_p = os.path.join(work, "01_raw.png")
    open(raw_p, "wb").write(base64.b64decode(imgs[0]))
    for i, extra in enumerate(imgs[1:], start=2):        # every image the response carries
        extra_p = os.path.join(work, f"01_raw_{i}.png")
        open(extra_p, "wb").write(base64.b64decode(extra))
        print(f"also returned: {extra_p}")
    raw = Image.open(raw_p).convert("RGBA")
    print(f"raw {raw.size}  ({len(imgs)} image(s) returned)")
    frames = write_frames(raw_p, job, work)
    if frames:
        widths = [(lambda bb: bb[2] - bb[0] if bb else 0)(Image.open(fp).convert("RGBA").getbbox())
                  for fp in frames]
        growth = round(100 * (max(widths) - (widths[0] or 1)) / (widths[0] or 1))
        print(f"frames {len(frames)}  ->  {work}/frame_00..{len(frames) - 1:02d}.png")
        print(f"  silhouette widths {widths}")
        print(f"  [{'PASS' if growth >= MOTION_GATE_PCT else 'FAIL'}] motion "
              f"{growth}% growth over frame 0 (gate {MOTION_GATE_PCT}%)")
    rows, ok = qa(raw, job)
    print(f"\nQA  {job['id']}")
    for name, val, good in rows:
        print(f"  [{'PASS' if good else 'FAIL'}] {name:<20} {val}")
    print(f"\ncontact sheet: {contact_sheet(raw, job, work, frames)}")
    print(f"final: {raw_p}")
    print(f"\nverdict: {'PASS' if ok else 'NEEDS WORK'} "
          f"(metrics only -- look at the contact sheet before accepting)")


def cmd_seam(args):
    """seam_tiling (POST /edit/tools/seam_tiling): RD repairs the image's left
    and right edges so it loops. --estimate validates and prices the request
    without running it. The result lands in art/runs/<ID>/runNN/01_raw.png."""
    tok = token()
    src = Image.open(args.image).convert("RGB")
    buf = io.BytesIO()
    src.save(buf, format="PNG")
    payload = {"input_image": base64.b64encode(buf.getvalue()).decode(),
               "tile_x": True, "tile_y": False, "seam_width": args.seam_width}
    shown = {k: v for k, v in payload.items() if k != "input_image"}
    if args.estimate:
        r = api(tok, "POST", "/edit/tools/seam_tiling/estimate", payload)
        print(f"seam_tiling {args.image} {src.size}: {json.dumps(shown)}\n  estimate: {json.dumps(r)[:400]}")
        return
    work = new_run_dir(args.id)
    save_json(os.path.join(work, "request.json"), dict(shown, tool="seam_tiling", input_path=args.image))
    shutil.copy(args.image, os.path.join(work, "00_input.png"))
    sub = api(tok, "POST", "/edit/tools/seam_tiling/tasks", payload)
    save_json(os.path.join(work, "task.json"), sub)      # before any polling
    task = sub.get("task_id")
    if not task:
        sys.exit(f"art: submit failed: {json.dumps(sub)[:300]}")
    print(f"run dir {work}\ntask {task} (saved to {work}/task.json)")
    result = poll(tok, f"/edit/tasks/{task}", work)
    if not result or result.get("status") != "succeeded":
        sys.exit(f"art: seam_tiling did not succeed: {json.dumps(result)[:300]}")
    res = result.get("result") or result
    imgs = res.get("base64_images") or []
    if imgs:
        data = base64.b64decode(imgs[0])
    elif res.get("output_urls"):
        with urllib.request.urlopen(res["output_urls"][0], timeout=60) as r:
            data = r.read()
    else:
        sys.exit(f"art: no image returned: {json.dumps(res)[:300]}")
    out = os.path.join(work, "01_raw.png")
    open(out, "wb").write(data)
    print(f"charged ${res.get('balance_cost')}  request {res.get('request_id')}\nfinal: {out}")


# ============================================================================
# The review page
# ============================================================================
# One row per image in the manifest (and per job for an image not in it yet):
# the file and what it redraws, the original Apple art, and the new art -- the
# game's file once it is in the game, else the job's newest run.
# A reload rebuilds the page.

SERVED = ("art/", "build/originals/", "assets/modern/")        # the only files the page hands out


def new_art(a, jobs):
    """(path, label) of the newest generated art for asset a, or None; an
    animation's frame n is frame n of its cycle, else the still."""
    last = a["id"].rsplit("_", 1)[-1]
    found = []
    for j in jobs:
        runs = sorted(glob.glob(f"{OUT_ROOT}/{j['id']}/run*/01_raw.png"))
        if not runs:
            continue
        run = os.path.dirname(runs[-1])
        p, note = runs[-1], ""
        if j["id"].endswith("_anim") and last.isdigit():
            f = f"{run}/frame_{int(last) - 1:02d}.png"
            if os.path.exists(f):
                p, note = f, f", frame {last} of 4"
        elif j["id"].endswith("_still") and last.isdigit() and last != "1":
            note = ", the still (frame 1)"
        rank = 2 if j["id"].endswith("_anim") else 1      # a finished cycle beats its still
        found.append((rank, os.path.getmtime(p), p, f"{j['id']} {os.path.basename(run)}{note}"))
    return max(found)[2:] if found else None


def page():
    items = assets()
    known = {x["id"] for x in items}
    jobs = {}
    for f in sorted(glob.glob(f"{JOBS}/*.json")):
        j = json.load(open(f))
        a = j.get("_asset")
        for i in a if isinstance(a, list) else [a] if a else []:
            jobs.setdefault(i, []).append(j)
            if i not in known:                              # a job's image, not in the manifest yet
                known.add(i)
                items.append({"id": i, "w": j["width"], "h": j["height"], "group": "new", "desc": j.get("_note", "")})
    ensure_originals()

    def img(path):
        return f'<img class="img" src="/{urllib.parse.quote(path)}" loading="lazy">'

    def source(i, game):
        """The run whose file is exactly the game's image, if any."""
        data = open(game, "rb").read()
        for j in jobs.get(i, []):
            for f in sorted(glob.glob(f"{OUT_ROOT}/{j['id']}/run*/*.png")):
                if open(f, "rb").read() == data:
                    return f"{j['id']} {os.path.basename(os.path.dirname(f))}"
        return None

    rows = []
    for a in items:
        i = a["id"]
        o = original_of(a)
        ref = (o[1] if o[0] == "pck" else f"{o[1]} {o[2]}") if o else "none"
        game = f"{ASSETS}/{i}.png"
        old = img(orig_path(o)) if o and os.path.exists(orig_path(o)) else '<div class="none">no original</div>'
        if os.path.exists(game):
            new = img(game)
            src = "drawn in code (art.py local)" if i in LOCAL else source(i, game) or "edited from a run"
            status = f"in the game; source: {html.escape(src)}"
        else:
            gen = new_art(a, jobs.get(i, []))
            if gen:
                new, status = img(gen[0]), f"not approved; newest run: {html.escape(gen[1])}"
            else:
                new, status = '<div class="none">nothing generated</div>', "not made yet"
        # the job's acceptance checklist (_accept), to tick off against the new art
        accept = [x for j in jobs.get(i, []) for x in j.get("_accept", [])]
        checks = ('<ul class="accept">' + "".join(f"<li>{html.escape(x)}</li>" for x in accept) + "</ul>") if accept else ""
        rows.append(f'<tr><td class="ref"><b>{i}</b><br>{html.escape(a["desc"])}<br>'
                    f'{a["w"]}x{a["h"]}<br>original: {html.escape(ref)}<br>{status}{checks}</td>'
                    f'<td>{old}</td><td>{new}</td></tr>')
    return f'''<!doctype html><meta charset="utf-8"><title>opentrail art</title>
<style>
body{{background:#1e1e22;color:#ddd;font:13px system-ui;margin:16px}}
table{{border-collapse:collapse}}th{{position:sticky;top:0;background:#2a2a30;text-align:left;padding:6px}}
td{{border-top:1px solid #444;padding:8px;vertical-align:top}}
td.ref{{width:300px;color:#aaa;line-height:1.5}}td.ref b{{color:#ffd75e;font-size:14px}}
td.ref ul.accept{{margin:6px 0 0;padding-left:16px;color:#bbb}}td.ref ul.accept li{{margin:2px 0}}
.img,.none{{display:block;width:384px;image-rendering:pixelated;background:repeating-conic-gradient(#3a3a40 0 25%,#2e2e34 0 50%) 0 0/16px 16px}}
.none{{height:216px;display:flex;align-items:center;justify-content:center;background:#2a2a30;color:#777}}
</style>
<table><tr><th>description / information</th><th>original art</th><th>modern art</th></tr>
{"".join(rows)}</table>'''


class Handler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *a, **k):
        super().__init__(*a, directory=ROOT, **k)

    def do_GET(self):
        path = urllib.parse.unquote(urllib.parse.urlparse(self.path).path)
        if path == "/":
            body = page().encode()
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return
        rel = os.path.normpath(path.lstrip("/"))
        if not rel.startswith(SERVED) or ".." in rel.split(os.sep):
            self.send_error(404)
            return
        super().do_GET()

    def log_message(self, *a):
        pass


def cmd_gallery(args):
    page()                                       # fail here, not in the browser
    srv = http.server.ThreadingHTTPServer(("127.0.0.1", args.port), Handler)
    url = f"http://localhost:{srv.server_port}/"
    print(f"art gallery: {url}  (Ctrl-C to stop)")
    if not args.no_open:
        try:
            wsl = "microsoft" in open("/proc/sys/kernel/osrelease").read().lower()
        except OSError:
            wsl = False
        if wsl:                                  # the Windows browser
            subprocess.run(["cmd.exe", "/c", "start", "", url], cwd="/mnt/c",
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        else:
            webbrowser.open(url)
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        pass


# ============================================================================

def cmd_icon(_):
    """The app icon, drawn from the modern art (no generation): the travel
    screen's ox and wagon on its prairie backdrop, the sky deepening upward,
    scaled 6x on whole pixels to 1024 px. Written for iOS
    (ios/Assets.xcassets/AppIcon.appiconset/icon-1024.png), Android's launcher
    (android/res/mipmap-xxxhdpi/ic_launcher.png, 192 px) and Play
    (android/play-assets/icon-512.png). Opaque, as the stores want."""
    bg = Image.open("assets/modern/travel_bg_prairie_grass.png").convert("RGB")
    wg = Image.open("assets/modern/travel_wagon_1.png").convert("RGBA")
    n = 170
    sky, grass = bg.getpixel((5, 0)), bg.getpixel((5, 63))
    c = Image.new("RGB", (n, n), sky)
    for y in range(n):
        t = max(0, 60 - y) / 60.0
        c.paste(Image.new("RGB", (n, 1), tuple(int(v * (1 - 0.35 * t)) for v in sky)), (0, y))
    c.paste(Image.new("RGB", (n, n - 110), grass), (0, 110))
    for x in range(-11, n, bg.width):
        c.paste(bg, (x, 50))
    c.paste(wg, (5, 66), wg)
    big = Image.new("RGB", (1024, 1024), grass)
    big.paste(c.resize((n * 6, n * 6), Image.NEAREST), (2, 2))
    big.paste(Image.new("RGB", (1024, 2), c.getpixel((0, 0))), (0, 0))
    out = [("ios/Assets.xcassets/AppIcon.appiconset/icon-1024.png", 1024),
           ("android/res/mipmap-xxxhdpi/ic_launcher.png", 192),
           ("android/play-assets/icon-512.png", 512)]
    for path, size in out:
        os.makedirs(os.path.dirname(path), exist_ok=True)
        (big if size == 1024 else big.resize((size, size), Image.LANCZOS)).save(path)
        print(f"icon -> {path} ({size} px)")


def main():
    ap = argparse.ArgumentParser(description="OpenTrail's assets and modern-mode art",
                                 formatter_class=argparse.RawDescriptionHelpFormatter, epilog=__doc__)
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("assets").set_defaults(fn=cmd_assets)
    sub.add_parser("icon").set_defaults(fn=cmd_icon)
    p = sub.add_parser("embed"); p.add_argument("c_file"); p.add_argument("files", nargs="*")
    p.add_argument("--art", action="store_true", help="modern mode's pictures (EMBEDDED_ART)")
    p.set_defaults(fn=cmd_embed)
    p = sub.add_parser("gallery"); p.set_defaults(fn=cmd_gallery)
    p.add_argument("--port", type=int, default=8765); p.add_argument("--no-open", action="store_true")
    sub.add_parser("originals").set_defaults(fn=cmd_originals)
    p = sub.add_parser("local"); p.set_defaults(fn=cmd_local)
    p.add_argument("--check", action="store_true", help="also the raft's track over the Columbia")
    p = sub.add_parser("anchors"); p.add_argument("--check", action="store_true"); p.set_defaults(fn=cmd_anchors)
    sub.add_parser("balance").set_defaults(fn=cmd_balance)
    p = sub.add_parser("cost"); p.add_argument("job"); p.set_defaults(fn=cmd_cost)
    sub.add_parser("plan").set_defaults(fn=cmd_plan)
    p = sub.add_parser("run"); p.add_argument("job"); p.set_defaults(fn=cmd_run)
    p.add_argument("--dry-run", action="store_true", help="price only, submit nothing")
    p = sub.add_parser("seam"); p.add_argument("id"); p.add_argument("image"); p.set_defaults(fn=cmd_seam)
    p.add_argument("--seam-width", type=int, default=32); p.add_argument("--estimate", action="store_true")
    p = sub.add_parser("keep"); p.add_argument("inp"); p.add_argument("out", nargs="+"); p.set_defaults(fn=cmd_keep)
    p.add_argument("--magenta", action="store_true")
    p.add_argument("--edge", action="store_true", help="drop the edge-joined background of the corners' colour")
    p = sub.add_parser("check"); p.add_argument("image"); p.add_argument("reference", nargs="?")
    p.set_defaults(fn=cmd_check)
    args = ap.parse_args()
    for k in ("job", "image", "inp", "reference", "c_file"):          # paths given relative to where we ran
        if getattr(args, k, None):
            setattr(args, k, os.path.abspath(getattr(args, k)))
    for k in ("out", "bins"):
        if getattr(args, k, None):
            setattr(args, k, [os.path.abspath(o) for o in getattr(args, k)])
    os.chdir(ROOT)
    args.fn(args)


if __name__ == "__main__":
    main()
