// A headless host for engine tests: answers every Req from a per-kind queue of
// scripted answers, falling back to a simple autoplay policy, and logs each
// request. The RNG can be fed a script of values before it falls back to the
// built-in generator.
#ifndef OT_SCRIPT_IO_H
#define OT_SCRIPT_IO_H

#include "game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SCRIPT_MAX 64
#define LOG_MAX 4096

typedef struct {
    int answers[REQ_TOPTEN_SHOW + 1][SCRIPT_MAX];
    int nans[REQ_TOPTEN_SHOW + 1], used[REQ_TOPTEN_SHOW + 1];
    int count[REQ_TOPTEN_SHOW + 1];         // requests seen per kind
    long total, limit;                      // abort a runaway game
    char log[LOG_MAX][96];                  // "KIND text"
    int nlog;
    int hunt_pounds;                        // REQ_HUNT answer
    const char *leader;
    int matt_plan[5];                       // yokes, food, clothes, boxes, each part
    int restock;                            // autoplay buys food at forts and hunts when low
    int store_step;
} ScriptIO;

typedef struct {
    double v[256];
    int n, i;
    unsigned long long fallback;
} ScriptRnd;

static double script_rnd(void *state) {
    ScriptRnd *r = state;
    if (r->i < r->n) return r->v[r->i++];
    unsigned long long z = (r->fallback += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    z ^= z >> 31;
    return (double)(z >> 11) / 9007199254740992.0;
}

static void rnd_push(ScriptRnd *r, double v) { r->v[r->n++] = v; }

static void script_init(ScriptIO *s) {
    memset(s, 0, sizeof *s);
    s->limit = 200000;
    s->leader = "Leader";
    int plan[5] = { 3, 1000, 10, 10, 1 };
    memcpy(s->matt_plan, plan, sizeof plan);
    s->hunt_pounds = 200;
    s->restock = 1;
}

static void script_push(ScriptIO *s, ReqKind k, int answer) {
    s->answers[k][s->nans[k]++] = answer;
}

static int script_has(const ScriptIO *s, const char *needle) {
    for (int i = 0; i < s->nlog; i++) if (strstr(s->log[i], needle)) return 1;
    return 0;
}

// The fallback policy: a cautious but uninspired traveller.
static int autoplay(ScriptIO *s, Game *g, Req *r) {
    switch (r->kind) {
    case REQ_OCCUPATION: return 1;
    case REQ_NAMES: snprintf(r->names[0], OT_NAME_MAX, "%s", s->leader); return 0;
    case REQ_MONTH: return 2;
    case REQ_MATT_MENU: {
        const Matt *m = &g->matt;
        if (!m->yokes) return 1;
        if (!m->food) return 2;
        if (!m->clothing) return 3;
        if (!m->boxes) return 4;
        if (!m->parts[0] && s->matt_plan[4]) return 5;
        return 0;
    }
    case REQ_MATT_QTY: return r->a == 5 ? s->matt_plan[4] : s->matt_plan[r->a - 1];
    case REQ_LOOK_AROUND:
        return s->restock && LM_TYPE[g->lm] == 1 && g->inv[INV_FOOD] < 700 && g->my_cents >= 2000;
    case REQ_ACTION_MENU:
        if (g->b == 2 || g->b >= 5) return 7;       // trade for what we lack
        if (s->restock && !g->ll && LM_TYPE[g->lm] == 1 && g->inv[INV_FOOD] < 700 && g->my_cents >= 2000 &&
            s->store_step == 0) { s->store_step = 1; return 9; }
        if (s->restock && g->ll && g->inv[INV_FOOD] < 250 && g->inv[INV_BULLETS] >= 20) return 8;
        s->store_step = 0;
        return 1;
    case REQ_STORE_MENU:
        if (s->store_step == 1) { s->store_step = 2; return 7; }
        return 8;
    case REQ_STORE_QTY: {
        long price = store_price_cents(r->a, g->lm);
        long want = 1000 - (long)g->inv[INV_FOOD], afford = g->my_cents / price;
        return (int)(want < afford ? want : afford);
    }
    case REQ_TRADE_OFFER: return 1;
    case REQ_HUNT: r->a = s->hunt_pounds; return 0;
    case REQ_SPLIT: return 1;
    case REQ_GRAVE_ASK: return 1;
    case REQ_PART_ASK: return 1;
    case REQ_TOMB_FINAL: snprintf(r->out, r->outlen, "RIP"); return 0;
    case REQ_RIVER_MENU:
        if (r->a == 2 && g->rd >= 2.5) return 3;                // ferry
        if (r->a == 1 && g->inv[INV_CLOTHES] >= 3) return 3;    // guide
        return g->rd < 2.5 ? 1 : 2;                             // ford, else caulk
    case REQ_FERRY_OFFER: return 1;
    case REQ_GUIDE_OFFER: return r->b;
    case REQ_DALLES: return g->my_cents > 500 + (long)(g->inv[INV_OXEN] + .5) * 50 ? 2 : 1;
    case REQ_BARLOW_ASK: return 1;
    case REQ_TOPTEN_NAME: snprintf(r->out, r->outlen, "Tester"); return 1;
    default: return 0;
    }
}

static const char *const KIND_NAME[] = {
    "GOSUB", "OCCUPATION", "NAMES", "MONTH", "MATT_INTRO", "MATT_MENU", "MATT_QTY",
    "MATT_MSG", "MATT_DEPART", "LOOK_AROUND", "ACTION_MENU", "MUST_TRADE", "SUPPLIES",
    "MAP", "PACE", "RATIONS", "REST_ASK", "REST_DATE", "TALK", "STORE_MENU", "STORE_QTY",
    "STORE_MSG", "TRADE_SHOW", "TRADE_NOBODY", "TRADE_LACK", "TRADE_OFFER", "HUNT",
    "HUNT_RESULT", "SPLIT", "LEG", "POLL", "DAY_END", "EVENT_SHOW", "EVENT_END",
    "GRAVE_ASK", "GRAVE_SHOW", "PART_ASK", "PART_MSG", "NO_OXEN", "TOMB_FINAL",
    "ALL_DEAD", "FLIP", "SAVE", "ERROR", "RIVER_INTRO", "RIVER_MENU", "RIVER_INFO",
    "RIVER_CAMP", "RIVER_CAMP_DATE", "RIVER_MSG", "FERRY_OFFER", "GUIDE_OFFER",
    "GUIDE_GO", "CROSS", "DALLES", "BARLOW_ASK", "BARLOW_PAID", "BARLOW_NO_CASH", "RAFT_INTRO",
    "RAFT_FRAME", "RAFT_HIT", "RAFT_LAND", "ARRIVE", "TOPTEN_NAME", "TOPTEN_SHOW",
};

static int script_io(void *ctx, Game *g, Req *r) {
    ScriptIO *s = ctx;
    if (++s->total > s->limit) {
        fprintf(stderr, "FAIL: runaway game (%ld requests) LM %d NM %d D %.1f oxen %.1f B %d NP %d AS %.1f %d/%d/%d\n",
                s->total, g->lm, g->nm, g->d, g->inv[INV_OXEN], g->b, g->np, g->as, g->am, g->ad, g->ay);
        for (int i = s->nlog > 12 ? s->nlog - 12 : 0; i < s->nlog; i++) fprintf(stderr, "  %s\n", s->log[i]);
        exit(1);
    }
    s->count[r->kind]++;
    if (r->kind != REQ_DAY_END && r->kind != REQ_POLL && r->kind != REQ_GOSUB &&
        r->kind != REQ_RAFT_FRAME && s->nlog < LOG_MAX)
        snprintf(s->log[s->nlog++], 96, "%s %s%s%s", KIND_NAME[r->kind], r->text ? r->text : "",
                 r->text2 ? " / " : "", r->text2 ? r->text2 : "");
    if (s->used[r->kind] < s->nans[r->kind]) {
        int v = s->answers[r->kind][s->used[r->kind]++];
        if (r->kind == REQ_HUNT) { r->b = r->a; r->a = v; return 0; }
        return v;
    }
    if (r->kind == REQ_HUNT) r->b = r->a;           // bullets unchanged unless scripted
    return autoplay(s, g, r);
}

#endif
