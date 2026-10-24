#!/usr/bin/env python3
"""The store screenshots, from the real game: the harness (build/harness) plays
one game of modern mode by touch at each store size, from a window of the same
shape, and snaps six moments of it as they come, plus the Play feature graphic
drawn from the title art.

    python3 tools/store_shots.py            # all of them, into the listing folders

The six, in the listing's order: on the trail, fording the Kansas River
midstream, the hunt, Chimney Rock, Matt's store, the map. Between them the
play-tester (tools/playtest.py) answers whatever the game asks.

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
import playtest  # noqa: E402
from harness import Harness  # noqa: E402

TARGETS = [                                   # (folder, width, height): landscape, as the game is
    ("android/play-assets/screenshots/phone", 1920, 1080),
    ("android/play-assets/screenshots/tablet", 2560, 1600),
    ("ios/app-store-assets/screenshots/iphone-6.9", 2796, 1290),
    ("ios/app-store-assets/screenshots/ipad-13", 2732, 2048),
]
ORDER = ["trail", "crossing", "hunt", "landmark", "store", "map"]
KANSAS, KEARNEY, CHIMNEY = 1, 3, 4            # landmarks (engine/data.c LM_NAME)
CROSS_AT = 70                                 # frames into the crossing: midstream, the bank going under


def shoot(folder, w, h_px):
    out = os.path.join(ROOT, folder)
    os.makedirs(out, exist_ok=True)
    p = playtest.Player("modern", list(playtest.VARIATIONS)[0], "/tmp/opentrail-store-shots", 1848)
    p.h.close()                               # its own harness, by touch at this size
    p.h = h = Harness(modern=True, seed=1848, size=f"{w // 2}x{h_px // 2}", touch=True)
    got = {}

    def snap(tag):
        path = os.path.join(out, "%02d.png" % (ORDER.index(tag) + 1))
        h.cmd(f"snap {path} {w} {h_px}")
        Image.open(path).convert("RGB").save(path, optimize=True)   # the harness writes it uncompressed
        got[tag] = 1

    for _ in range(4000):
        if len(got) == len(ORDER):
            break
        h.cmd("wait 3000")
        s = h.cmd("state")
        r, g, sc = s.get("req") or {}, s.get("game") or {}, s.get("scene") or {}
        k, lm = r.get("kind"), g.get("landmark")
        if k == "MATT_MENU" and "store" not in got:
            snap("store")
        if k == "LEG" and lm == KEARNEY and "trail" not in got:
            p.answer(s, 0)                    # on the way to Chimney Rock: rolling until it is in sight
            for _ in range(200):
                h.cmd("step 10")
                st = h.cmd("state"); sc2 = st.get("scene") or {}
                if (st.get("req") or {}).get("kind") not in (None, "POLL", "DAY_END", "LEG"):
                    break                     # something happened on the way: it is answered below
                if any(x["id"].startswith("travel_icon") and 130 < x["x"] < 230 for x in sc2.get("sprites", [])) \
                        and not sc2.get("body"):      # the rock on the horizon, ahead of the wagon
                    snap("trail")
                    break
            continue
        if k == "LEG" and "landmark" in got and "map" not in got:
            p.answer(s, 0)                    # leaving Chimney Rock: Return on the trail, the menu
            h.cmd("step 60"); h.cmd("key Return")
            continue
        if k == "RIVER_MENU" and lm == KANSAS and "crossing" not in got:
            p.choose(1)                       # ford it
            h.cmd("until CROSS 5000")
            h.cmd(f"step {CROSS_AT}")
            snap("crossing")
            continue
        if k == "LOOK_AROUND" and lm == CHIMNEY and sc.get("prompt") == "yesno" and "landmark" not in got:
            p.yes()
            h.cmd("step 30")
            snap("landmark")
            continue
        if k == "ACTION_MENU" and "landmark" in got and g.get("on_trail"):
            if "map" not in got:
                p.choose(3); h.cmd("wait")
                snap("map")
                p.cont()
                continue
            if "hunt" not in got:
                p.choose(8); h.cmd("wait"); p.cont()
                h.cmd("until field 900")
                h.cmd("tap 192 120")          # walk him into the field
                for _ in range(120):          # until an animal is well in sight
                    h.cmd("step 5")
                    hs = (h.cmd("state").get("hunt") or {}).get("sprites") or []
                    if any(x.get("kind") == "animal" and 30 < x["box"][0] < 220 for x in hs):   # the original's 280 columns
                        break
                snap("hunt")
                continue
        nid = r.get("n"); nth = p.step_in.get(nid, 0); p.step_in[nid] = nth + 1
        p.answer(s, nth)
    h.close() if hasattr(h, "close") else None
    missing = [t for t in ORDER if t not in got]
    print(f"{folder}: {len(got)} at {w}x{h_px}" + (f", missing {missing}" if missing else ""))
    return not missing


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
    ok = all([shoot(folder, w, h) for folder, w, h in TARGETS])
    feature_graphic()
    sys.exit(0 if ok else 1)
