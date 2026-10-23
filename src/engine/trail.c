// OREGON TRAIL (the main program) with the overlays it appends:
// PACE, RATION, TALK, BUY, TRADE, HUNT, PART, LF, TOMB, FLIP.
// Line numbers in comments are OREGON TRAIL's unless another file is named.
#include "engine.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static void menu4000(Game *g);
static void day3100(Game *g);

// ---------------------------------------------------------------------------
// Message boxes
// ---------------------------------------------------------------------------
// 700: a box that stays up (F9 clears it the next day, 3170).
static void box700(Game *g, const char *text, int art) {
    ASK(g, .kind = REQ_EVENT_SHOW, .a = EV_TOAST, .b = art, .text = text);
}

// 710: box, wait, restore. G (the box's row offset) is passed in c.
static void box710(Game *g, const char *text, int art, int goff) {
    ASK(g, .kind = REQ_EVENT_SHOW, .a = EV_BOX, .b = art, .c = goff, .text = text);
    int ret = ASK(g, .kind = REQ_EVENT_END, .a = EV_BOX);
    if (!g->b) g->b = ret && !g->sd;
}

// 900: text and the loss lines T$(0..Z-1).
void list_box(Game *g, const char *text) {
    ASK(g, .kind = REQ_EVENT_SHOW, .a = EV_LIST, .text = text,
        .lines = (const char (*)[OT_LINE_MAX])g->t, .nlines = g->nt);
    g->b = ASK(g, .kind = REQ_EVENT_END, .a = EV_LIST);
}

// 550-570: "A$.  Lose n day(s)." and the lost days run as rest days.
static void lose_days(Game *g, const char *a, int z, int art) {
    char v[96], num[16];
    int xx = (int)floor(RND(g) * z + 1);
    snprintf(v, sizeof v, "%s.  Lose %s day%s.", a, a2_str(xx, num), xx > 1 ? "s" : "");
    GOSUB(g, 355);
    ASK(g, .kind = REQ_EVENT_SHOW, .a = EV_LOSE_DAYS, .b = art, .text = v);
    g->sd = xx;
    rest500(g);
    GOSUB(g, 400);
    int ret = ASK(g, .kind = REQ_EVENT_END, .a = EV_LOSE_DAYS);
    GOSUB(g, 350);
    g->b = ret;
}

// ---------------------------------------------------------------------------
// Rest days
// ---------------------------------------------------------------------------
// 500: event penalties. D and P are parked so the days neither move nor tire.
void rest500(Game *g) {
    if (!g->sd) return;
    double zx = g->d, zy = g->m;
    int di = g->p;
    g->d = 0; g->p = 0;
    for (int k1 = 1; k1 <= g->sd; k1++) { GOSUB(g, 500); day3100(g); }   // INVERSE: GOSUB 3100
    g->sd = 0;
    g->d = zx; g->m = zy;
    g->inv[INV_FOOD] = g->pf;
    g->p = di;
}

// 4505: rest from the menu (X5 = 9) or waiting for the ferry (X5 = 18).
void rest4505(Game *g) {
    if (!g->sd) return;
    g->f9 = 0;
    int jq = g->p;
    double zx = g->d, zy = g->m;
    g->p = 0; g->d = 0;
    for (int k1 = 1; k1 <= g->sd; k1++) {
        day3100(g);
        ASK(g, .kind = REQ_REST_DATE, .a = g->x5);
    }
    g->sd = 0;
    g->d = zx; g->m = zy;
    g->p = jq;
}

// ---------------------------------------------------------------------------
// Party
// ---------------------------------------------------------------------------
// 11500-11505: pick "someone". The leader only when alone.
static int pick(Game *g) {
    int z = (int)floor(RND(g) * g->np);
    for (int guard = 0; guard < 1000; guard++) {
        z = (z + 1) * (z < g->np - 1);
        if (!(g->h1[z] < 0)) break;
    }
    return z + (g->np > 1) * (z == 0);
}

// TOMB.LIB:50000-50040, reached through 8000 (which then runs 650).
void death(Game *g, int q) {
    if (g->h > 105) g->h = 105;
    g->np = g->np - 1;
    g->h1[q] = g->h1[g->np]; g->h1[g->np] = -1;
    g->h2[q] = g->h2[g->np];
    char z[OT_NAME_MAX];
    memcpy(z, g->name[q], OT_NAME_MAX);
    memcpy(g->name[q], g->name[g->np], OT_NAME_MAX);
    memcpy(g->name[g->np], z, OT_NAME_MAX);
    if (g->np) return;

    // 50010: the whole party is gone.
    g->np = q;
    char epitaph[OT_EPITAPH_MAX] = "";
    ASK(g, .kind = REQ_TOMB_FINAL, .text = g->name[q], .out = epitaph, .outlen = 30);
    tomb_write(g, epitaph);                                     // 50035
    ASK(g, .kind = REQ_ALL_DEAD);                               // 50040
    game_over(g, OUTCOME_DIED);
}

static void death8000(Game *g, int q) { death(g, q); recompute(g); }

// 21000: no oxen.
static void oxen_check(Game *g) {
    if (!g->inv[INV_OXEN]) {
        ASK(g, .kind = REQ_NO_OXEN);
        g->b = 2;
    }
}

// ---------------------------------------------------------------------------
// Loss lists (RIVER.LIB / FLOAT 50175-50260, LF.LIB 53000)
// ---------------------------------------------------------------------------
static void add_line(Game *g, const char *s) {
    if (g->nt < OT_LOSS_MAX) snprintf(g->t[g->nt], OT_LINE_MAX, "%.*s", OT_LINE_MAX - 1, s);
    g->nt++;
}

static void add_qty(Game *g, int item, double qty, int trade_rules) {
    char name[40], num[16], line[OT_LINE_MAX + 16];
    a2_item_name(item, qty, trade_rules, name, sizeof name);
    snprintf(line, sizeof line, "%s %s", a2_str(qty, num), name);
    add_line(g, line);
}

// 50205: each of clothing .. food held is hit with probability V.
void loss_items(Game *g, double v) {
    for (int l = 3; l <= 8; l++) {
        double x = g->inv[l];
        int hit = RND(g) < v;                   // both sides of AND are evaluated
        if (x && hit) {
            double y = floor(RND(g) * x + 1);
            g->inv[l] = x - y;
            add_qty(g, l, y, 0);
        }
    }
}

// 50185: each ox is lost with probability V.
void loss_oxen(Game *g, double v) {
    double x = g->inv[INV_OXEN];
    int y = 0;
    if (!x) return;
    for (double l = 1; ; l++) {                 // FOR L = 1 TO X runs at least once
        if (RND(g) < v) y++;
        if (l + 1 > x) break;
    }
    if (y) {
        char line[OT_LINE_MAX], num[16];
        snprintf(line, sizeof line, "%s oxen", a2_str(y, num));
        if (y == 1) line[strlen(line) - 2] = 0;             // "1 ox"
        add_line(g, line);
        g->inv[INV_OXEN] = x - y;
        if (g->inv[INV_OXEN] < 0) g->inv[INV_OXEN] = 0;
    }
}

// 50175: members drown with probability V; the leader only when alone.
void loss_drown(Game *g, double v) {
    for (int l = (g->np > 1); ; l++) {
        if (RND(g) < v) {
            char line[OT_LINE_MAX];
            snprintf(line, sizeof line, "%s (drowned)", g->name[l]);
            add_line(g, line);
            g->h1[l] = -2;
            v = v * (g->nt < 11);
        }
        if (l + 1 > g->np - 1) break;
    }
}

// ---------------------------------------------------------------------------
// Random events (10000-11420)
// ---------------------------------------------------------------------------
static void illness10300(Game *g) {
    if (!g->sd) GOSUB(g, 355);
    g->hr = 20;
    int v = (int)floor(RND(g) * 6) + 3;
    int z = pick(g);
    char msg[64];
    if (g->h1[z]) {
        snprintf(msg, sizeof msg, "%s has died.", g->name[z]);
        g->h1[z] = -1;
        box710(g, msg, ART_NONE, 20);
        death8000(g, z);
        if (!g->sd) GOSUB(g, 400);
        return;
    }
    snprintf(msg, sizeof msg, "%s has %s.", g->name[z], ILLNESS[v]);   // 10310
    box710(g, msg, ART_NONE, 20);
    g->h1[z] = v; g->h2[z] = 10;
    if (!g->sd) GOSUB(g, 400);
}

// PART.LIB:42000-42100
static void broken_part(Game *g) {
    const char *part = STORE_UNIT[g->b - 2];
    char msg[128];
    int yes = ASK(g, .kind = REQ_PART_ASK, .a = g->b);
    int lucky = RND(g) < .5;
    if (yes && lucky) {
        snprintf(msg, sizeof msg, "You were able to repair the wagon %s.", part);
        g->b = ASK(g, .kind = REQ_PART_MSG, .a = PART_MSG_REPAIRED, .text = msg);
        return;
    }
    const char *a = yes ? "were unable to" : "did not";
    snprintf(msg, sizeof msg, "You %s repair the broken wagon %s.  You must replace it with a spare part.",
             a, part);
    double z = g->inv[g->b];
    if (z) {
        g->inv[g->b] = z - 1;
        g->b = ASK(g, .kind = REQ_PART_MSG, .a = PART_MSG_SPARE, .text = msg);
        return;
    }
    ASK(g, .kind = REQ_PART_MSG, .a = PART_MSG_NO_SPARE, .text = msg);
    snprintf(msg, sizeof msg, "Since you don't have a spare %s, you must trade for one.", part);
    ASK(g, .kind = REQ_PART_MSG, .a = PART_MSG_TRADE, .text = msg);
}

// LF.LIB:50000 fire
static void fire(Game *g) {
    g->nt = 0;
    for (int y = 3; y <= 7; y++) {
        double yy = g->inv[y];
        int hit = RND(g) < .5;
        if (hit && yy) {
            double x = floor(RND(g) * yy + 1);
            g->inv[y] = yy - x;
            add_qty(g, y, x, 0);
        }
    }
    double yy = g->pf;
    int hit = RND(g) < .5;                      // IF YY AND RND(1) < .5: both evaluated
    if (yy && hit) {
        double x = floor(RND(g) * yy + 1);
        g->pf = yy - x;
        g->inv[INV_FOOD] = g->pf;
        add_qty(g, 8, x, 0);
    }
}

// LF.LIB:51000 abandoned wagon
static void abandoned(Game *g, char *a, size_t n) {
    g->nt = 0;
    for (int y = 3; y <= 7; y++) {
        if (RND(g) < .5) {
            double x = floor(RND(g) * 3 + 1);
            x = x + x * (y == 4) * 20;
            if (y < 5 || g->inv[y] + x < 4) {
                g->inv[y] += x;
                add_qty(g, y, x, 0);
            }
        }
    }
    snprintf(a, n, "You find an abandoned wagon%s", g->nt ? " with the following:" : ", but it is empty.");
}

// LF.LIB:52000 thief
static void thief(Game *g, char *a, size_t n) {
    g->inv[INV_FOOD] = g->pf;
    g->nt = 0;
    int y = (int)floor(RND(g) * 4) + 2;
    y = y + 3 * (y == 5);
    double yy = g->inv[y];
    if (yy) {
        double x = 100 * (yy > 100) + yy * (yy < 101);
        x = floor(RND(g) * x + 1);
        g->inv[y] = yy - x;
        g->pf = g->pf - x * (y == 8);
        add_qty(g, y, x, 0);
        if (g->inv[y] < 0) g->inv[y] = 0;
    }
    // 52020: the money branch needs Y = 0, which the draw never produces.
    snprintf(a, n, "A thief comes during the night and steals %s.", g->nt ? g->t[0] : "");
    g->t[0][0] = 0;                             // 52030: T$(0) = "", yet Z stays 1
    recompute(g);
}

static void event(Game *g, int k) {
    char msg[128];
    switch (k) {
    case 0:                                                     // 10000
        lose_days(g, "Snow bound", 10, ART_SNOW);
        GOSUB(g, 300);
        break;
    case 1:                                                     // 10100
        if (g->tm > 2) {
            int z = pick(g);
            snprintf(msg, sizeof msg, "%s has %s.", g->name[z], ILLNESS[2]);
            g->h1[z] = 2; g->h2[z] = 10;
            box710(g, msg, ART_NONE, 0);
        }
        break;
    case 2:                                                     // 10200 does not exist
        break;
    case 3:
        illness10300(g);
        break;
    case 4: {                                                   // 10400
        if (ASK(g, .kind = REQ_GRAVE_ASK)) {
            char name[OT_NAME_MAX], ep[OT_EPITAPH_MAX];
            tomb_read(g, g->ln, name, ep);
            ASK(g, .kind = REQ_GRAVE_SHOW, .text = name, .text2 = ep);
            g->sr = 0;
            GOSUB(g, 300);
        }
        // 10410 / 450: the next tombstone on this leg.
        g->dl = -1;
        for (int l4 = 0; l4 <= 1; l4++)
            if (g->sn == g->sn_rec[l4] && g->dl < g->ml[l4] && g->ml[l4] < g->d) {
                g->ln = l4; g->dl = g->ml[l4];
            }
        break;
    }
    case 5:                                                     // 10500
        g->pf += 30;
        box710(g, "Indians help find food.", ART_NONE, 0);
        break;
    case 6: {                                                   // 10600-10605
        if (g->tm == 2 || g->tm == 3) break;
        const char *z = "thunderstorm";
        int art = ART_STORM;
        g->ar = a2_round(a2_add(g->ar, a2_fac(1)));
        if (g->tm < 2) {
            z = "blizzard"; art = ART_BLIZZARD;
            g->ar = a2_round(a2_sub(g->ar, a2_fac(1)));
            g->as = a2_round(a2_add(g->as, a2_fac(8)));
        }
        snprintf(msg, sizeof msg, "Severe %s", z);
        lose_days(g, msg, 1, art);
        break;
    }
    case 7:                                                     // 10700-10705
        if (g->lm > 11 && g->tm < 5) {
            if (RND(g) > .5) lose_days(g, "Heavy fog", 1, ART_NONE);
            else box710(g, "Heavy fog", ART_NONE, 0);
        }
        if (g->lm <= 11 && g->tm > 4) box710(g, "Hail storm.", ART_HAIL, 0);
        break;
    case 8:                                                     // 10800-10830
        if (RND(g) < .33) {
            g->b = (int)floor(RND(g) * 3) + 5;
            broken_part(g);
            g->sd = 1;
            rest500(g);
            if (!g->b) GOSUB(g, 300);
            break;
        }
        if (RND(g) < .5) {                                      // 10830
            int z = pick(g);
            int v = (int)floor(RND(g) * 2);
            g->h1[z] = v; g->h2[z] = 30;
            snprintf(msg, sizeof msg, "%s has %s.", g->name[z], ILLNESS[v]);
            box710(g, msg, ART_NONE, 0);
            break;
        }
        g->inv[INV_OXEN] -= .5;                                 // 10820
        snprintf(msg, sizeof msg, "One of the oxen %s",
                 g->inv[INV_OXEN] == floor(g->inv[INV_OXEN]) ? "has died." : "is injured.");
        box710(g, msg, ART_NONE, 0);
        recompute(g);
        oxen_check(g);
        break;
    case 9:                                                     // 10900
        lose_days(g, RND(g) < .5 ? "Wrong trail" : "Lose trail", 5, ART_NONE);
        break;
    case 10:                                                    // 11000-11010
        if (RND(g) < .5) { box710(g, "Rough trail", ART_NONE, 0); g->hr = 10; }
        else lose_days(g, "Impassible trail", 10, ART_NONE);
        break;
    case 11:                                                    // 11100
        g->pf += 20;
        box710(g, "Find wild fruit.", ART_FRUIT, 0);
        GOSUB(g, 300);
        break;
    case 12: {                                                  // 11200-11275
        int r = (int)floor(RND(g) * 3);
        if (r == 1) {
            int z = pick(g);
            snprintf(msg, sizeof msg, "%s is lost", g->name[z]);
            lose_days(g, msg, 5, ART_NONE);
        } else if (r == 2) {
            lose_days(g, "Ox wanders off", 3, ART_NONE);
        } else {
            fire(g);                                            // 11205-11210
            if (g->nt > 0) {
                ASK(g, .kind = REQ_EVENT_SHOW, .a = EV_LIST, .b = ART_FIRE,
                    .text = "A fire in the wagon results in loss of:",
                    .lines = (const char (*)[OT_LINE_MAX])g->t, .nlines = g->nt);
                g->b = ASK(g, .kind = REQ_EVENT_END, .a = EV_LIST);
                GOSUB(g, 300);
            }
        }
        break;
    }
    case 13: {                                                  // 11300-11370
        char a[96];
        if (RND(g) < .5) {
            thief(g, a, sizeof a);
            if (g->nt) {
                ASK(g, .kind = REQ_EVENT_SHOW, .a = EV_LIST, .b = ART_THIEF, .text = a,
                    .lines = (const char (*)[OT_LINE_MAX])g->t, .nlines = g->nt);
                g->b = ASK(g, .kind = REQ_EVENT_END, .a = EV_LIST);
            }
            oxen_check(g);
        } else {
            abandoned(g, a, sizeof a);
            list_box(g, a);
        }
        break;
    }
    case 14:                                                    // 11400-11420
        g->f9 = 1;
        if (RND(g) < .2) { g->hr = 20; box700(g, "Bad water", ART_NONE); break; }
        if (RND(g) < .5) { g->hr = 10; box700(g, "Very little water", ART_NONE); break; }
        box700(g, "Inadequate grass", ART_NONE);
        break;
    }
}

// ---------------------------------------------------------------------------
// The day (3100-3499)
// ---------------------------------------------------------------------------
// 800: Return during travel opens the menu.
static void poll800(Game *g) {
    if (g->sd) return;
    if (ASK(g, .kind = REQ_POLL)) {
        g->inv[INV_FOOD] = g->pf;
        menu4000(g);
        g->pf = g->inv[INV_FOOD];
        GOSUB(g, 300);
    }
}

static void day3100(Game *g) {
    for (;;) {                                                  // FOR L0 = 1 TO 1
        climate(g, &g->qt, &g->qp);                             // 3100
        if (!g->sd) poll800(g);
        if (g->h > 139) g->h = 139;                             // 3150
        g->re[0] = (g->as > 30);                                // 3160
        g->re[14] = .5 * (g->ar < .1);
        g->re[3] = .01 + g->h / 1500;
        g->re[4] = (g->d < g->dl);
        g->re[5] = .05 * (g->pf == 0);
        g->re[6] = (g->w > 7) + .15 * (g->tm < 2);
        if (g->f9) { GOSUB(g, 350); g->f9 = 0; }                // 3170
        g->hr = 0;                                              // 3180
        if (!g->sd) {
            for (int l8 = 0; l8 <= 14; l8++) {
                if (RND(g) < g->re[l8]) {
                    GOSUB(g, 3180);                             // & WIND: NORMAL: & DFT,7,52200
                    event(g, l8);
                    GOSUB(g, 3181);                             // INVERSE
                    if (g->b > 0) { menu4000(g); GOSUB(g, 300); l8 = 20; }
                }
            }
            GOSUB(g, 3190);                                     // & CST: & DFT,7,49152: & WIND
            poll800(g);
        }
        g->h0 = 0;                                              // 3200
        for (int l = 0; l <= 4; l++)
            if (g->h1[l] > 0) {
                g->h2[l] -= 1; g->h0 += 1;
                if (g->h2[l] < 1) g->h1[l] = 0;
            }
        if (RND(g) < .5) {                                      // 3205
            int t = (int)(g->qt + floor(RND(g) * 41));
            g->pp = RND(g) < g->qp;
            g->w = (int)floor((t + 10) / 20.0);
            g->tm = g->w;
        }
        g->tr = 0; g->ts = 0;                                   // 3206
        if (g->pp) {
            int z = RND(g) < .3;
            g->w = 6 + z + z;
            g->tr = a2_round(a2_add(A2K(.2), a2_mul(A2K(.6), a2_fac(z))));   // TR = .2 + .6 * Z
            if (g->tm < 2) { g->w += 1; g->ts = a2_round(a2_mul(8, a2_fac(g->tr))); g->tr = 0; }
        }
        double zt = g->tm - 3;                                  // 3207
        if (zt < 0) zt = 2 - g->tm;
        double zc = 5 - g->tm - g->tm - g->op;                  // 3210
        if (zc < 0) zc = 0;
        int x = zc > .5, y = g->pf == 0;                        // 3215
        double zf = y ? 8 : g->f0;
        double zp = (g->w > 5) + (g->w > 7) + g->p + g->p;      // 3220
        double z = (x || y) ? g->fs + .8 : g->fs * .5;          // 3225
        g->fs = z;                                              // 3230
        g->h = .9 * g->h + zt + zc + zf + zp + g->fs + g->h0 + g->hr;
        g->pf -= g->fc;
        if (g->pf < 0) g->pf = 0;
        if (!g->w1 && g->h > 139) { illness10300(g); GOSUB(g, 3235); }   // 3235: ... INVERSE
        // The miles (and the weather they depend on) in Applesoft's own
        // arithmetic: INT(M) shows, and M reaches each landmark's total give
        // or take its rounding.
        // 3240: AR = .9 * AR + TR: AS = .97 * AS + TS
        g->ar = a2_round(a2_add(a2_round(a2_mul(A2K(.9), a2_fac(g->ar))), a2_fac(g->tr)));
        g->as = a2_round(a2_add(a2_round(a2_mul(A2K(.97), a2_fac(g->as))), a2_fac(g->ts)));
        if (g->as && (g->tm > 2 || g->w == 8)) {
            g->ar = a2_round(a2_add(g->ar, a2_fac(.5)));        // AR = AR + P5
            g->as = a2_round(a2_sub(g->as, a2_fac(5)));         // AS = AS - 5
            if (g->as < 0) g->as = 0;
        }
        double s = a2_round(a2_sub(1, a2_div(g->as, a2_fac(40))));   // 3244: Z = 1 - AS / 40
        if (s < 0) s = 0;
        // 3245: V = BS * (C1 - .1 * H0) * NOT SD * Z: IF 1.1 * V > D THEN V = D
        A2Fac t = a2_mul(g->bs, a2_sub(1, a2_mul(A2K(.1), a2_fac(g->h0))));
        t = a2_mul(a2_round(t), a2_fac(!g->sd));
        double v = a2_round(a2_mul(a2_round(t), a2_fac(s)));
        if (a2_round(a2_mul(A2K(1.1), a2_fac(v))) > g->d) v = g->d;
        g->d = a2_round(a2_sub(g->d, a2_fac(v)));               // 3246: D = D - V: M = M + V
        g->m = a2_round(a2_add(g->m, a2_fac(v)));
        if (g->h > 139) g->h = 139;                             // 3250
        g->ad += 1;                                             // 3255
        if (g->ad > 31 || (g->ad > 28 && g->am == 2) ||
            (g->ad > 30 && (g->am == 4 || g->am == 6 || g->am == 9 || g->am == 11))) {
            g->ad = 1; g->am += 1;
            g->ay += (g->am > 12);
            if (g->am > 12) g->am = 1;
            climate(g, &g->qt, &g->qp);
        }
        ASK(g, .kind = REQ_DAY_END, .a = !g->sd);               // 3246-3265
        if (!g->d) break;                                       // 3499: L0 = NOT D
    }
    g->inv[INV_FOOD] = g->pf;
    GOSUB(g, 3499);                                             // NORMAL
}

// ---------------------------------------------------------------------------
// The action menu (4000-4095) and its choices
// ---------------------------------------------------------------------------
static void pace4300(Game *g) {                                 // PACE.LIB
    int z = ASK(g, .kind = REQ_PACE);
    if (z) g->p = z;
    recompute(g);
}

static void rations4400(Game *g) {                              // RATION.LIB
    int z = ASK(g, .kind = REQ_RATIONS);
    if (z) g->r = z;
    recompute(g);
}

// BUY.LIB: fort store. Returns with PF = I(8).
static int store_check(Game *g, int l, int z, int many) {       // 50030-50035
    long v = store_price_cents(l, g->lm);
    if (z * v > g->my_cents) {
        ASK(g, .kind = REQ_STORE_MSG, .a = many ? STORE_MSG_AFFORD_MANY : STORE_MSG_AFFORD, .b = l);
        return 1;
    }
    int t = 0;
    if (l + 1 == 2 && g->inv[2] + z > 20) { t = 1; ASK(g, .kind = REQ_STORE_MSG, .a = STORE_MSG_OXEN_CAP, .b = l); }
    if (l + 1 == 8 && g->inv[8] + z > 2000) { t = 1; ASK(g, .kind = REQ_STORE_MSG, .a = STORE_MSG_FOOD_CAP, .b = l); }
    if (l + 1 < 8 && l + 1 > 4 && g->inv[l + 1] + z > 3) { t = 1; ASK(g, .kind = REQ_STORE_MSG, .a = STORE_MSG_PART_CAP, .b = l); }
    return t;
}

static void store4800(Game *g) {
    for (int first = 1; ; first = 0) {                          // 50000 once, then 50010
        int l = ASK(g, .kind = REQ_STORE_MENU, .a = first);
        if (l == 8) { g->pf = g->inv[INV_FOOD]; return; }
        if (store_check(g, l, 1, 0)) continue;                  // 50015
        int z = ASK(g, .kind = REQ_STORE_QTY, .a = l);          // 50020
        if (store_check(g, l, z, 1)) continue;
        int k = 1 + 19 * (l == 3);                              // 50025
        g->my_cents -= z * store_price_cents(l, g->lm);
        g->inv[l + 1] += z * k;
    }
}

// TRADE.LIB
static void trade4900(Game *g) {
    ASK(g, .kind = REQ_TRADE_SHOW, .a = 0);
    int x = (int)floor(RND(g) * 7);                             // 50010
    int y = (int)floor(RND(g) * 7);
    y = y + (y == x);
    y = y * (y < 7);
    double zx = a2_val(STORE_BASE[x]) / (1 + 19 * (x == 2));    // 50011
    double zy = a2_val(STORE_BASE[y]) / (1 + 19 * (y == 2));
    double z = zx / (zy * (1 + 1.2 * RND(g)));
    double q = floor(g->inv[x + 2] + .5);
    double v = 1, f = floor(z);
    if (z < 1) { v = floor(1 / z); f = 1; }
    if ((x == 2 && y == 6) || (y == 2 && x == 6)) { v *= 50; f *= 50; }   // 50015
    double have = g->inv[y + 2] + f;                            // 50025
    int cap = (have > 3) * (y < 6) * (y > 2) + (y == 6) * (have > 2000) + (y == 0) * (have > 20);
    int nobody = RND(g) > .95;                                  // 50027
    if (nobody || cap) { ASK(g, .kind = REQ_TRADE_NOBODY); return; }
    char want[64], offer[64], name[40], num[16];
    a2_item_name(x + 2, v, 1, name, sizeof name);               // 50030
    snprintf(want, sizeof want, "%s %s", a2_str(v, num), name);
    if (q < v) { ASK(g, .kind = REQ_TRADE_LACK, .text = want); return; }
    int she = RND(g) > .67;                                     // 50031
    a2_item_name(y + 2, f, 1, name, sizeof name);               // 50032
    snprintf(offer, sizeof offer, "%s %s", a2_str(f, num), name);
    if (ASK(g, .kind = REQ_TRADE_OFFER, .text = want, .text2 = offer, .a = she)) {
        g->inv[y + 2] += f;                                     // 50035
        g->inv[x + 2] = q - v;
        ASK(g, .kind = REQ_TRADE_SHOW, .a = 1);
        recompute(g);
    }
    g->pf = g->inv[INV_FOOD];                                   // 50040
}

// HUNT.LIB
static void hunt4600(Game *g) {
    double q = g->pf;
    Req r = { .kind = REQ_HUNT, .a = (int)g->inv[INV_BULLETS],
              .b = (g->lm > 3 && g->lm < 13) | (g->lm > 6) << 1 | (g->lm < 7) << 2,
              .c = g->zo };
    if (ot_ask(g, &r) < 0) game_over(g, OUTCOME_ABANDONED);     // Esc Esc -> GOTO 31000
    int l = r.a;
    l = (int)floor(l / (double)(2 - (l < 3)));                  // 50011
    int shot = l;
    g->inv[INV_BULLETS] = r.b;
    int flags = 0;
    if (l) flags |= HUNT_GOT;                                   // 50012
    if (l && g->inv[INV_FOOD] >= 2000) { l = 0; flags |= HUNT_FULL; }   // 50013
    else {
        if (l && l + g->inv[INV_FOOD] > 2000) {                 // 50014
            l = (int)(2000 - g->inv[INV_FOOD]);
            if (l && l <= 100) flags |= HUNT_ROOM;
        }
        if (l > 100) { flags |= HUNT_CARRY100; l = 100; }       // 50015
        if (!l) flags |= HUNT_NONE;                             // 50016
    }
    ASK(g, .kind = REQ_HUNT_RESULT, .a = flags, .b = l, .c = shot);
    g->inv[INV_FOOD] = q + l;                                   // 50020
    g->pf = g->inv[INV_FOOD];
}

static void menu4000(Game *g) {
    g->f9 = 0; g->sr = 0;                                       // 4000
    for (int first = 1; ; first = 0) {
        g->b = g->b * (g->b != 1);                              // 4005
        if (g->h > 139) g->h = 139;                             // 4010
        g->a = (g->a + 1) * (g->a < 2);                         // 4030
        int x = ASK(g, .kind = REQ_ACTION_MENU, .a = first);    // 4005-4060
        if (!g->b && x == 1) {
            g->b = 2 * (g->inv[INV_OXEN] == 0);
            if (!g->b) return;
        }
        if (g->b && x == 1) { ASK(g, .kind = REQ_MUST_TRADE, .a = g->b); continue; }   // 4070
        int z = (g->ll ? 2 : 0) * (x > 7) + x;                  // 4090
        switch (z) {
        case 2: ASK(g, .kind = REQ_SUPPLIES); break;            // 4100
        case 3: ASK(g, .kind = REQ_MAP); break;                 // 4200
        case 4: pace4300(g); break;
        case 5: rations4400(g); break;
        case 6:                                                 // 4500
            g->sd = ASK(g, .kind = REQ_REST_ASK);
            g->x5 = 9;
            rest4505(g);
            break;
        case 7:                                                 // 4900
            trade4900(g);
            g->sd = 1;
            g->inv[INV_OXEN] = g->inv[INV_OXEN] * (g->inv[INV_OXEN] > 0);
            break;
        case 8: ASK(g, .kind = REQ_TALK, .a = g->a); break;     // 4700
        case 9: store4800(g); break;                            // 4800
        case 10: hunt4600(g); g->sd = 1; break;                 // 4600
        }
        if (g->sd > 0) rest500(g);
        if (g->inv[g->b]) { g->inv[g->b] -= (g->b > 2); g->b = 0; }
    }
}

// ---------------------------------------------------------------------------
// The trail (1000-3505)
// ---------------------------------------------------------------------------
// 3000-3060: a leg, travelled day by day until D = 0.
static void travel3000(Game *g) {
    g->pf = g->inv[INV_FOOD];
    g->sn = g->nm * 100 + g->lm;
    GOSUB(g, 300);
    GOSUB(g, 3000);                                             // X = USR(1)
    g->dd = g->d;
    g->sd = 0;
    recompute(g);
    g->dl = -1;                                                 // 450
    for (int l4 = 0; l4 <= 1; l4++)
        if (g->sn == g->sn_rec[l4] && g->dl < g->ml[l4] && g->ml[l4] < g->d) {
            g->ln = l4; g->dl = g->ml[l4];
        }
    g->re[8] = .04 + .03 * (g->zo > 2);                         // 3060
    g->re[10] = .05 * (g->zo > 2);
    g->re[11] = .04 * (g->am > 4 && g->am < 10);
    day3100(g);
}

// 3500-3505: river crossing, animation, outcome, and the drowned.
static void river3500(Game *g) {
    g->w1 = 1;
    river_run(g);                                               // RIVER.LIB
    g->w1 = 0;
    ASK(g, .kind = REQ_CROSS, .a = g->ci, .b = g->nt);         // CROSS.LIB
    // CROSS.LIB's 50070 / 50080 pick splash points (two RND each) that are
    // never plotted, yet the draws move the RND stream: once per line of the
    // water (50010), of the bank (50030), or of a ford gone wrong (50060,
    // which first draws which of the two to call).
    int splash = 53 + (!g->nt ? 61 : g->ci == 2 ? 22 : 0);
    for (int l = 0; l < splash; l++) { RND(g); RND(g); }
    if (g->nt && g->ci == 2)
        for (int l = 0; l <= 21; l++) RND(g);
    list_box(g, g->amsg);                                       // 900
    if (g->nt)
        for (int l1 = 0; l1 <= 4; l1++)
            if (g->h1[l1] == -2) death(g, l1);
    g->pf = g->inv[INV_FOOD];                                   // 3505
}

// 29000-29020: state from the handoff area, set once.
static void trail_init(Game *g) {
    g->my_cents = (peek(g, 913) + peek(g, 914) * 256L) * 10;
    g->am = peek(g, 902); g->ad = peek(g, 903); g->ay = peek(g, 901) + 1800;
    // VAR.BIN presets
    g->np = 5; g->p = 1; g->r = 1; g->h = 0; g->m = 0; g->lm = 0; g->s = 0; g->sr = 1;
    g->zo = 0;
    double rnd = RND(g);                                        // AS = RND(1) * 12 * (PEEK(902) < 4)
    g->as = a2_round(a2_mul(a2_round(a2_mul(rnd, a2_fac(12))), a2_fac(peek(g, 902) < 4)));
    double qt, qp;
    climate(g, &qt, &qp);
    g->w = (int)floor((qt + 10) / 20);
    g->re[7] = .06; g->re[9] = .02; g->re[12] = .01; g->re[13] = .02;   // 29001
    g->re[1] = .007; g->re[2] = 0;
    g->tm = g->w;                                               // 29004
    g->ar = a2_round(a2_add(7 - peek(g, 902), a2_fac(RND(g))));   // AR = 7 - PEEK(902) + RND(1)
    g->q1 = 1;
    tomb_read_segments(g);
    for (int l = 3; l <= 7; l++) g->inv[l] = peek(g, 905 + l);  // 29005
    g->inv[INV_FOOD] = peek(g, 906) + peek(g, 907) * 256;
    g->inv[INV_OXEN] = 2 * peek(g, 905);
    g->inv[INV_BULLETS] = peek(g, 909) * 20;
    g->pf = g->inv[INV_FOOD];
    int z = 1920;                                               // 29010-29020
    for (int l = 0; l <= 4; l++) {
        int k = 0;
        for (int a = z; a <= 2047; a++) {
            if (!peek(g, a)) { z = a + 1; break; }
            if (k < OT_NAME_MAX - 1) g->name[l][k++] = (char)peek(g, a);
        }
        g->name[l][k] = 0;
    }
}

// FLIP.LIB (side 1): the disk turns over at Fort Laramie.
static void flip600(Game *g) {
    g->s = 1;
    ASK(g, .kind = REQ_FLIP);
    tomb_read_segments(g);
}

void trail_run(Game *g) {
    trail_init(g);
    int look = 1;                                               // 110: Z$ = "Y"
    for (;;) {
        g->a = (int)floor(RND(g) * 3);                          // 1000
        g->zo = zone_of(g->lm);
        if (g->lm) {                                            // 1005-1006
            g->f9 = 0;
            look = ASK(g, .kind = REQ_LOOK_AROUND);
            g->q[g->q1] = g->lm; g->q1 += 1;
        }
        if (look) {                                             // 1010
            g->ll = 0;
            GOSUB(g, 300);
            GOSUB(g, 950);
            menu4000(g);
            g->ll = 1;
        }
        int z;
        for (;;) {                                              // 1015
            if (LM_TYPE[g->lm] == 2) river3500(g);
            oxen_check(g);
            if (g->b > 0) menu4000(g);
            if (g->lm == 16) end_lib(g);                        // 2100
            GOSUB(g, 300);                                      // 2105
            GOSUB(g, 355);
            z = 1;
            if (LM_ALT[g->lm]) z = ASK(g, .kind = REQ_SPLIT);   // 2110-2120
            if (z != 3) break;
            ASK(g, .kind = REQ_MAP);                            // 4200
        }
        int k = z == 2 ? LM_ALT[g->lm] : LM_LEG[g->lm];         // 2200
        g->d = LEG_MILES[k]; g->md = LEG_MPD[k]; g->nm = LEG_DEST[k];
        ASK(g, .kind = REQ_LEG);
        g->ll = 1;                                              // 1016
        travel3000(g);
        g->lm = g->nm;
        if (g->lm == 5) flip600(g);
        if (g->lm == 17) break;
    }
    g->ll = 0;                                                  // 1020
    end_handoff(g);
}
