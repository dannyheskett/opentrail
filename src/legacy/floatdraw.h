// The raft game's drawing (FLOAT:1140-1165, 770, 900, 960), kept apart from the
// shell's text screens so tests can drive it with the engine's raft_tick().
#ifndef OT_FLOATDRAW_H
#define OT_FLOATDRAW_H

#include "game.h"

#ifndef OT_MACH_TYPEDEF                 // C99 allows one typedef of a name
#define OT_MACH_TYPEDEF
typedef struct Mach Mach;
#endif

typedef struct {
    int tc;                 // the pass last drawn
    int lp;                 // LP: the lane the raft was last drawn in
    double rx[2], ry[2];    // rock positions last seen
} FloatDrawn;

void float_draw_start(FloatDrawn *d);                           // 1030: LP = HP = 16
void float_draw_pass(Mach *m, const Raft *r, FloatDrawn *d);    // 1140-1165 for the last pass
void float_draw_hit_rock(Mach *m, const Raft *r);               // 770
void float_draw_landing(Mach *m, const Raft *r, void (*delay)(void *ctx, int passes), void *ctx);  // 960, 900

#endif
