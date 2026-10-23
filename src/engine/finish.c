// The end of the trail: END.LIB (The Dalles and the handoff), FLOAT (the raft
// game) and WIN (points and the Top Ten). FLOAT and WIN are separate programs
// in the original, so they start from the memory handoff area alone.
#include "engine.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

// ---------------------------------------------------------------------------
// END.LIB
// ---------------------------------------------------------------------------
// 50000-50040 at The Dalles (from OREGON TRAIL:2100).
void end_lib(Game *g) {
    for (int first = 1; ; first = 0) {
        if (ASK(g, .kind = REQ_DALLES, .a = first) == 1) { end_handoff(g); return; }   // 50010
        // 50015-50020: the toll, $5 plus 50 cents per ox (rounded).
        long v = 500 + (long)floor(g->inv[INV_OXEN] + .5) * 50;
        int yes = ASK(g, .kind = REQ_BARLOW_ASK, .x = v / 100.0);
        if (yes && g->my_cents > v) { g->my_cents -= v; ASK(g, .kind = REQ_BARLOW_PAID); return; }
        if (yes && g->my_cents < v) ASK(g, .kind = REQ_BARLOW_NO_CASH);   // 50030
        // 50040: back to the choice (exact change lands here too)
    }
}

// 50050-50080: state into the handoff area, then FLOAT or WIN.
void end_handoff(Game *g) {
    const int L = 50050;
    poke(g, 900, g->np, L);
    poke(g, 901, g->ay - 1800, L);
    poke(g, 902, g->am, L);
    poke(g, 903, g->ad, L);
    poke(g, 909, g->inv[INV_OXEN] + .5, L);
    double f = g->inv[INV_FOOD];
    poke(g, 906, f - floor(f / 256) * 256, L);
    poke(g, 907, floor(f / 256), L);
    poke(g, 908, g->inv[INV_CLOTHES], L);
    f = g->inv[INV_BULLETS];
    poke(g, 904, f - floor(f / 256) * 256, L);
    poke(g, 905, floor(f / 256), L);
    poke(g, 916, floor(g->h / 35), L);
    int z = 1920;                                               // 50060
    for (int l = 0; l <= g->np - 1; l++) {
        for (const char *c = g->name[l]; *c; c++) poke(g, z++, (unsigned char)*c, 50060);
        poke(g, z++, 0, 50060);
    }
    for (int l = 5; l <= 7; l++) poke(g, 905 + l, g->inv[l], 50070);
    long dollars = g->my_cents / 100;
    poke(g, 913, dollars % 256, 50070);
    poke(g, 914, dollars / 256, 50070);
    poke(g, 917, g->my_cents % 100 + .5, 50070);
    if (g->lm == 16) float_run(g);
    win_run(g);
}

// ---------------------------------------------------------------------------
// FLOAT
// ---------------------------------------------------------------------------
static void raft_box(Raft *r) {                                 // 1110 / 1170
    r->x = 84 + 8 * r->hp; r->y = 5 * r->hp - 6;
    r->x1 = r->x + 6; r->y1 = r->y + 18; r->x2 = r->x + 26; r->y2 = r->y + 25;
}

// 200: box test; ROCK keeps the rock index on a hit, -1 on a miss.
static void rock_test(Raft *r, int z) {
    r->x3 = (int)r->rx[z] + 11; r->y3 = (int)r->ry[z] + 1;
    r->x4 = r->x3 + 20; r->y4 = r->y3 + 6;
    r->rock = z; r->col = 1;
    if (r->x2 < r->x3 || r->x1 > r->x4 || r->y1 > r->y4 || r->y2 < r->y3) { r->col = 0; r->rock = -1; }
}

// 300: a rock enters from the bottom (13%) or the left edge.
static void rock_spawn(Game *g, int z) {
    Raft *r = &g->raft;
    r->fl[z] = 1;
    int n = (int)floor(100 * RND(g) + 1);
    if (n < 14) { r->ry[z] = 175; r->rx[z] = floor(10 * RND(g)); return; }
    r->rx[z] = 0;
    r->ry[z] = 50 + floor(120 * RND(g) + 1);
}

void raft_start(Game *g) {                                      // 1030-1060
    Raft *r = &g->raft;
    memset(r, 0, sizeof *r);
    r->dir = -1; r->hp = 16; r->tc = 0; r->lp = r->hp; r->rock = -1; r->col = 0;
    r->x = 84 + 8 * r->hp; r->y = 5 * r->hp - 6;
    r->x1 = r->x; r->y1 = r->y; r->x2 = r->x + 35; r->y2 = r->y + 25;
    r->nr = -1;
}

// 1070-1170, one pass. key is USR(3): 136/139 left/up, 149/138 right/down.
int raft_tick(Game *g, int key) {
    Raft *r = &g->raft;
    r->tc += 1;                                                 // 1070
    // IF NOT FL(0) AND INT(100*RND(1)+1) <= RF: Applesoft draws RND either way
    int n0 = (int)floor(100 * RND(g) + 1);
    if (!r->fl[0] && n0 <= 15) rock_spawn(g, 0);
    int n1 = (int)floor(100 * RND(g) + 1);                     // 1075
    if (!r->fl[1] && n1 <= 15) rock_spawn(g, 1);
    for (int z = 0; z < 2; z++)                                 // 1080-1085
        if (r->fl[z]) { r->rx[z] += 8; r->ry[z] -= 4; }
    if (r->hp > 0 && (key == 136 || key == 139)) { r->dir -= 1; if (r->dir < -1) r->dir = -1; }
    if (r->hp < 17 && (key == 149 || key == 138)) { r->dir += 1; if (r->dir > 1) r->dir = 1; }
    if (r->dir) {                                               // 1110 + 400
        r->hp += r->dir;
        r->col = 0;
        if (r->hp < 1 || r->hp > 16) { r->col = 1; r->dir = -r->dir; }
        raft_box(r);
    }
    if (r->tc > 205 && r->hp == 17) return RAFT_LANDED;         // 1115
    if (r->tc > 225) return RAFT_MISSED;                        // 1116
    if (!r->col && r->fl[0]) rock_test(r, 0);                   // 1120
    if (!r->col && r->fl[1]) rock_test(r, 1);                   // 1130
    if (r->hp != r->lp) r->lp = r->hp;                          // 1140
    if (r->tc == 97 || r->tc == 157) r->fl[2] = 0;              // 1144
    if (r->tc == 60 || r->tc == 120 || r->tc == 170) { r->fl[2] = 1; r->sx = 62; r->sy = 189; }
    if (r->tc == 175) { r->fl[3] = 1; r->tx = -8; r->ty = 218; }
    if (r->fl[2]) { r->sx += 6; r->sy -= 3; }                   // 1148 (drawn before the move)
    if (r->fl[3]) { r->tx += 6; r->ty -= 3; }
    for (int z = 1; z >= 0; z--)                                // 1160-1165: 600
        if (r->fl[z] && !(r->rx[z] < 240 && r->ry[z] > 10)) r->fl[z] = 0;
    return r->col ? RAFT_HIT : RAFT_GO;                         // 1170
}

// 700-770: the collision; returns with the survivors, or ends the game.
static void raft_hit(Game *g) {
    Raft *r = &g->raft;
    char a[96];
    g->nt = 0;
    if (r->rock == -1) {
        snprintf(a, sizeof a, "The raft has hit the shore.");
        loss_drown(g, .15); loss_oxen(g, .3); loss_items(g, .5);
    } else {
        snprintf(a, sizeof a, "The raft has hit a rock.");
        r->fl[r->rock] = 0;
        loss_drown(g, .6); loss_oxen(g, .6); loss_items(g, .7);
    }
    const char *z = g->nt ? "  You have lost:" : "";            // 720
    if (g->nt > 9) {                                            // 730
        snprintf(a, sizeof a, "The raft is destroyed, everything has been lost.");
        g->np = 0; z = ""; g->nt = 0;
    }
    strcat(a, z);                                               // 750
    ASK(g, .kind = REQ_RAFT_HIT, .text = a,
        .lines = (const char (*)[OT_LINE_MAX])g->t, .nlines = g->nt);
    int top = g->np - 1;
    for (int l = 0; ; l++) {                                    // FOR L = 0 TO NP - 1
        if (g->h1[l] == -2) {
            g->h1[l] = -1;
            g->np -= 1;
            memcpy(g->name[l], g->name[g->np < 0 ? 0 : g->np], OT_NAME_MAX);
        }
        if (l + 1 > top) break;
    }
    if (!g->np) {                                               // 760
        g->nt = 0;
        ASK(g, .kind = REQ_RAFT_HIT, .text = "Everyone in the party has died.");
        game_over(g, OUTCOME_DIED);
    }
}

static void float_land(Game *g) {                               // 900-910
    ASK(g, .kind = REQ_RAFT_LAND);
    poke(g, 900, g->np, 910);
    poke(g, 909, g->inv[INV_OXEN], 910);
    double f = g->inv[INV_FOOD];
    poke(g, 906, f - floor(f / 256) * 256, 910);
    poke(g, 907, floor(f / 256), 910);
    poke(g, 908, g->inv[INV_CLOTHES], 910);
    f = g->inv[INV_BULLETS];
    poke(g, 904, f - floor(f / 256) * 256, 910);
    poke(g, 905, floor(f / 256), 910);
    for (int l = 5; l <= 7; l++) poke(g, 905 + l, g->inv[l], 910);
}

void float_run(Game *g) {
    // 110-150: a new program; only the handoff area survives.
    g->np = peek(g, 900);
    g->am = peek(g, 902); g->ad = peek(g, 903); g->ay = peek(g, 901);
    for (int l = 0; l < 5; l++) { g->h1[l] = 0; g->h2[l] = 0; g->name[l][0] = 0; }
    for (int l = 3; l <= 7; l++) g->inv[l] = peek(g, 905 + l);
    g->inv[INV_FOOD] = peek(g, 906) + 256 * peek(g, 907);
    g->inv[INV_OXEN] = peek(g, 909);
    g->inv[INV_BULLETS] = peek(g, 904) + 256 * peek(g, 905);
    int z = 1920;
    for (int l = 0; l <= g->np - 1; l++) {
        int k = 0;
        for (int a = z; a <= 2047; a++) {
            if (!peek(g, a)) { z = a + 1; break; }
            if (k < OT_NAME_MAX - 1) g->name[l][k++] = (char)peek(g, a);
        }
        g->name[l][k] = 0;
    }

    ASK(g, .kind = REQ_RAFT_INTRO);                             // 1010-1020
    raft_start(g);
    for (;;) {
        int key = ASK(g, .kind = REQ_RAFT_FRAME);
        int st = raft_tick(g, key);
        if (st == RAFT_LANDED) break;
        if (st == RAFT_MISSED) {                                // 950-960
            g->nt = 0;
            loss_items(g, .5);
            if (g->nt) ASK(g, .kind = REQ_RAFT_HIT, .text = "The raft has missed the landing.  You have lost:",
                           .lines = (const char (*)[OT_LINE_MAX])g->t, .nlines = g->nt);
            g->raft.hp = 17;
            break;
        }
        if (st == RAFT_HIT) {                                   // 1170
            raft_hit(g);
            g->raft.col = 0;
            raft_box(&g->raft);
        }
    }
    float_land(g);
}

// ---------------------------------------------------------------------------
// WIN
// ---------------------------------------------------------------------------
void score_compute(Game *g) {
    Score *s = &g->score;
    memset(s, 0, sizeof *s);
    s->band = peek(g, 916);
    s->occupation = peek(g, 915);
    s->n[0] = peek(g, 900); s->pts[0] = s->n[0] * (500 - 100 * s->band);       // 130
    s->n[1] = 1;            s->pts[1] = 50;
    s->n[2] = peek(g, 909); s->pts[2] = s->n[2] * 4;
    s->n[3] = peek(g, 910) + peek(g, 911) + peek(g, 912); s->pts[3] = s->n[3] * 2;
    s->n[4] = peek(g, 908); s->pts[4] = s->n[4] * 2;                            // 140
    s->n[5] = peek(g, 904) + peek(g, 905) * 256;
    s->n[6] = peek(g, 906) + peek(g, 907) * 256;
    s->cash = peek(g, 913) + peek(g, 914) * 256 + peek(g, 917) / 100.0;
    s->pts[6] = s->n[6] / 25;
    s->pts[7] = (long)floor(s->cash / 5);
    s->pts[5] = s->n[5] / 50;
    for (int l = 0; l <= 7; l++) s->base += s->pts[l];                          // 305
    s->total = s->base * (s->occupation > 1 ? s->occupation : 1);               // 625
    s->rating = (s->total < 6000) + (s->total < 3000);                          // 630
    s->row = -1;
}

void win_run(Game *g) {
    score_compute(g);
    ASK(g, .kind = REQ_ARRIVE);                                 // 600-630
    TopTen hi;
    topten_read(g->persist, hi);                                // 205
    Score *s = &g->score;
    int f = a2_val(hi[9][1]) < s->total;                        // 405
    ASK(g, .kind = REQ_TOPTEN_SHOW, .a = f);                    // 2000-2020
    if (f) {                                                    // 500-540
        int row = 0;
        for (int l = 9; l >= 1; l--) {
            if (s->total > a2_val(hi[l][1])) memcpy(hi[l], hi[l - 1], sizeof hi[l]);
            if (s->total <= a2_val(hi[l][1])) { row = l; break; }
        }
        char name[OT_NAME_MAX] = "";
        hi[row][0][0] = hi[row][1][0] = hi[row][2][0] = 0;     // 520
        a2_str(s->total, hi[row][1]);
        snprintf(hi[row][2], OT_NAME_MAX, "%s", RATING[s->rating]);
        ASK(g, .kind = REQ_TOPTEN_NAME, .a = row, .out = name, .outlen = 15, .data = hi);
        snprintf(hi[row][0], OT_NAME_MAX, "%s", name);
        a2_str(s->total, hi[row][1]);
        snprintf(hi[row][2], OT_NAME_MAX, "%s", RATING[s->rating]);
        topten_write(g->persist, hi);
        s->row = row;
        ASK(g, .kind = REQ_SAVE, .a = 1);
    }
    game_over(g, OUTCOME_ARRIVED);                              // 1030
}
