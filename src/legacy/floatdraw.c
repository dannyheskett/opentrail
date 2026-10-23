// See floatdraw.h. The engine runs each pass's rules in raft_tick(); the
// drawing that pass ends with is done afterwards from a copy of what was last
// drawn, so each pass draws exactly what the BASIC drew.
#include "floatdraw.h"
#include "dalles.h"
#include "mach.h"

static int rx_(int hp) { return 84 + 8 * hp; }                     // FN RX, FN RY (1040)
static int ry_(int hp) { return 5 * hp - 6; }

void float_draw_start(FloatDrawn *d) {
    d->tc = 0;
    d->lp = 16;
    for (int z = 0; z < 2; z++) { d->rx[z] = 0; d->ry[z] = 0; }
}

// Unless the pass left for 900 or 950 first (1115/1116).
void float_draw_pass(Mach *m, const Raft *r, FloatDrawn *d) {
    if (r->tc == d->tc) return;
    d->tc = r->tc;
    if ((r->tc > 205 && r->hp == 17) || r->tc > 225) return;
    if (r->hp != d->lp) {                                           // 1140
        d->lp = r->hp;
        amp_putp(m, 0, 1, rx_(r->hp), ry_(r->hp));
    }
    if (r->fl[2]) dalles_image(m, 5, r->sx - 6, r->sy + 3);         // 1148 (drawn, then moved)
    if (r->fl[3]) dalles_image(m, 6, r->tx - 6, r->ty + 3);         // 1149
    int moved[2];
    for (int z = 0; z < 2; z++) {                                   // 1150-1155
        // a rock on the river moves every pass; one that did not move was not drawn
        moved[z] = r->rx[z] != d->rx[z] || r->ry[z] != d->ry[z];
        d->rx[z] = r->rx[z]; d->ry[z] = r->ry[z];
        if (moved[z]) amp_putp(m, z + 1, 3, (int)r->rx[z], (int)r->ry[z]);
    }
    for (int z = 1; z >= 0; z--)                                    // 1160-1165: rocks leaving
        if (moved[z] && !(r->rx[z] < 240 && r->ry[z] > 10)) amp_putp(m, z + 1, 3, 275, 0);
}

void float_draw_hit_rock(Mach *m, const Raft *r) {                 // 770
    if (r->rock >= 0) amp_putp(m, r->rock + 1, 3, 275, 0);
}

void float_draw_landing(Mach *m, const Raft *r, void (*delay)(void *ctx, int passes), void *ctx) {
    int hp = (r->x - 84) / 8;                                       // the lane when the last pass ended
    if (hp != 17)                                                   // 960: carried to the landing
        for (int a = hp; a <= 17; a++) {
            amp_putp(m, 0, 1, rx_(a), ry_(a));
            if (delay) delay(ctx, 30);                              // FOR B = 1 TO 30
        }
    amp_putp(m, 0, 1, rx_(17), ry_(17));                            // 900
}
