// RIVER.LIB: the crossing decision and its outcome. Line numbers are RIVER.LIB's.
// The caller (OREGON TRAIL:3500-3505) shows CROSS.LIB's animation and the
// outcome box from g->ci, g->amsg and the loss list g->t / g->nt.
#include "engine.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

// 50150
void river_conditions(Game *g) {
    const double *rc = RIVER[g->rc];
    // in Applesoft's arithmetic, as AR is (see trail.c 3240)
    // RD = INT((RC(RC,0) + AR * 2) * 10 + .5) / 10
    A2Fac t = a2_add(rc[0], a2_mul(g->ar, a2_fac(2)));
    t = a2_add(a2_round(a2_mul(a2_round(t), a2_fac(10))), a2_fac(.5));
    g->rd = a2_round(a2_div(a2_int(t), a2_fac(10)));
    g->rw = a2_int(a2_add(rc[1], a2_mul(15, a2_fac(g->ar))));   // RW = INT(RC(RC,1) + 15 * AR)
    g->rs = a2_round(a2_add(rc[2], a2_fac(g->ar)));            // RS = RC(RC,2) + AR
    g->rb = (int)rc[3];
}

static void set_a(Game *g, const char *s) { snprintf(g->amsg, sizeof g->amsg, "%s", s); }

// 50060: muddy bottom
static void muddy(Game *g, double ix) {
    set_a(g, "It was a muddy crossing, but you did not get stuck.");
    if (RND(g) < .4 / ix) {
        set_a(g, "You become stuck in the mud.  Lose 1 day.");
        g->sd = 1; g->sr = 0;
        rest500(g);
        g->nt = 0;
    }
}

// 50035-50075: ford. Returns F (1 = back to the choices).
static int ford(Game *g, double ix) {
    g->nt = 0; g->ci = 2;
    set_a(g, "You made the crossing successfully.");
    if (g->rd < 2.5) {
        if (g->rb == 1) muddy(g, ix);
        else if (g->rb == 2) {                                  // 50070-50075
            char a[128];
            snprintf(a, sizeof a, "It was a rough crossing, but you did not overturn.");
            if (RND(g) < .16 / ix) {
                double v = .1 + RND(g) * .3;
                loss_items(g, v);
                snprintf(a, sizeof a, "The wagon tipped over%s",
                         g->nt ? ".  You lose:" : " but you did not lose anything.");
            }
            set_a(g, a);
        }
        return 0;
    }
    if (g->rd < 3) {                                            // 50037
        set_a(g, "Your supplies got wet.  Lose 1 day.");
        g->sd = 1;
        rest500(g);
        g->nt = 0;
        return 0;
    }
    loss_items(g, (g->rd / 10) / ix);                        // 50040
    loss_oxen(g, ((g->rd - 1) / 10) / ix);
    loss_drown(g, ((g->rd - 2.5) / 10) / ix);
    if (g->nt) set_a(g, "The river is too deep to ford.  You lose:");
    return 0;
}

// 50080-50095: caulk and float.
static int caulk(Game *g, double ix) {
    g->nt = 0; g->ci = 3;
    if (g->rd < 1.5) {
        ASK(g, .kind = REQ_RIVER_MSG, .a = RIVER_MSG_FLOAT_SHALLOW);
        return 1;
    }
    g->sd = 1;                                                  // 50085
    rest500(g);
    g->nt = 0;
    set_a(g, "You had no trouble floating the wagon across.");
    double v = (g->rd > 2.5) * (g->rs / 20) / ix;
    if (RND(g) < v) {
        loss_items(g, (.4 + g->rs / 25) / ix);
        loss_drown(g, ((g->rs - 3) / 15) / ix);
    }
    if (g->nt) set_a(g, "The wagon tipped over while floating.  You lose:");
    return 0;
}

// 50100-50125: ferry.
static int ferry(Game *g) {
    g->nt = 0;
    if (g->rd < 2.5) {
        ASK(g, .kind = REQ_RIVER_MSG, .a = RIVER_MSG_FERRY_SHALLOW);
        return 1;
    }
    int x = (int)floor(RND(g) * 5 + 2);                         // 50101
    long v = 500;
    int yes = ASK(g, .kind = REQ_FERRY_OFFER, .a = x, .x = 5);
    if (yes && g->my_cents < v) {                               // 50105
        ASK(g, .kind = REQ_RIVER_MSG, .a = RIVER_MSG_FERRY_NO_MONEY);
        return 1;
    }
    if (!yes) return 1;                                         // 50110
    g->sr = 0; g->ci = 1;                                       // 50115
    g->my_cents -= v;
    g->sd = x; g->x5 = 18;
    rest4505(g);
    g->nt = 0;
    set_a(g, "The ferry got your party and wagon safely across.");
    double p = .05 * (g->rs > 5) + .1 * (g->rs > 10);
    if (RND(g) < p) {                                           // 50120
        set_a(g, "The ferry broke loose from moorings. You lose:");
        loss_items(g, .8);
        loss_drown(g, .2);
        loss_oxen(g, .5);
        if (!g->nt) set_a(g, "Some trouble in crossing but nothing was lost.");
    }
    return 0;
}

// 50130-50145: the Shoshoni guide (Snake River).
static int guide(Game *g) {
    int x = (int)floor(RND(g) * 2 + 2);
    int has = !(g->inv[INV_CLOTHES] < x);
    if (!ASK(g, .kind = REQ_GUIDE_OFFER, .a = x, .b = has) || !has) return 1;
    g->inv[INV_CLOTHES] -= x;                                   // 50140
    int v = g->rd > 2.4 ? 2 : 1;
    ASK(g, .kind = REQ_GUIDE_GO, .a = v);
    g->nt = 0;
    return v == 1 ? ford(g, 5) : caulk(g, 5);
}

void river_run(Game *g) {
    g->sr = 0;                                                  // 50000
    g->rc = (g->lm == 2) + 2 * (g->lm == 9) + 3 * (g->lm == 12);
    river_conditions(g);
    ASK(g, .kind = REQ_RIVER_INTRO);                            // 50001-50005
    for (;;) {
        river_conditions(g);                                    // 50010
        int extra = (int)RIVER[g->rc][5];
        int f = extra ? 1 : 0;
        int v = ASK(g, .kind = REQ_RIVER_MENU, .a = extra);     // 50015
        if (v == 4 + f) { ASK(g, .kind = REQ_RIVER_INFO); continue; }   // 50020
        if (v == 3 + f) {                                       // 50025
            ASK(g, .kind = REQ_RIVER_CAMP);
            g->sr = 0; g->sd = 1;
            rest500(g);
            ASK(g, .kind = REQ_RIVER_CAMP_DATE);
            continue;
        }
        v = v + (v == 3) * (extra == 2);                        // 50030
        g->nt = 0;
        switch (v) {
        case 1: f = ford(g, 1); break;
        case 2: f = caulk(g, 1); break;
        case 3: f = guide(g); break;
        case 4: f = ferry(g); break;
        }
        if (!f) return;
    }
}
