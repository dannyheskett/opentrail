// Modern mode's picture of a scene: the MScene drawn on a software canvas 252
// rows high and 384-480 wide (raylib-free; every host shows this canvas).
#ifndef OT_MODERN_DRAW_H
#define OT_MODERN_DRAW_H

#include "modern.h"
#include "canvas.h"

#define MS_CANVAS_H 252             // the art's 216 rows (MS_ART_H) and a band under them
#define MS_CANVAS_W_MIN 384
#define MS_CANVAS_W_MAX 480

// assets/modern/<id>.png, decoded once: compiled in (embed.h), or from
// $OPENTRAIL_ART/<id>.png when that is set, to try new art. NULL if missing.
const CvImage *modern_image(const char *id);

// The canvas width for a w x h window: the window's shape at 252 rows.
int modern_canvas_w(int w, int h);

// Draw sc into cv (already sized). mouse_x/y: the pointer in canvas pixels
// (negative: none), which sets sc->hover; ask: the quit dialog is up; t:
// seconds, for the cursor's blink; version: shown on the opening, or NULL.
void modern_draw(Canvas *cv, MScene *sc, float mouse_x, float mouse_y, int ask, double t, const char *version);

// A tap (or a click) at (x, y) on the canvas last drawn, as the keys it types
// (Apple key codes) into keys[0..max): a choice, a scene button, the touch
// keyboard, the hunt's aim, the travel screen's Return... Returns how many.
// The touch menu opens and closes here.
int modern_tap(MScene *sc, float x, float y, int *keys, int max);
int modern_option_rect(int i, float *x, float *y, float *w, float *h);   // choice i as last drawn
int modern_paused(void);                        // the touch menu is open (the game waits)
void modern_menu_open(void);                    // open it (Android's Back)
int modern_quit_hit(float x, float y);          // the quit dialog: 1 Yes, 2 No, 0 neither

#endif
