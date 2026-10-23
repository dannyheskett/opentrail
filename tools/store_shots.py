#!/usr/bin/env python3
"""The store screenshots, from the real game: the harness (build/harness) plays
modern mode by touch to six screens and snapshots each at every store size,
from a window of the same shape (so the canvas is laid out for it), plus the
Play feature graphic drawn from the title art.

    python3 tools/store_shots.py            # all of them, into the listing folders

Writes android/play-assets/screenshots/{phone,tablet}/NN.png,
android/play-assets/feature-graphic-1024x500.png and
ios/app-store-assets/screenshots/{iphone-6.9,ipad-13}/NN.png. Needs a display
for the hidden window (xvfb-run on a server).
"""
import os
import sys

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
from harness import Harness  # noqa: E402

TARGETS = [                                   # (folder, width, height): landscape, as the game is
    ("android/play-assets/screenshots/phone", 1920, 1080),
    ("android/play-assets/screenshots/tablet", 2560, 1600),
    ("ios/app-store-assets/screenshots/iphone-6.9", 2796, 1290),
    ("ios/app-store-assets/screenshots/ipad-13", 2732, 2048),
]


def play(h, *keys):
    for k in keys:
        h.cmd(k if k.split()[0] in ("type", "key") else "key " + k)
        h.cmd("wait")


def answer(h, state):
    """Go on from whatever the game waits on: a choice's first option, No to a
    question, the space bar to a message."""
    pr = state["scene"].get("prompt")
    h.cmd("key 2" if pr == "yesno" else "key 1" if pr == "choose" else "key space")
    h.cmd("wait")


def shoot(folder, w, h_px):
    out = os.path.join(ROOT, folder)
    os.makedirs(out, exist_ok=True)
    h = Harness(modern=True, seed=1848, size=f"{w // 2}x{h_px // 2}", touch=True)
    n = [0]

    def snap():
        n[0] += 1
        path = os.path.join(out, "%02d.png" % n[0])
        h.cmd(f"snap {path} {w} {h_px}")
        Image.open(path).convert("RGB").save(path, optimize=True)   # the harness writes it uncompressed

    h.cmd("wait")
    snap()                                                      # 1: the opening
    h.cmd("key space"); h.cmd("wait")
    play(h, "1", "1", "type Ann", "Return", "type Bo", "Return", "type Cy", "Return",
         "type Di", "Return", "type Ed", "Return", "y", "3", "space", "space", "space", "space")
    snap()                                                      # 2: Matt's store
    play(h, "1", "key 3 Return", "2", "type 800", "Return", "3", "type 5", "Return",
         "4", "type 5", "Return", "5", "type 1", "Return", "type 1", "Return", "type 1", "Return",
         "6", "space", "space")
    play(h, "1")                                                # on the trail
    h.cmd("key space"); h.cmd("step 400")
    snap()                                                      # 3: travelling
    for _ in range(400):                                        # to the Kansas River
        s = h.cmd("state")
        kind = (s.get("req") or {}).get("kind")
        if kind == "RIVER_MENU":
            break
        answer(h, s)
    h.cmd("key 1"); h.cmd("until CROSS 5000"); h.cmd("step 120")
    snap()                                                      # 4: fording the river
    for _ in range(100):                                        # whatever the crossing brings, to the menu
        s = h.cmd("state")
        if (s.get("req") or {}).get("kind") == "ACTION_MENU":
            break
        answer(h, s)
    h.cmd("key 3"); h.cmd("wait")                               # the map
    snap()                                                      # 5: the map
    h.cmd("key space"); h.cmd("wait")
    h.cmd("key 8"); h.cmd("wait"); h.cmd("key space")           # hunting
    h.cmd("until field"); h.cmd("key Return"); h.cmd("step 240")
    snap()                                                      # 6: the hunt
    h.close() if hasattr(h, "close") else None
    print(f"{folder}: {n[0]} at {w}x{h_px}")


def feature_graphic():
    """1024x500: the title picture across it, the logo over its sky."""
    title = Image.open(os.path.join(ROOT, "assets/modern/scene_title.png")).convert("RGB")
    logo = Image.open(os.path.join(ROOT, "assets/modern/ui_logo.png")).convert("RGBA")
    k = 1024 / title.width
    big = title.resize((1024, round(title.height * k)), Image.NEAREST)
    top = (big.height - 500) // 2
    g = big.crop((0, top, 1024, top + 500))
    lg = logo.resize((logo.width * 2, logo.height * 2), Image.NEAREST)
    g.paste(lg, ((1024 - lg.width) // 2, 24), lg)
    path = os.path.join(ROOT, "android/play-assets/feature-graphic-1024x500.png")
    g.save(path, optimize=True)
    print(f"feature graphic -> {path}")


if __name__ == "__main__":
    for folder, w, h in TARGETS:
        shoot(folder, w, h)
    feature_graphic()
