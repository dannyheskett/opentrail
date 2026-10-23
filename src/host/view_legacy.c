// Legacy mode's presenter: the emulated Apple II hi-res page, rendered at the
// Apple's native 560 dots in clean colour (ntsc.c), lines doubled, scaled by
// whole numbers. Esc asks whether to quit (overlay.c); Shift+Esc is the
// Apple's Esc.
#include "view_legacy.h"
#include "ntsc.h"
#include "overlay.h"
#include <raylib.h>

static Texture2D s_tex;
static int s_ready;
static uint32_t s_px[NTSC_W * NTSC_H];
static int s_ask;                                   // the quit dialog is up

// The page shown (or the quit dialog over it) into the texture, once a frame.
static void prepare(void *ctx) {
    Mach *m = ctx;
    if (!s_ready) {
        Image img = { s_px, NTSC_W, NTSC_H, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
        s_tex = LoadTextureFromImage(img);
        SetTextureFilter(s_tex, TEXTURE_FILTER_POINT);
        s_ready = 1;
    }
    ntsc_render(s_ask ? overlay_quit(m) : m->ram + (m->page2 ? 0x4000 : 0x2000), s_px);
    UpdateTexture(s_tex, s_px);
}

static void draw(void *ctx, int w, int h) {
    (void)ctx;
    float tw = NTSC_W, th = NTSC_H * 2;              // lines doubled: square output pixels
    float scale = w / tw < h / th ? w / tw : h / th;
    if (scale >= 1 && !host_recording_pass()) scale = (float)(int)scale;   // whole-number scale when it fits (the video fills)
    float dw = tw * scale, dh = th * scale;
    float dx = (w - dw) / 2, dy = (h - dh) / 2;
    DrawTexturePro(s_tex, (Rectangle){ 0, 0, NTSC_W, NTSC_H }, (Rectangle){ dx, dy, dw, dh },
                   (Vector2){ 0, 0 }, 0, WHITE);
}

static int key(void *ctx, int k) {
    (void)ctx;
    if (s_ask) {                                    // Y quits; N, Esc or Return carry on
        if (k == KEY_Y) host_quit();
        if (k == KEY_Y || k == KEY_N || k == KEY_ESCAPE || k == KEY_ENTER || k == KEY_KP_ENTER) s_ask = 0;
        return 1;
    }
    switch (k) {
    case KEY_ESCAPE:
        if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) return 0;
        s_ask = 1; return 1;
    }
    return 0;
}

static int modal(void *ctx) { (void)ctx; return s_ask; }

HostPresenter view_legacy(Mach *m) {
    return (HostPresenter){ .prepare = prepare, .draw = draw, .key = key, .modal = modal, .ctx = m };
}
