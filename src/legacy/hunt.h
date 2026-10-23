// The hunting game: the machine code the boot loads into language-card RAM
// ($D000-$FEFF, entered through & HUNT at $8A9A -> $E003), rebuilt in C.
// Spec §21, REQ-190..193 and REQ-192a-k.
#ifndef OT_HUNT_H
#define OT_HUNT_H

#include "mach.h"

// & HUNT,Z,(LM>3 AND LM<13),(LM>6),(LM<7),1,ZO,L,Z
// bullets: Z; b1..b3: the region flags (deer, bear, buffalo allowed); b4: the
// spawn-rate flag (1 halves the chance); zone: ZO (scenery).
// Out: *pounds = L (raw pounds shot), *bullets_left = Z.
// Returns -1 when the player pressed Esc twice (the original plants a
// GOTO 31000 and leaves the game), else 0.
// The random generator is seeded as the original seeds it, from RAM $4E/$4F
// (the keyboard wait counter) and $C9-$CD (Applesoft's RND seed) in m->ram.
int hunt_play(Mach *m, int bullets, int b1, int b2, int b3, int b4, int zone,
              int *pounds, int *bullets_left);

// The language card keeps its RAM between hunts, and so does the hunt: the
// sprites stay mirrored the way they were last drawn, and the walking speed,
// a pending turn and the animals' animation frames carry over.

// What the field holds now, for a presenter that draws it with other art
// (modern mode). Read-only: the game itself runs as above. Boxes are on the
// 280x192 field, inclusive; img is the 1-based image in the original's set.
enum { HV_SCENERY, HV_DEAD, HV_ANIMAL, HV_HUNTER };
typedef struct {
    int kind;                       // HV_*
    int img;                        // scenery 1-15 (HUNT_SCENERY 7-21); dead and animal: the type 0-5
                                    // (buck, bear, buffalo, deer, rabbit, squirrel); hunter: 1-15
    int frame;                      // animal: 0-2; hunter: the walk step 0-3
    int mirrored;                   // drawn mirrored (the original's images face right, the hunter's left)
    int x1, y1, x2, y2;
} HuntSprite;
typedef struct {
    int zone;
    HuntSprite s[24];
    int n;                          // in drawing order
    int shot, sx, sy;               // a shot in flight at (sx, sy)
    int bullets;
    int aim;                        // the way the rifle points, 0 east counting counter-clockwise (REQ-192g)
    int turning;                    // steps of turning still to come
    int walking;                    // the hunter walks (Return toggles it)
} HuntView;
void hunt_view(HuntView *v);
int hunt_running(void);             // a hunt is on the field (hunt_view is meaningful)

// Called before each frame is presented while a hunt runs, so a presenter can
// redraw the field (0: none).
void hunt_set_frame_hook(void (*fn)(void *ctx), void *ctx);

#endif
