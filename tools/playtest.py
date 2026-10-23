#!/usr/bin/env python3
"""Play whole games through the harness, in either mode and along chosen
paths, and keep a screenshot of every new screen, for looking the game over.

    python3 tools/playtest.py OUT [--mode modern|legacy|both] [--variations V,...]
                              [--jobs J] [--list]

Each run goes to OUT/<mode>/<variation>/: a PNG per new screen (and samples
of travel, the hunt, the crossings and the raft), log.json (each screen's
request and game state, and in modern mode its scene) and sheet_NN.png
contact sheets. OUT/summary.md lists how each run ended; OUT/coverage.md the
request kinds no run reached. --list prints the variations.

The player decides from the engine's state alone (the harness's `state`), so
the same choices drive both modes; only the keys differ (legacy fields end
with Return, and legacy answers follow the order of the stops within a
request). Its default is careful: supplies by the cash, hunting and buying
when food runs low, rivers by their depth, rest when health is poor, the raft
steered by looking ahead at the rocks. A variation changes some of that (see
VARIATIONS). Needs build/harness and a display.
"""
import argparse, hashlib, itertools, json, math, os, random, subprocess, sys
from PIL import Image, ImageDraw

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from harness import Harness

NAMES = ['Ann', 'Bo', 'Cy', 'Di', 'Ed', 'Flo', 'Gus', 'Hal', 'Ida', 'Jo']
AIM_KEYS = [';', 'P', 'O', 'I', 'K', ',', '.', '/']            # the hunt's eight directions, east first, anticlockwise

# The careful default, and the variations: each changes some of these.
DEFAULT = dict(occupation=1, month=2, pace=1, rations=1, yoke=4, food=1200, clothes=10, boxes=30, parts=2,
               river='depth', split1=1, split2=1, dalles=1, hunt=True, rest=True, buy=True, trade='need',
               raft='steer', talk=True, tour=[], advice=False, abandon=None, quit_dialog=False, spend=False,
               epitaph='Rest in peace', replay=False)
VARIATIONS = {
    'v01_banker_april_raft':      dict(),
    'v02_carpenter_may_barlow':   dict(occupation=2, month=3, dalles=2, yoke=3, food=800, clothes=8, boxes=15, parts=1),
    'v03_farmer_march_raft':      dict(occupation=3, month=1, yoke=3, food=600, clothes=5, boxes=10, parts=0),
    'v04_june_always_ford':       dict(month=4, river='ford', dalles=2),
    'v05_always_float':           dict(river='float'),
    'v06_always_ferry':           dict(river='ferry', dalles=2),
    'v07_wait_at_rivers':         dict(river='wait'),
    'v08_other_splits_barlow':    dict(split1=2, split2=2, dalles=2),
    'v09_grueling_meager':        dict(pace=3, rations=2),
    'v10_strenuous_bare_bones':   dict(pace=2, rations=3, dalles=2),
    'v11_no_hunting':             dict(hunt=False, food=1500),
    'v12_menu_tour':              dict(tour=['learn', 'topten', 'sound', 'sound'], dalles=2),
    'v13_advice_screens':         dict(advice=True),
    'v14_poor_party_starves':     dict(occupation=3, food=150, boxes=0, parts=0, clothes=2, hunt=False, buy=False, rest=False, replay=True),
    'v15_abandon_esc_esc':        dict(abandon=3),
    'v16_raft_unsteered':         dict(raft='none'),
    'v17_barlow_without_cash':    dict(dalles=2, spend=True),
    'v18_july_into_winter':       dict(month=5, clothes=0, buy=False),
    'v19_trade_and_talk':         dict(trade='always', dalles=2),
    'v20_quit_dialog':            dict(quit_dialog=True, dalles=2),
}


class Player:
    def __init__(self, mode, name, out, seed):
        self.mode, self.name, self.out = mode, name, out
        self.v = dict(DEFAULT, **VARIATIONS[name])
        os.makedirs(out, exist_ok=True)
        self.R = random.Random(seed)
        self.h = Harness(modern=mode == 'modern', seed=seed * 104729)
        self.W, self.H = (384, 252) if mode == 'modern' else (560, 384)
        self.log, self.seen, self.n = [], set(), 0
        self.tmp = f'/tmp/opentrail-playtest-{os.getpid()}.png'
        self.step_in = {}                 # stops within one request (legacy answers by their order)
        self.matt = 0; self.rested = 0; self.must_trade = False; self.store_done = False
        self.shop = []; self.buying = 0; self.talked = set(); self.landmarks = 0; self.done = set()
        self.tour = list(self.v['tour']); self.started = False
        self.menu_sigs = set(); self.last_sig = None; self.spaced = None   # legacy: the main menu known by its picture

    # -- the harness
    def cmd(self, line): return self.h.cmd(line)
    def key(self, *k): self.cmd('key ' + ' '.join(k))
    def type(self, t): self.cmd('type ' + t)
    def choose(self, n): self.key(str(n)) if self.mode == 'modern' else self.key(str(n), 'Return')
    def yes(self, y=True): self.key('y' if y else 'n') if self.mode == 'modern' else self.key('Y' if y else 'N', 'Return')
    def text(self, t): self.type(t); self.key('Return')
    def cont(self): self.key('space')

    def snap(self, tag, force=False):
        s = self.cmd('state')
        self.cmd(f'snap {self.tmp} {self.W} {self.H}')
        im = Image.open(self.tmp).convert('RGB')
        sig = hashlib.md5(im.tobytes()).hexdigest()
        self.last_sig = sig
        if sig in self.seen and not force: return s
        self.seen.add(sig); self.n += 1
        name = f'{self.n:04d}_{tag}.png'
        im.save(os.path.join(self.out, name), optimize=True)
        sc = s.get('scene') or {}
        self.log.append({'snap': name, 'frame': s['frame'], 'req': s.get('req'), 'game': s.get('game'),
                         'scene': {k: sc.get(k) for k in ('bg', 'title', 'body', 'opts', 'prompt', 'hint', 'overlay', 'pic')}})
        return s

    # -- the hunt: aim at an animal when one is on a line from the hunter, fire
    def hunt(self):
        self.key('Return')
        for t in range(500):
            s = self.cmd('state')
            if 'hunt' not in s: return
            sp = s['hunt']['sprites']
            hu = next(p for p in sp if p['kind'] == 'hunter')
            hx, hy = (hu['box'][0] + hu['box'][2]) / 2, (hu['box'][1] + hu['box'][3]) / 2
            for p in sp:
                if p['kind'] != 'animal': continue
                ax, ay = (p['box'][0] + p['box'][2]) / 2, (p['box'][1] + p['box'][3]) / 2
                ang = math.degrees(math.atan2(hy - ay, ax - hx)) % 360
                d = round(ang / 45) % 8
                if abs((ang - d * 45 + 180) % 360 - 180) < 7:
                    self.key(AIM_KEYS[d]); self.cmd('step 8'); self.key('space'); self.cmd('step 3')
                    break
            else:
                self.cmd('step 4')
            if t % 80 == 0: self.snap('hunt', True)
            if any(p['kind'] == 'dead' for p in sp) and t % 40 == 0: self.snap('hunt_kill', True)

    # -- the raft: look ahead at the rocks and the shores (the rules of finish.c)
    def raft(self):
        def hit(hp, rocks):
            x, y = 84 + 8 * hp, 5 * hp - 6
            x1, y1, x2, y2 = x + 6, y + 18, x + 26, y + 25
            for on, rx, ry in rocks:
                if not on: continue
                x3, y3 = rx + 11, ry + 1
                if not (x2 < x3 or x1 > x3 + 20 or y1 > y3 + 6 or y2 < y3): return True
            return False

        def safe(hp, d, tc, rocks, keys):
            for k in keys:
                rocks = [(on, rx + 8, ry - 4) for on, rx, ry in rocks]
                if k < 0 and hp > 0: d = max(-1, d - 1)
                if k > 0 and hp < 17: d = min(1, d + 1)
                hp += d; tc += 1
                if tc > 205 and hp == 17: return True
                if hp < 1 or hp > 16: return False
                if hit(hp, rocks): return False
            return True

        passes = 0
        while True:
            a = self.cmd('until RAFT_FRAME 900')
            if a.get('stopped') != 'request' or (a.get('req') or {}).get('kind') != 'RAFT_FRAME': return
            passes += 1
            if passes % 12 == 0: self.snap('raft', True)
            if self.v['raft'] == 'none': continue
            r = self.cmd('state')['game']['raft']
            rocks = [tuple(x) for x in r['rocks']]
            hp, d, tc = r['hp'], r['dir'], r['tc']
            best = None
            for seq in itertools.product((0, -1, 1), repeat=8 if tc > 190 else 6):
                if not safe(hp, d, tc, rocks, seq): continue
                h2, d2 = hp, d
                if seq[0] < 0 and h2 > 0: d2 = max(-1, d2 - 1)
                if seq[0] > 0 and h2 < 17: d2 = min(1, d2 + 1)
                h2 += d2
                score = -h2 if tc > 198 else abs(h2 - 9) / 2 + abs(d2) * .3   # the middle lanes; late, the landing
                score += sum(1 for k in seq if k) * .01
                if best is None or score < best[0]: best = (score, seq[0])
            choice = best[1] if best else 0
            if choice < 0: self.key('Left')
            elif choice > 0: self.key('Right')

    # -- one stop's answer, from the request and the game state
    def answer(self, s, nth):
        r = s.get('req') or {}; k = r.get('kind'); g = s.get('game') or {}; v = self.v
        sc = s.get('scene') or {}; modern = self.mode == 'modern'; pr = sc.get('prompt')

        if modern and pr == 'continue' and k not in ('RAFT_INTRO', 'HUNT', 'MUST_TRADE', 'GUIDE_OFFER', 'BARLOW_NO_CASH'):
            return self.cont()                                    # a page of text: Space, whatever the request
        if modern and not g and pr == 'yesno' and 'points are earned' in (sc.get('body') or ''):
            return self.yes()                                     # the Top Ten's scoring pages: see them
        if not g and (k is None or not r.get('active')):        # the main menu, or a page it opened
            menu = pr == 'choose' if modern else (self.last_sig in self.menu_sigs or self.spaced == (self.last_sig, 2))
            if not menu:                                          # a Learn page, the Top Ten, the sound message
                if modern or self.spaced != (self.last_sig, 1):
                    self.spaced = (self.last_sig, 1); return self.cont()
                self.spaced = (self.last_sig, 2); return self.yes(False)   # Space did nothing: a yes/no ("how points are earned?")
            self.menu_sigs.add(self.last_sig); self.spaced = None
            if self.tour: return self.choose({'learn': 2, 'topten': 3, 'sound': 4}[self.tour.pop(0)])
            return self.choose(1)
        if k == 'OCCUPATION':
            if v['advice'] and nth == 0: return self.choose(4)
            return self.choose(v['occupation'])
        if k == 'NAMES':
            if modern: return self.text(self.R.choice(NAMES)) if pr == 'text' else self.yes()
            return self.text(self.R.choice(NAMES)) if nth < 5 else self.yes()
        if k == 'MONTH':
            if v['advice'] and nth == 0: return self.choose(6)
            return self.choose(v['month'])
        if k == 'MATT_MENU':
            if r.get('a') and self.matt < 10: self.matt = 10; return self.choose(2)       # over budget: less food
            if self.matt < 5: self.matt += 1; return self.choose(self.matt)
            return self.choose(6) if modern else self.cont()
        if k == 'MATT_QTY':
            q = {1: v['yoke'], 2: v['food'], 3: v['clothes'], 4: v['boxes'], 5: v['parts']}[r['a']] if self.matt < 10 else 300
            return self.text(str(q))
        if k == 'ACTION_MENU':
            on_trail, fort = g.get('on_trail'), g.get('lm_type') == 1
            food, bullets, cash, health = g.get('food', 0), g.get('bullets', 0), g.get('cash', 0), g.get('health', 0)
            if not on_trail and r.get('a') and nth == 0: self.landmarks += 1
            if v['abandon'] and self.landmarks >= v['abandon']:
                self.snap('abandon', True); self.key('Esc'); self.cmd('step 10'); self.key('Esc'); return
            if v['quit_dialog'] and self.landmarks >= 2 and 'quit' not in self.done:
                self.done.add('quit'); self.cmd('esc'); self.cmd('step 2'); self.snap('quit_dialog', True)
                self.cmd('esc no'); self.cmd('step 2')
            if (v['advice'] or v['pace'] != 1) and 'pace' not in self.done: self.done.add('pace'); return self.choose(4)
            if v['rations'] != 1 and 'rations' not in self.done: self.done.add('rations'); return self.choose(5)
            if self.must_trade: self.must_trade = False; return self.choose(7)
            if v['rest'] and health >= 70 and self.rested < 2: self.rested += 1; return self.choose(6)
            if v['hunt'] and on_trail and food < 400 and bullets >= 10: return self.choose(8)
            if v['buy'] and fort and not on_trail and (food < 800 or bullets < 300 or v['spend']) and cash > 40 and not self.store_done:
                return self.choose(9)
            if v['talk'] and not on_trail and g.get('landmark') not in self.talked:
                self.talked.add(g.get('landmark')); return self.choose(8)
            if v['trade'] == 'always' and not on_trail and ('trade', g.get('landmark')) not in self.done:
                self.done.add(('trade', g.get('landmark'))); return self.choose(7)
            self.rested = 0; self.store_done = False
            if 'supplies' not in self.done: self.done.add('supplies'); return self.choose(2)
            if 'map' not in self.done and self.landmarks >= 2: self.done.add('map'); return self.choose(3)
            return self.choose(1)
        if k == 'STORE_MENU':
            if not self.store_done:                               # a fort: this visit's shopping list
                self.store_done = True
                month = int((g.get('date') or '0/').split('/')[0] or 0)
                self.shop = [x for x, want in ((7, g.get('food', 0) < 800 or v['spend']), (3, g.get('bullets', 0) < 300),
                                               (2, month >= 8 and g.get('clothes', 0) < 10)) if want]
            if self.shop: self.buying = self.shop.pop(0); return self.choose(self.buying)
            return self.choose(8)
        if k == 'STORE_QTY':
            cash = g.get('cash', 0)
            q = {7: int(cash / .5) if v['spend'] else max(100, 1000 - g.get('food', 0)), 3: 10, 2: 5}.get(self.buying, 1)
            return self.text(str(q))
        if k == 'PACE':
            if v['advice'] and nth == 0: return self.choose(4)
            return self.choose(v['pace'])
        if k == 'RATIONS': return self.choose(v['rations'])
        if k == 'REST_ASK': return self.text('3')
        if k == 'MUST_TRADE': self.must_trade = True; return self.cont()
        if k == 'TRADE_OFFER': return self.yes(v['trade'] == 'always' or self.must_trade or 'food' not in (r.get('text') or ''))
        if k == 'RIVER_MENU':
            extra = r.get('a') or 0                               # 1 a guide, 2 a ferry: option 3
            wait, info = 3 + (extra > 0), 4 + (extra > 0)
            if v['advice'] and 'info' not in self.done: self.done.add('info'); return self.choose(info)
            pol, depth = v['river'], g.get('river_depth', 3)
            if pol == 'wait' and ('wait', g.get('landmark')) not in self.done:
                self.done.add(('wait', g.get('landmark'))); return self.choose(wait)
            if pol == 'ford': return self.choose(1)
            if pol == 'float': return self.choose(2)
            no_guide = extra == 1 and ('noguide', g.get('landmark')) in self.done
            if pol == 'ferry' and extra and not no_guide: return self.choose(3)
            if depth < 2.5: return self.choose(1)
            return self.choose(3 if extra and not no_guide else 2)
        if k == 'GUIDE_OFFER' and not r.get('b'):                # not enough clothing for the guide
            self.done.add(('noguide', g.get('landmark'))); return self.cont()
        if k in ('FERRY_OFFER', 'GUIDE_OFFER', 'GRAVE_ASK', 'PART_ASK', 'BARLOW_ASK', 'LOOK_AROUND'):
            if (modern and pr != 'yesno') or (not modern and nth > 0): return self.cont()   # the message after the answer
            return self.yes()
        if k == 'SPLIT':
            first = g.get('landmark', 0) < 10                     # South Pass; else the Blue Mountains
            return self.choose(v['split1'] if first else v['split2'])
        if k == 'DALLES': return self.choose(1 if 'no_cash' in self.done else v['dalles'])
        if k == 'BARLOW_NO_CASH': self.done.add('no_cash'); return self.cont()
        if k == 'RAFT_INTRO' and nth == 1: self.cont(); return self.raft()
        if k == 'HUNT':
            self.cont()
            if self.cmd('until field 900').get('stopped') == 'request': self.snap('hunt_start', True); self.hunt()
            return
        if k == 'TOMB_FINAL':
            if modern:
                if pr == 'text': return self.text(v['epitaph'])
                if pr == 'yesno': return self.yes('make changes' not in (sc.get('body') or ''))   # write one; no changes
                return self.cont()
            # the tune, "write an epitaph?" yes, the epitaph, "make changes?" no
            return [self.cont, self.yes, lambda: self.text(v['epitaph']), lambda: self.yes(False)][min(nth, 3)]()
        if k == 'TOPTEN_NAME':
            if (modern and pr == 'yesno') or (not modern and nth % 2): return self.yes(False)   # "make any changes?" no
            return self.text(self.R.choice(NAMES))
        if modern:
            if pr == 'choose': return self.choose(sc.get('opt_base', 1))
            if pr == 'yesno': return self.yes()
            if pr == 'text': return self.text('1')
        self.cont()

    def play(self, limit=8000):
        ended = None
        if self.mode == 'legacy':
            self.cmd('step 300'); self.cmd('wait'); self.snap('menu', True); self.menu_sigs.add(self.last_sig)
        for step in range(limit):
            a = self.cmd('wait 3000')
            s = self.cmd('state'); r = s.get('req') or {}; g = s.get('game')
            if g: self.started = True
            if self.started and not g and not self.tour and a.get('waiting'):
                self.snap('back_at_menu', True); ended = 'menu'
                if self.v['replay']:                              # a second game, past the first party's grave
                    self.v = dict(DEFAULT, epitaph='Here lies a poor party'); self.started = False
                    self.matt = 0; self.done = set(); self.talked = set(); self.landmarks = 0
                    self.choose(1); continue
                break
            if r.get('kind') == 'RAFT_FRAME': self.raft(); continue
            nid = r.get('n'); nth = self.step_in.get(nid, 0); self.step_in[nid] = nth + 1
            if nth > 40: ended = 'stuck'; self.snap('STUCK', True); break
            self.snap((r.get('kind') or 'menu').lower())
            if r.get('kind') == 'CROSS':
                for _ in range(4): self.cmd('step 25'); self.snap('cross', True)
            self.answer(s, nth)
            if g and r.get('kind') in ('LEG', 'EVENT_END', 'HUNT_RESULT', 'ACTION_MENU', 'LOOK_AROUND'):
                b = None
                for _ in range(4):                                # travel a few days, then look at the menu
                    b = self.cmd('until DAY_END 400')
                    if b.get('stopped') != 'request': break
                if b and b.get('stopped') == 'request':
                    self.snap('travel', True); self.key('Return')
        kinds = sorted({(e['req'] or {}).get('kind') or 'menu' for e in self.log})
        final = [e for e in self.log if e.get('game')]
        summary = {'mode': self.mode, 'variation': self.name, 'end': ended or 'limit', 'screens': self.n,
                   'arrived': any((e['req'] or {}).get('kind') == 'ARRIVE' for e in self.log),
                   'died': any((e['req'] or {}).get('kind') == 'ALL_DEAD' for e in self.log),
                   'last_game': final[-1]['game'] if final else None, 'kinds': kinds}
        json.dump({'summary': summary, 'log': self.log}, open(os.path.join(self.out, 'log.json'), 'w'), indent=1)
        sheets(self.out, self.log, self.W, self.H)
        self.h.close()
        return summary


def sheets(out, log, w, h, per=12):
    """Contact sheets: the first screen of each kind, and up to six of the busy kinds."""
    picked, count = [], {}
    for e in log:
        kind = e['snap'].split('_', 1)[1][:-4]
        count[kind] = count.get(kind, 0) + 1
        if count[kind] == 1 or (kind in ('event_end', 'travel', 'look_around', 'cross', 'raft', 'hunt') and count[kind] <= 6):
            picked.append(e)
    tw, th = (384, 252) if w == 384 else (420, 288)
    for i in range(0, len(picked), per):
        part = picked[i:i + per]; rows = (len(part) + 2) // 3
        s = Image.new('RGB', (3 * (tw + 6), rows * (th + 16)), (30, 30, 34)); d = ImageDraw.Draw(s)
        for j, e in enumerate(part):
            x, y = (j % 3) * (tw + 6), (j // 3) * (th + 16)
            s.paste(Image.open(os.path.join(out, e['snap'])).convert('RGB').resize((tw, th)), (x, y))
            d.text((x + 2, y + th + 2), e['snap'], fill=(255, 220, 120))
        s.save(os.path.join(out, f'sheet_{i // per:02d}.png'))


ALL_KINDS = ['OCCUPATION', 'NAMES', 'MONTH', 'MATT_INTRO', 'MATT_MENU', 'MATT_QTY', 'MATT_MSG', 'MATT_DEPART',
             'LOOK_AROUND', 'ACTION_MENU', 'MUST_TRADE', 'SUPPLIES', 'MAP', 'PACE', 'RATIONS', 'REST_ASK', 'REST_DATE',
             'TALK', 'STORE_MENU', 'STORE_QTY', 'STORE_MSG', 'TRADE_SHOW', 'TRADE_NOBODY', 'TRADE_LACK', 'TRADE_OFFER',
             'HUNT', 'HUNT_RESULT', 'SPLIT', 'LEG', 'DAY_END', 'EVENT_SHOW', 'EVENT_END', 'GRAVE_ASK', 'GRAVE_SHOW',
             'PART_ASK', 'PART_MSG', 'NO_OXEN', 'TOMB_FINAL', 'ALL_DEAD', 'RIVER_INTRO', 'RIVER_MENU', 'RIVER_INFO',
             'RIVER_CAMP', 'RIVER_CAMP_DATE', 'RIVER_MSG', 'FERRY_OFFER', 'GUIDE_OFFER', 'GUIDE_GO', 'CROSS', 'DALLES',
             'BARLOW_ASK', 'BARLOW_NO_CASH', 'RAFT_INTRO', 'RAFT_FRAME', 'RAFT_HIT', 'ARRIVE', 'TOPTEN_NAME', 'TOPTEN_SHOW']


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('out'); ap.add_argument('--mode', default='both', choices=('modern', 'legacy', 'both'))
    ap.add_argument('--variations', default=','.join(VARIATIONS)); ap.add_argument('--jobs', type=int, default=4)
    ap.add_argument('--list', action='store_true'); ap.add_argument('--one', nargs=2, help=argparse.SUPPRESS)
    a = ap.parse_args()
    if a.list:
        for k, v in VARIATIONS.items(): print(f'{k:28s} {v}')
        return
    if a.one:
        mode, name = a.one
        seed = list(VARIATIONS).index(name) + 1
        print(json.dumps(Player(mode, name, os.path.join(a.out, mode, name), seed).play())); return
    modes = ['modern', 'legacy'] if a.mode == 'both' else [a.mode]
    runs = [(m, v) for v in a.variations.split(',') for m in modes]
    results, procs = [], []
    while runs or procs:
        while runs and len(procs) < a.jobs:
            m, v = runs.pop(0)
            procs.append((m, v, subprocess.Popen([sys.executable, __file__, a.out, '--one', m, v], stdout=subprocess.PIPE, text=True)))
        m, v, p = procs.pop(0)
        out = p.communicate()[0].strip().splitlines()
        try: results.append(json.loads(out[-1]))
        except (IndexError, ValueError): results.append({'mode': m, 'variation': v, 'end': 'crashed', 'kinds': []})
    seen = set()
    with open(os.path.join(a.out, 'summary.md'), 'w') as f:
        f.write('| mode | variation | end | arrived | died | screens | last date | miles | at |\n|---|---|---|---|---|---|---|---|---|\n')
        for r in sorted(results, key=lambda r: (r['variation'], r['mode'])):
            g = r.get('last_game') or {}; seen.update(r.get('kinds', []))
            f.write(f"| {r['mode']} | {r['variation']} | {r.get('end')} | {r.get('arrived')} | {r.get('died')} | {r.get('screens')} | "
                    f"{g.get('date')} | {g.get('miles')} | {g.get('landmark_name')} |\n")
    with open(os.path.join(a.out, 'coverage.md'), 'w') as f:
        f.write('Request kinds no run reached: ' + (', '.join(k for k in ALL_KINDS if k not in seen) or 'none') + '\n')
    print(open(os.path.join(a.out, 'summary.md')).read()); print(open(os.path.join(a.out, 'coverage.md')).read())


if __name__ == '__main__':
    main()
