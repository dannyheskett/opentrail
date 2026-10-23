// Modern mode's presenter: the scene is drawn on the software canvas
// (src/modern/draw.c, 252 rows high and 384-480 wide, following the window's
// shape), uploaded as one texture and scaled up by whole numbers. A click on a
// choice types its key; a click elsewhere while a message waits is the space
// bar. Esc asks whether to quit, as in legacy mode; Shift+Esc is the Apple's
// Esc.
#include "view_modern.h"
#include "draw.h"
#include "platform_android.h"
#include <raylib.h>
#include <stdio.h>
#include <string.h>

#define CH MS_CANVAS_H

static Canvas s_cv;
static Texture2D s_canvas;                  // the canvas, uploaded each frame
static int s_cw;                            // its width in use
static int s_ask;                           // the quit dialog is up

static int s_touch;                         // taps, not keys: fill the screen

// Where the canvas sits in a w x h target: whole-number scale when it fits
// (filling it, by fractions, with touch, as on a phone).
static void fit(int w, int h, int cw, float *scale, float *dx, float *dy) {
    SafeArea a = host_recording_pass() ? (SafeArea){ 0 } : safe_area();   // clear of a phone's camera and gesture bar
    int x0 = a.left, y0 = a.top;
    w -= a.left + a.right;
    h -= a.top + a.bottom;
    float sc = (float)w / cw < (float)h / CH ? (float)w / cw : (float)h / CH;
    if (sc >= 1 && !host_recording_pass() && !s_touch) sc = (float)(int)sc;   // whole pixels in the window; the video fills its frame
    *scale = sc;
    *dx = x0 + (w - cw * sc) / 2;
    *dy = y0 + (h - CH * sc) / 2;
}

// The scene onto the canvas, once a frame, and the canvas into its texture.
static void prepare(void *ctx) {
    MShell *ms = ctx;
    if (GetTouchPointCount() > 0) ms->sc.touch = 1;     // a touch screen (the web on a phone): from now on
    s_touch = ms->sc.touch;
    ms->sc.sound = ms->m->sound;
    int w = GetScreenWidth(), h = GetScreenHeight();
    SafeArea a = safe_area();
    int cw = modern_canvas_w(w - a.left - a.right, h - a.top - a.bottom);
    cv_size(&s_cv, cw, CH);
    float scale, dx, dy;
    fit(w, h, cw, &scale, &dx, &dy);
    Vector2 mp = GetMousePosition();
    modern_draw(&s_cv, &ms->sc, (mp.x - dx) / scale, (mp.y - dy) / scale, s_ask, GetTime(), ms->m->version);
    if (cw != s_cw) {
        if (s_cw) UnloadTexture(s_canvas);
        Image img = { .data = s_cv.px, .width = cw, .height = CH, .mipmaps = 1, .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
        s_canvas = LoadTextureFromImage(img);
        SetTextureFilter(s_canvas, TEXTURE_FILTER_POINT);
        s_cw = cw;
    } else UpdateTexture(s_canvas, s_cv.px);
}

static void draw(void *ctx, int w, int h) {
    (void)ctx;
    float scale, dx, dy;
    fit(w, h, s_cw, &scale, &dx, &dy);
    DrawTexturePro(s_canvas, (Rectangle){ 0, 0, (float)s_cw, (float)CH },
                   (Rectangle){ dx, dy, s_cw * scale, CH * scale }, (Vector2){ 0, 0 }, 0, WHITE);
}

// Clicks and taps become keys (modern_tap: choices, buttons, the touch
// keyboard and menu); on the quit dialog, its Yes or No.
static void input(void *ctx) {
    MShell *ms = ctx;
    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) return;
    float scale, dx, dy;
    fit(GetScreenWidth(), GetScreenHeight(), s_cw, &scale, &dx, &dy);
    Vector2 mp = GetMousePosition();
    float x = (mp.x - dx) / scale, y = (mp.y - dy) / scale;
    if (s_ask) {
        int q = modern_quit_hit(x, y);
        if (q == 1) host_quit();
        if (q) s_ask = 0;
        return;
    }
    int keys[4], n = modern_tap(&ms->sc, x, y, keys, 4);
    for (int i = 0; i < n; i++) mach_key_push(ms->m, (uint8_t)keys[i]);
}

static int key(void *ctx, int k) {
    (void)ctx;
    if (s_ask) {                                    // 1 or Y quits; 2, N, Esc or Return carry on
        int yes = k == KEY_Y || k == KEY_ONE || k == KEY_KP_1;
        if (yes) host_quit();
        if (yes || k == KEY_N || k == KEY_TWO || k == KEY_KP_2 || k == KEY_ESCAPE || k == KEY_ENTER || k == KEY_KP_ENTER) s_ask = 0;
        return 1;
    }
    if (k == KEY_BACK) {                            // Android's Back: the touch menu
        modern_menu_open();
        return 1;
    }
    if (k == KEY_ESCAPE && !IsKeyDown(KEY_LEFT_SHIFT) && !IsKeyDown(KEY_RIGHT_SHIFT)) {
        s_ask = 1;
        return 1;
    }
    return 0;
}

static int modal(void *ctx) {
    (void)ctx;
    return s_ask || modern_paused();
}

HostPresenter view_modern(MShell *ms) {
    return (HostPresenter){ .prepare = prepare, .draw = draw, .key = key, .modal = modal, .input = input, .ctx = ms };
}

// ---------------------------------------------------------------------------
// The mode chooser's pictures and text (chooser.c draws with raylib)
// ---------------------------------------------------------------------------
#define MAXTEX 16
typedef struct { char id[MS_ID]; Texture2D tex; int ok; } Tex;
static Tex s_tex[MAXTEX];
static int s_ntex;

const struct Texture *view_modern_tex(const char *id) {
    for (int i = 0; i < s_ntex; i++)
        if (!strcmp(s_tex[i].id, id)) return s_tex[i].ok ? &s_tex[i].tex : NULL;
    if (s_ntex >= MAXTEX) return NULL;
    Tex *t = &s_tex[s_ntex++];
    snprintf(t->id, MS_ID, "%s", id);
    const CvImage *ci = modern_image(id);
    if (ci) {
        Image img = { .data = ci->px, .width = ci->w, .height = ci->h, .mipmaps = 1, .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
        t->tex = LoadTextureFromImage(img);
        SetTextureFilter(t->tex, TEXTURE_FILTER_POINT);
        t->ok = t->tex.id != 0;
    }
    return t->ok ? &t->tex : NULL;
}

// Text in ui_font at size (a multiple of 8).
void view_modern_text(const char *s, float x, float y, int size, unsigned rgba) {
    const Texture2D *f = view_modern_tex("ui_font");
    Color c = { (unsigned char)(rgba >> 24), (unsigned char)(rgba >> 16), (unsigned char)(rgba >> 8), (unsigned char)rgba };
    if (!f) { DrawText(s, (int)x, (int)y, size, c); return; }
    for (; *s; s++, x += size) {
        int g = (unsigned char)*s - 32;
        if (g < 0 || g > 94) continue;
        DrawTexturePro(*f, (Rectangle){ (float)(g % 16 * 8), (float)(g / 16 * 8), 8, 8 },
                       (Rectangle){ x, y, (float)size, (float)size }, (Vector2){ 0, 0 }, 0, c);
    }
}
