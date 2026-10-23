#!/usr/bin/env python3
"""test_harness: the harness (build/harness) plays both modes through its
commands, its state matches what was typed, and snapshots show the screens.

Modern: a new game to the trail and into the hunt; the scene lists the hunt's
sprites and the hunt field the hunter and the bullets. Legacy: the title to
the menu. Needs a display for the hidden window; without one it says SKIP.
"""
import os, struct, sys, tempfile, zlib
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'tools'))
from harness import Harness

fails = 0


def check(cond, what):
    global fails
    print(('PASS: ' if cond else 'FAIL: ') + what)
    if not cond: fails += 1


def png_colours(path):
    """The number of distinct colours in a harness PNG (stored deflate, RGBA)."""
    data = open(path, 'rb').read()
    w, h = struct.unpack('>II', data[16:24])
    idat, i = b'', 8
    while i < len(data):
        n = struct.unpack('>I', data[i:i + 4])[0]
        if data[i + 4:i + 8] == b'IDAT': idat += data[i + 8:i + 8 + n]
        i += 12 + n
    raw = zlib.decompress(idat)
    seen = set()
    for y in range(0, h, 4):
        row = raw[y * (w * 4 + 1) + 1:(y + 1) * (w * 4 + 1)]
        for x in range(0, w * 4, 16): seen.add(row[x:x + 3])
    return len(seen)


def play(h, *steps):
    """Keys, each followed by a wait; returns the last answer."""
    a = None
    for s in steps:
        r = h.cmd(s if ' ' in s and s.split()[0] in ('type', 'key') else 'key ' + s)
        assert r['ok'], r
        a = h.cmd('wait')
    return a


def modern(tmp):
    h = Harness(modern=True, seed=1848)
    a = h.cmd('wait')
    s = h.cmd('state')['scene']
    check(s['bg'] == 'scene_title' and s['overlay'] == 'ui_logo' and s['hint'] == 'Press any key to begin' and a['frame'] > 120,
          'modern: the opening: the logo, then the picture, then a key')
    h.cmd('key space')
    a = h.cmd('wait')
    check(h.cmd('state')['scene']['opts'][0] == 'Travel the trail', 'modern: the menu after the key')
    a = play(h, '1', '1', 'type Ann', 'Return', 'type Bo', 'Return', 'type Cy', 'Return',
             'type Di', 'Return', 'type Ed', 'Return', 'y', '3', 'space', 'space', 'space', 'space')
    check(a['req']['kind'] == 'MATT_MENU', 'modern: names and month typed, at Matt\'s store')
    a = play(h, '1', 'key 3 Return', '4', 'key 9 Return', '6', 'space', 'space')
    s = h.cmd('state')
    check(a['req']['kind'] == 'ACTION_MENU' and s['game']['oxen'] == 6 and s['game']['bullets'] == 180
          and s['game']['party'] == ['Ann', 'Bo', 'Cy', 'Di', 'Ed'],
          'modern: bought oxen and bullets; the engine\'s state agrees')
    play(h, '1')
    h.cmd('key space'); h.cmd('step 200'); h.cmd('key Return')
    a = h.cmd('until ACTION_MENU 20000'); h.cmd('wait')
    check(a['stopped'] == 'request', 'modern: travelled, back at the action menu')
    h.cmd('key 8'); h.cmd('wait'); h.cmd('key space')
    a = h.cmd('until field')
    h.cmd('key Return'); h.cmd('step 90')
    s = h.cmd('state')
    hunt = s.get('hunt') or {}
    kinds = [p['kind'] for p in hunt.get('sprites', [])]
    ids = [p['id'] for p in s['scene']['sprites']]
    check(a['stopped'] == 'request' and 'hunter' in kinds and hunt.get('bullets') == 180,
          'modern: the hunt is on the field: the hunter, 180 bullets')
    check(s['scene']['bg'].startswith('scene_hunt_zone') and any(i.startswith('hunt_hunter_') for i in ids)
          and len(ids) == len(kinds), 'modern: one scene sprite per thing on the field')
    check(all('anchor' in p for p in s['scene']['sprites']), 'modern: every hunt sprite stands by its feet (hunt_anchor.h)')
    p = os.path.join(tmp, 'hunt.png')
    r = h.cmd('snap %s 384 252' % p)
    check(r['ok'] and png_colours(p) > 50, 'modern: the hunt snapshot shows the field')
    h.close()


def legacy(tmp):
    h = Harness(modern=False, seed=1848)
    h.cmd('step 300')
    a = h.cmd('wait')
    check(a['stopped'] == 'waiting', 'legacy: past the title, waiting at the menu')
    p = os.path.join(tmp, 'menu.png')
    h.cmd('snap %s 560 384' % p)
    check(png_colours(p) >= 2, 'legacy: the menu snapshot')
    h.close()


if __name__ == '__main__':
    if not (os.environ.get('DISPLAY') or os.environ.get('WAYLAND_DISPLAY')):
        print('SKIP: test_harness (no display for the hidden window)'); sys.exit(0)
    with tempfile.TemporaryDirectory() as tmp:
        modern(tmp)
        legacy(tmp)
    sys.exit(1 if fails else 0)
