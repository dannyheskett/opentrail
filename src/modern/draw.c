// Modern mode's picture of a scene, drawn on the software canvas (canvas.h):
// the layout of every screen. See draw.h.
#include "draw.h"
#include "embed.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CH MS_CANVAS_H              // the canvas: the art's 216 rows (MS_ART_H) and a band for its caption
#define FONT CV_FONT                // Press Start 2P: 8 x 8 cells (ui_font)
#define CARVE_X 192.0f              // the tombstone's face (scene_tombstone, art.py draw_tombstone):
#define CARVE_Y 100.0f              // its centre, the first line's top, and its width in characters
#define CARVE_CHARS 17
#define MAXIMG 256
#define CV_MAGENTA ((CvColor){ 255, 0, 255, 255 })

typedef struct { float x, y; } Vec2;
typedef struct { float x, y, width, height; } Rect;

static Canvas *CV;                  // the canvas being drawn
static double s_time;               // seconds, for the cursor's blink

// What a tap can meet, as last drawn (canvas pixels).
static Rect s_opt[MS_OPTS];         // the choices
static int s_nopt;
static Rect s_quit[2];              // the quit dialog's Yes and No
static int s_pause;                 // the touch menu is open
static int s_shift, s_digits;       // the touch keyboard: capitals once, the digits row

// ---------------------------------------------------------------------------
// The pictures
// ---------------------------------------------------------------------------
typedef struct { char id[MS_ID]; CvImage img; int ok; } Img;
static Img s_img[MAXIMG];
static int s_nimg;

const CvImage *modern_image(const char *id) {
    for (int i = 0; i < s_nimg; i++)
        if (!strcmp(s_img[i].id, id)) return s_img[i].ok ? &s_img[i].img : NULL;
    if (s_nimg >= MAXIMG) return NULL;
    Img *m = &s_img[s_nimg++];
    snprintf(m->id, MS_ID, "%s", id);
    // compiled in (build/embed_art.c); OPENTRAIL_ART=DIR loads DIR/<id>.png instead, to try new art
    const char *dir = getenv("OPENTRAIL_ART");
    if (dir && *dir) {
        char path[512];
        snprintf(path, sizeof path, "%s/%s.png", dir, id);
        m->ok = cv_load_png(&m->img, path);
    } else
        for (int i = 0; i < EMBEDDED_ART_COUNT; i++)
            if (EMBEDDED_ART[i].name && !strcmp(EMBEDDED_ART[i].name, id)) {
                m->ok = cv_decode_png(&m->img, EMBEDDED_ART[i].data, EMBEDDED_ART[i].size);
                break;
            }
    return m->ok ? &m->img : NULL;
}

int modern_canvas_w(int w, int h) {
    int cw = h > 0 ? (int)(CH * (float)w / (float)h + .5f) : MS_CANVAS_W_MIN;
    if (cw < MS_CANVAS_W_MIN) cw = MS_CANVAS_W_MIN;
    if (cw > MS_CANVAS_W_MAX) cw = MS_CANVAS_W_MAX;
    return cw;
}

// The text font: Press Start 2P cut into 8 x 8 cells, characters 32-126 in a
// 16 x 6 grid (ui_font; tools/art.py local).
static void text(const char *s, float x, float y, CvColor c) { cv_text(CV, modern_image("ui_font"), s, x, y, c); }
static float text_w(const char *s) { return cv_text_w(s); }
static void rect_rec(Rect r, CvColor c) { cv_rect(CV, r.x, r.y, r.width, r.height, c); }
static int in_rect(Vec2 p, Rect r) { return p.x >= r.x && p.x < r.x + r.width && p.y >= r.y && p.y < r.y + r.height; }

static void image_alpha(const char *id, float x, float y, int flip, float alpha) {
    const CvImage *t = modern_image(id);
    if (!t) { cv_rect_lines(CV, x, y, 16, 16, CV_MAGENTA); return; }
    cv_image(CV, t, flip, x, y, (float)t->w, (float)t->h, cv_fade(CV_WHITE, alpha));
}

static void image(const char *id, float x, float y, int flip) { image_alpha(id, x, y, flip, 1); }

// Draw the text wrapped to width w from (x, y); returns the height used.
static int wrapped(const char *s, float x, float y, float w, CvColor c, int draw) {
    char line[256];
    int n = 0;
    float yy = y;
    const char *p = s;
    while (*p) {
        // take words while they fit
        const char *start = p;
        int len = 0, last_space = -1;
        n = 0;
        while (p[len] && p[len] != '\n') {
            line[n] = p[len]; line[n + 1] = 0;
            if (text_w(line) > w && last_space >= 0) break;
            if (p[len] == ' ') last_space = len;
            n++; len++;
            if (n >= 250) break;
        }
        int take = len;
        if (p[len] && p[len] != '\n' && last_space >= 0) take = last_space;
        snprintf(line, sizeof line, "%.*s", take, start);
        if (draw) text(line, x, yy, c);
        yy += FONT + 2;
        p = start + take;
        while (*p == ' ') p++;
        if (*p == '\n') p++;
    }
    return (int)(yy - y);
}

static void panel_box(Rect r) {
    const CvImage *t = modern_image("ui_panel");
    if (t) {
        cv_npatch(CV, t, 16, r.x, r.y, r.width, r.height, CV_WHITE);
    } else rect_rec(r, (CvColor){ 20, 16, 12, 230 });
    rect_rec((Rect){ r.x + 4, r.y + 4, r.width - 8, r.height - 8 }, (CvColor){ 12, 10, 8, 215 });
}

// The travel backdrop: scaled by the whole number that fills the scene's rows,
// repeated across the canvas, offset by the pan; black below (the status).
static void draw_travel(const MScene *sc, int cw, float ox) {
    const CvImage *t = modern_image(sc->bg);
    if (t) {
        int k = (MS_SCENE_H + t->h - 1) / t->h;
        float bw = (float)t->w * k;
        for (float x = fmodf(sc->pan + ox, bw) - bw; x < cw; x += bw)
            cv_image(CV, t, 0, x, 0, bw, (float)t->h * k, CV_WHITE);
    } else image(sc->bg, ox, 0, 0);                                  // the missing-image mark
    cv_rect(CV, 0, MS_SCENE_H, cw, CH - MS_SCENE_H, CV_BLACK);
}

// The travel screen, as the original's (300-400): the scene, a box on its
// ground under the wagon for a message or a question, the "Press RETURN"
// line under the scene, the status (six lines, the labels' colons in a
// column, as at x 135), and the message's "Press SPACE BAR" at the bottom.
#define TR_BOX_X 38                 // the original's box: 28-244, from y 60 (the wagon's wheels end at 59)
#define TR_BOX_W 297
#define TR_BOX_Y 80
#define TR_PROMPT_Y 131             // "Press RETURN to size up the situation" (111)
#define TR_STATUS_Y 146             // the status (111-180): its six lines from y 153 (125)
#define TR_STATUS_H 76
#define TR_COLON 185                // the labels end here, the values start after (135)
#define BOTTOM_Y 234                // "Press SPACE BAR to continue" (184)

static const CvColor HINT_INK = { 160, 200, 160, 255 };
static const CvColor TITLE_INK = { 255, 220, 120, 255 };

static void draw_status(const MScene *sc, int cw, float ox) {
    static const char *const LABEL[6] = { "Date:", "Weather:", "Health:", "Food:", "Next landmark:", "Miles traveled:" };
    panel_box((Rect){ 4, TR_STATUS_Y, (float)cw - 8, TR_STATUS_H });
    for (int i = 0; i < 6; i++) {
        float y = TR_STATUS_Y + 7 + i * (FONT + 3), lw = text_w(LABEL[i]);
        text(LABEL[i], ox + TR_COLON - lw, y, (CvColor){ 200, 190, 160, 255 });
        text(sc->st[i], ox + TR_COLON + 8, y, (CvColor){ 240, 230, 200, 255 });
    }
}

// Text centred on x, each line of it.
static float centred(const char *s, float x, float y, CvColor c, int draw) {
    char line[160];
    while (*s) {
        int n = (int)strcspn(s, "\n");
        snprintf(line, sizeof line, "%.*s", n < 159 ? n : 159, s);
        if (draw) text(line, (float)(int)(x - text_w(line) / 2), y, c);
        y += FONT + 2;
        s += n; if (*s == '\n') s++;
    }
    return y;
}

// The hunt's expert keys as the original's picture of them (OREGON.IMA 1):
// white keys touching in a 3 x 3 block, the middle empty.
static void draw_keys(float x, float y) {
    static const char K[9] = { 'I', 'O', 'P', 'K', 0, ';', ',', '.', '/' };
    for (int i = 0; i < 9; i++) {
        if (!K[i]) continue;
        float kx = x + (i % 3) * 20, ky = y + (i / 3) * 16;
        cv_rect(CV, (int)kx, (int)ky, 19, 15, (CvColor){ 240, 238, 228, 255 });
        char c[2] = { K[i], 0 };
        text(c, kx + 6, ky + 4, (CvColor){ 20, 16, 12, 255 });
    }
}

// The band under a picture (MS_ART_H to the bottom), as the landmarks'
// caption: the scene's bar (two lines), or a tall panel's hint.
static void draw_band(const MScene *sc, int cw) {
    cv_rect(CV, 0, MS_ART_H, cw, CH - MS_ART_H, (CvColor){ 20, 16, 12, 255 });
    cv_rect(CV, 0, MS_ART_H, cw, 1, (CvColor){ 120, 100, 70, 255 });
    float y1 = MS_ART_H + 7, y2 = MS_ART_H + 21;
    if (!sc->bar[0][0] && !sc->bar[1][0]) y2 = MS_ART_H + 14;     // one line: in the middle
    if (sc->bar[0][0]) text(sc->bar[0], 8, y1, TITLE_INK);
    if (sc->bar[1][0])
        text(sc->bar[1], cw - 8 - text_w(sc->bar[1]), y1, (CvColor){ 240, 230, 200, 255 });
    const char *line2 = sc->bar[2][0] ? sc->bar[2] : sc->panel == PANEL_TALL ? sc->hint : "";
    if (!line2[0]) return;
    if (y2 == MS_ART_H + 14 && text_w(line2) > cw - 16) {   // too long: two lines,
        char a[80], b[80];                                                          // broken at a space
        int n = (int)strlen(line2), cut = n / 2;
        const char *gap = strstr(line2, "  ");                                     // a sentence's end first
        if (gap) cut = (int)(gap - line2);
        else while (cut > 0 && line2[cut] != ' ') cut--;
        if (cut <= 0) cut = n / 2;
        snprintf(a, sizeof a, "%.*s", cut, line2);
        while (line2[cut] == ' ') cut++;
        snprintf(b, sizeof b, "%s", line2 + cut);
        centred(a, cw / 2.0f, MS_ART_H + 7, HINT_INK, 1);
        centred(b, cw / 2.0f, MS_ART_H + 21, HINT_INK, 1);
        return;
    }
    int buttons = 0;                                             // the hunt's buttons at the right: the line at the left
    for (int i = 0; i < sc->nbtn; i++) buttons |= sc->btn[i].label[0] != 0;
    if (buttons) text(line2, 8, y2, HINT_INK);
    else centred(line2, cw / 2.0f, y2, HINT_INK, 1);
}

static float kb_height(const MScene *sc);
static void draw_touch(const MScene *sc, int cw, float ox);

// A choice's text, wrapped to w: its height (lines of FONT + 2).
// With touch, at least a finger's height (TOUCH_ROW).
#define TOUCH_ROW 20
static int s_touch_now;             // the scene being drawn is touched, not typed
static int option_h(const char *s, float w) {
    int h = wrapped(s, 0, 0, w, CV_WHITE, 0);
    return s_touch_now && h < TOUCH_ROW ? TOUCH_ROW : h;
}
static int row_h(void) { return s_touch_now ? TOUCH_ROW : FONT + 3; }

// The panel. A page fills the canvas, the original's text screen: the title
// centred, the dividers when rule is set (at 34 and 210, as at 30 and 160),
// the text, the prompt centred at the bottom. A box on the travel scene sits
// on its ground; its prompt goes to the bottom line. A caption is a band under
// the picture. Elsewhere the box sits at the bottom, a tall panel over the art.
static void draw_panel(MScene *sc, int cw, float ox, Vec2 mouse) {
    if (!sc->panel && !sc->hint[0]) return;
    if (!sc->panel && sc->prompt == MP_CONTINUE) {  // the scene alone: the hint small at the bottom
        Vec2 sz = { text_w(sc->hint), FONT };
        float x = (cw - sz.x) / 2, y = CH - FONT - 8;
        cv_rect(CV, (int)x - 4, (int)y - 2, (int)sz.x + 8, FONT + 4, (CvColor){ 20, 16, 12, 170 });
        text(sc->hint, x, y, HINT_INK);
        return;
    }
    if (!sc->panel && sc->prompt == MP_NONE) {      // a bare hint
        if (sc->travel && sc->status) {             // travelling: the original's line under the scene
            centred(sc->hint, cw / 2.0f, TR_PROMPT_Y, HINT_INK, 1);
            return;
        }
        Vec2 sz = { text_w(sc->hint), FONT };   // the hunt's bullets, the raft: a tag at the top left
        cv_rect(CV, 4, 4, (int)sz.x + 8, FONT + 4, (CvColor){ 20, 16, 12, 170 });
        text(sc->hint, 8, 6, (CvColor){ 230, 225, 200, 255 });
        return;
    }
    if (sc->panel == PANEL_CAPTION) {               // under the picture: its name and the date, then the prompt
        cv_rect(CV, 0, MS_ART_H, cw, 24, (CvColor){ 20, 16, 12, 255 });
        cv_rect(CV, 0, MS_ART_H, cw, 1, (CvColor){ 120, 100, 70, 255 });
        cv_rect(CV, 0, MS_ART_H + 23, cw, 1, (CvColor){ 120, 100, 70, 255 });
        centred(sc->title, cw / 2.0f, MS_ART_H + 3, TITLE_INK, 1);
        centred(sc->body, cw / 2.0f, MS_ART_H + 13, (CvColor){ 240, 230, 200, 255 }, 1);
        if (sc->hint[0]) centred(sc->hint, cw / 2.0f, MS_ART_H + 27, HINT_INK, 1);
        return;
    }
    int page = sc->panel == PANEL_PAGE, tall = sc->panel == PANEL_TALL;
    int trail = sc->panel == PANEL_BOX && sc->travel;
    float px = 8, pw = (float)cw - 16;
    if (page) { px = 4; pw = (float)cw - 8; }
    if (trail) { px = ox + TR_BOX_X; pw = TR_BOX_W; }
    const CvImage *por = sc->overlay[0] ? modern_image(sc->overlay) : NULL;
    if (tall && por && por->h > 100) pw -= por->w + 8;   // a portrait at the right: the panel leaves it room
    // the picture at the top right: the choice under the mouse's, else the panel's
    const char *pid = sc->hover >= 0 && sc->hover < sc->nopts && sc->opt_pic[sc->hover][0] ? sc->opt_pic[sc->hover]
                    : sc->pic[0] ? sc->pic : NULL;
    for (int i = 0; !pid && i < sc->nopts; i++) if (sc->opt_pic[i][0]) pid = "";   // keep the room while choosing
    const CvImage *pt = pid && pid[0] ? modern_image(pid) : NULL;
    int pw_pic = pt ? pt->w + 8 : pid ? 40 : 0; // text keeps clear of it
    const CvImage *div = sc->rule ? modern_image("ui_divider") : NULL;
    float pad = page ? 12 : 8, inner = pw - 2 * pad;
    char optxt[MS_OPTS][100];
    float optw = inner - pw_pic - 12;
    for (int i = 0; i < sc->nopts; i++) {
        snprintf(optxt[i], sizeof optxt[i], "%d. %s", sc->prompt == MP_YESNO ? 1 + i : sc->opt_base + i, sc->opts[i]);
    }
    int hint_out = page || trail || tall;           // the prompt on the bottom line (a tall panel's: in the band)
    int row = trail && (sc->prompt == MP_YESNO || sc->nopts == 2);   // on the trail, two choices side by side (Yes and No: the original reads one word)
    if (row) {                                      // unless they are too wide for the box: then one above the other
        float rw = 0;
        for (int i = 0; i < sc->nopts; i++) rw += (text_w(optxt[i]) + 20 > 64 ? text_w(optxt[i]) + 20 : 64) + 8;
        if (rw - 8 > inner + 4) row = 0;
    }
    // With touch, a long list in two columns (1-4 at the left, 5-8 at the
    // right), so finger-sized rows still fit the screen.
    int cols = s_touch_now && !row && sc->nopts > 6 ? 2 : 1, half = (sc->nopts + 1) / 2;
    float colw = (inner + 4 - pw_pic - 8) / 2;      // a column's button
    float colt = colw - 12;                         // and its text
    float rowh[MS_OPTS] = { 0 };                    // two columns: each row's height
    if (cols == 2)
        for (int i = 0; i < sc->nopts; i++) {
            int oh = option_h(optxt[i], colt);
            if (oh > rowh[i % half]) rowh[i % half] = (float)oh;
        }
    float top, h;
    if (page) { top = 4; h = CH - 8; }
    else {                                          // measure
        h = 8;
        if (sc->title[0]) h += wrapped(sc->title, 0, 0, inner - pw_pic, CV_WHITE, 0) + 4;
        if (div) h += div->h + 4;
        if (sc->body[0]) h += wrapped(sc->body, 0, 0, inner - pw_pic, CV_WHITE, 0) + 4;
        if (sc->nicon) h += 36;
        if (row) h += sc->nopts ? row_h() : 0;
        else if (cols == 2) for (int i = 0; i < half; i++) h += rowh[i] + 1;
        else for (int i = 0; i < sc->nopts; i++) h += option_h(optxt[i], optw) + 1;
        h += sc->nopts ? 4 : 0;
        if (sc->prompt == MP_TEXT) h += FONT + 6;
        if (sc->hint[0] && !hint_out) h += wrapped(sc->hint, 0, 0, inner, CV_WHITE, 0) + 2;
        h += 6;
        const CvImage *ov = sc->overlay[0] ? modern_image(sc->overlay) : NULL;
        if (trail) {                                // on the ground, and clear of the status under the scene
            top = TR_BOX_Y;
            if (top + h > TR_STATUS_Y - 2) top = TR_STATUS_Y - 2 - h;
        }
        else if (tall) {
            top = 6;
            if (ov && ov->h <= 100) top = 6 + ov->h + 4;   // a banner above the panel: start below it
            if (top + h > MS_ART_H - 4) top = MS_ART_H - 4 - h;   // the band under the picture keeps the hint
        } else top = CH - kb_height(sc) - 6 - h;    // above the touch keyboard when it is up
        if (top < 2) top = 2;
    }
    panel_box((Rect){ px, top, pw, h });
    float y = top + 8, x = px + pad;
    if (page) {
        const CvImage *ov = sc->overlay[0] ? modern_image(sc->overlay) : NULL;
        if (ov && ov->h <= 100) {              // a banner heads the page (the Top Ten, the arrival)
            image_alpha(sc->overlay, (float)(int)((cw - ov->w) / 2.0f), y, 0, 1 - sc->overlay_hide);
            y += ov->h + 4;
        }
        if (sc->title[0]) y = centred(sc->title, cw / 2.0f, y, TITLE_INK, 1) + 2;
        if (div) { image("ui_divider", (float)(int)((cw - div->w) / 2.0f), 34, 0); if (y < 58) y = 58; }
        else y += 8;
        if (sc->spot[0]) {                          // the page's picture
            const CvImage *t = modern_image(sc->spot);
            int k = sc->spot_k > 1 ? sc->spot_k : 1;
            if (t) cv_image(CV, t, 0, sc->spot_x, sc->spot_y, (float)t->w * k, (float)t->h * k, CV_WHITE);
        }
        if (sc->keys_x > 0) draw_keys(sc->keys_x, sc->keys_y);
    }
    if (pt) image(pid, px + pw - pad - pt->w, y - 2, 0);
    if (!page && sc->title[0]) y += wrapped(sc->title, x, y, inner - pw_pic, TITLE_INK, 1) + 4;
    if (!page && div) { image("ui_divider", (float)(int)(px + (pw - div->w) / 2), y, 0); y += div->h + 4; }
    if (sc->body[0]) y += wrapped(sc->body, x, y, inner - pw_pic, CV_WHITE, 1) + 4;
    if (sc->nicon) {                                // a row of pictures, centred
        float iw = sc->nicon * 36 - 4, ix = (float)(int)(x + (inner - iw) / 2);
        for (int i = 0; i < sc->nicon; i++) image(sc->icon[i], ix + i * 36, y, 0);
        y += 36;
    }
    sc->hover = -1;
    s_nopt = 0;
    float y0 = y, rx = x - 2;                       // two columns: from the first row; a row's next button
    for (int i = 0; i < sc->nopts; i++) {
        const char *s = optxt[i];
        float tw = cols == 2 ? colt : optw;
        int oh = option_h(s, tw);
        Rect r = { x - 2, y - 1, inner + 4, (float)oh };
        if (row) {
            float bw = text_w(s) + 20 > 64 ? text_w(s) + 20 : 64;
            r = (Rect){ rx, y - 1, bw, (float)oh };
            rx += bw + 8;
        } else if (cols == 2) {
            float ry = y0;
            for (int k = 0; k < i % half; k++) ry += rowh[k] + 1;
            oh = (int)rowh[i % half];
            r = (Rect){ x - 2 + (i / half) * (colw + 8), ry - 1, colw, (float)oh };
        }
        s_opt[s_nopt++] = r;
        int hov = sc->prompt != MP_NONE && in_rect(mouse, r);
        if (hov) sc->hover = i;
        const CvImage *bt = sc->prompt != MP_NONE ? modern_image("ui_button") : NULL;   // the choice's button
        if (bt) {
            cv_npatch(CV, bt, 4, r.x, r.y, r.width - (row || cols == 2 ? 0 : pw_pic), r.height, hov ? CV_WHITE : (CvColor){ 225, 215, 200, 255 });
        } else if (hov) rect_rec(r, (CvColor){ 90, 70, 30, 255 });
        CvColor tc = bt ? (hov ? (CvColor){ 20, 10, 0, 255 } : (CvColor){ 50, 32, 16, 255 })          // dark on the wood
                      : (hov ? (CvColor){ 255, 240, 180, 255 } : (CvColor){ 220, 220, 200, 255 });
        wrapped(s, r.x + 10, r.y + 1 + (oh - wrapped(s, 0, 0, tw, tc, 0)) / 2, tw, tc, 1);   // in the middle of a tall row
        if (!row && cols == 1) y += oh + 1;
    }
    if (cols == 2) for (int k = 0; k < half; k++) y += rowh[k] + 1;
    if (row && sc->nopts) y += row_h();
    if (sc->nopts) y += 4;
    if (sc->prompt == MP_TEXT) {
        char s[80];
        int blink = ((int)(s_time * 2)) & 1;
        snprintf(s, sizeof s, "> %s%s", sc->input, blink ? "_" : "");
        text(s, x, y, (CvColor){ 255, 255, 140, 255 });
        y += FONT + 6;
    }
    if (page && div && y <= 206) image("ui_divider", (float)(int)((cw - div->w) / 2.0f), 210, 0);
    if (tall) draw_band(sc, cw);
    else if (sc->hint[0]) {
        if (hint_out) centred(sc->hint, cw / 2.0f, BOTTOM_Y, HINT_INK, 1);
        else wrapped(sc->hint, x, y, inner, HINT_INK, 1);
    }
}

// The quit dialog, centred over the scene (legacy mode's words, numbered
// as every choice is).
static void draw_quit(int cw) {
    static const char *const L[4] = { "Do you want to quit", "the game?", "", "1. Yes   2. No" };
    float w = 0;
    for (int i = 0; i < 4; i++) {
        float lw = text_w(L[i]);
        if (lw > w) w = lw;
    }
    Rect r = { (float)(int)((cw - w - 32) / 2), (CH - 4 * (FONT + 2) - 24) / 2.0f, (float)(int)w + 32, 4 * (FONT + 2) + 24 };
    panel_box(r);
    for (int i = 0; i < 4; i++)
        text(L[i], r.x + 16, r.y + 12 + i * (FONT + 2), CV_WHITE);
    float ly = r.y + 12 + 3 * (FONT + 2) - 3;                     // "1. Yes   2. No": a click on either
    s_quit[0] = (Rect){ r.x + 12, ly, 7 * FONT, FONT + 6 };
    s_quit[1] = (Rect){ r.x + 12 + 9 * FONT, ly, 6 * FONT, FONT + 6 };
}

// The trail map's route, landmarks and legend, over its background.
static const CvColor ROUTE = { 210, 30, 30, 255 };
static const CvColor INK = { 0, 0, 0, 255 };
static const CvColor RIVER = { 20, 200, 250, 255 };

static void map_label(const char *s, float x, float y) {
    float w = text_w(s);
    cv_rect(CV, (int)x, (int)y, (int)w + 6, FONT + 4, INK);
    cv_rect(CV, (int)x + 1, (int)y + 1, (int)w + 4, FONT + 2, CV_WHITE);
    text(s, x + 3, y + 2, INK);
}

static void map_symbol(int kind, float x, float y) {
    if (kind == MK_FORT) {
        cv_rect(CV, (int)x - 3, (int)y - 3, 7, 7, INK);
        cv_rect(CV, (int)x - 1, (int)y - 1, 3, 3, CV_WHITE);
    } else if (kind == MK_LANDMARK) cv_rect(CV, (int)x - 1, (int)y - 1, 3, 3, INK);
    else {
        cv_rect(CV, (int)x - 2, (int)y, 5, 1, INK);         // a small star
        cv_rect(CV, (int)x, (int)y - 2, 1, 5, INK);
        cv_rect(CV, (int)x - 1, (int)y - 1, 3, 3, INK);
    }
}

static void draw_map(const MScene *sc, int cw, float ox) {
    for (int i = 0; i < sc->nline; i++) {
        const MLine *l = &sc->line[i];
        cv_line(CV, ox + l->x0 + .5f, l->y0 + .5f, ox + l->x1 + .5f, l->y1 + .5f, 2, ROUTE);
    }
    for (int i = 0; i < sc->nmark; i++) {
        const MMark *m = &sc->mark[i];
        map_symbol(m->kind, ox + m->x, m->y);
        if (m->kind == MK_START) map_label("START", ox + m->x - 18, m->y + 5);
        if (m->kind == MK_FINISH) map_label("FINISH", ox + 5, m->y - 7);
    }

    // the legend
    float x = cw - ox - 116, y = 12, w = 112;           // ten characters and a symbol
    panel_box((Rect){ x, y, w, 112 });
    x += 8; y += 8;
    CvColor gold = { 255, 220, 120, 255 }, ink = { 220, 220, 200, 255 };
    text("Map of the", x, y, gold); y += FONT + 2;
    text("Open Trail", x, y, gold); y += FONT + 8;
    static const char *const L[5] = { "Forts", "Landmarks", "Your route", "Rivers", "Mountains" };
    for (int i = 0; i < 5; i++, y += FONT + 4) {
        float sx = x + 5, sy = y + FONT / 2.0f;
        cv_rect(CV, (int)x - 1, (int)y - 1, 13, FONT + 2, CV_WHITE);
        if (i < 2) map_symbol(i == 0 ? MK_FORT : MK_LANDMARK, sx, sy);
        else if (i == 2) cv_rect(CV, (int)x + 1, (int)sy - 1, 9, 2, ROUTE);
        else if (i == 3) { cv_rect(CV, (int)x + 1, (int)sy - 1, 4, 1, RIVER); cv_rect(CV, (int)x + 5, (int)sy, 5, 1, RIVER); }
        else for (int k = 0; k < 3; k++) {                    // a peak
            if (k == 2) cv_rect(CV, (int)x + 5, (int)sy - 1, 1, 1, INK);
            cv_rect(CV, (int)x + 2 + k, (int)sy + 2 - k, 1, 1, INK);
            cv_rect(CV, (int)x + 8 - k, (int)sy + 2 - k, 1, 1, INK);
        }
        text(L[i], x + 16, y, ink);
    }
}

// The scene onto the canvas, once a frame.
void modern_draw(Canvas *cv, MScene *sc, float mouse_x, float mouse_y, int ask, double t, const char *version) {
    CV = cv;
    s_time = t;
    s_touch_now = sc->touch;
    int cw = cv->w;
    Vec2 mouse = { mouse_x, mouse_y };
    if (ask) mouse = (Vec2){ -1, -1 };              // no hover under the dialog
    cv_clear(CV, CV_BLACK);
    float ox = (cw - 384) / 2.0f;
    if (sc->panel == PANEL_PAGE) {                                  // a page is alone on the screen
        draw_panel(sc, cw, ox, mouse);
        draw_touch(sc, cw, ox);
        if (ask) draw_quit(cw);
        cv_flatten(CV);
        return;
    }
    if (sc->travel) draw_travel(sc, cw, ox);
    else if (sc->bg[0]) image_alpha(sc->bg, ox, 0, 0, 1 - sc->bg_hide);
    if (sc->map) draw_map(sc, cw, ox);
    if (sc->ripple > 0)                                             // the crossing's wake
        for (int k = 0; k < 3; k++) {
            float r = fmodf(sc->ripple + k * 9.0f, 27.0f) + 6;
            cv_ellipse_lines(CV, (int)(ox + sc->ripple_x), (int)sc->ripple_y, r * 1.6f * sc->ripple_k, r * .55f * sc->ripple_k, cv_fade(CV_WHITE, .55f - r / 60.0f));
        }
    if (sc->splash)
        for (int k = 0; k < 14; k++) {
            float a = k * .45f, d = (10 + (k * 7) % 13) * sc->ripple_k;
            cv_rect(CV, (int)(ox + sc->ripple_x + cosf(a) * d * 1.4f), (int)(sc->ripple_y - fabsf(sinf(a)) * d - 6 * sc->ripple_k), 2, 2, CV_WHITE);
        }
    if (sc->veil)                                                   // the weather over the scene
        cv_rect(CV, 0, 0, cw, sc->travel ? MS_SCENE_H : MS_ART_H, sc->veil == 1 ? (CvColor){ 20, 24, 40, 110 } : (CvColor){ 235, 240, 250, 120 });
    // the picture's sprites stay in the picture: none in the bands beside a
    // 384-wide scene (the hunt's animals walking on), none under its caption
    if (!sc->travel && sc->bg[0]) cv_clip(CV, ox, 0, 384, MS_ART_H);
    for (int i = 0; i < sc->nspr; i++) {
        const MSprite *p = &sc->spr[i];
        const CvImage *t = modern_image(p->id);
        float k = p->size > 0 ? p->size : p->scale > 1 ? p->scale : 1;
        float x = ox + p->x - (p->right && t ? t->w * k : 0);
        float y = p->y - (p->bottom && t ? t->h * k : 0);
        if (sc->travel && sc->panel == PANEL_BOX && t && p->above_box && y + t->h * k > TR_BOX_Y - 1)
            y = TR_BOX_Y - 1 - t->h * k;            // a landmark reaching under the box (a river, a pool) stands above it
        if (p->anchor && t) {                       // on whole pixels, so the art stays crisp
            x = floorf(x - (p->flip ? t->w - p->ax : p->ax) * k + .5f);
            y = floorf(y - p->ay * k + .5f);
        }
        if (t && k != 1)
            cv_image(CV, t, p->flip, x, y, (float)t->w * k, (float)t->h * k, CV_WHITE);
        else image(p->id, x, y, p->flip);
    }
    if (sc->dot) cv_rect(CV, (int)(ox + sc->dot_x), (int)sc->dot_y, 2, 2, CV_WHITE);   // the hunt's shot
    cv_unclip(CV);
    if (sc->carve[0]) {                             // the words on the tombstone's face, centred, cut dark
        char line[64];
        const char *p = sc->carve;
        float y = CARVE_Y;
        while (*p) {
            int n = (int)strcspn(p, "\n");
            if (n > CARVE_CHARS) {                  // too wide for the face: break at the last space that fits
                int cut = CARVE_CHARS;
                while (cut > 0 && p[cut] != ' ') cut--;
                n = cut > 0 ? cut : CARVE_CHARS;
            }
            snprintf(line, sizeof line, "%.*s", n < 63 ? n : 63, p);
            float w = text_w(line);
            text(line, (float)(int)(ox + CARVE_X - w / 2) + 1, y + 1, (CvColor){ 230, 230, 225, 140 });
            text(line, (float)(int)(ox + CARVE_X - w / 2), y, (CvColor){ 40, 40, 44, 255 });
            y += FONT + 3;
            p += n; if (*p == '\n' || *p == ' ') p++;
        }
    }
    if (sc->overlay[0]) {
        const CvImage *t = modern_image(sc->overlay);
        if (t) {
            if (t->h > 100) image(sc->overlay, cw - t->w - 12.0f, MS_ART_H - t->h - 4.0f, 0);   // a portrait: on the floor
            else image_alpha(sc->overlay, (cw - t->w) / 2.0f, 6, 0, 1 - sc->overlay_hide);
        }
    }
    if (sc->status && sc->travel) draw_status(sc, cw, ox);         // under the travel scene; a box may cover it
    if (!sc->travel && (sc->bar[0][0] || sc->bar[1][0] || sc->bar[2][0])) draw_band(sc, cw);
    draw_panel(sc, cw, ox, mouse);
    if (sc->version && version) {           // the opening: the version at the bottom right, as the
        float vx = cw - text_w(version) - 6, vy = CH - FONT - 8;   // prompt's line
        text(version, vx + 1, vy + 1, CV_BLACK);
        text(version, vx, vy, CV_WHITE);
    }
    draw_touch(sc, cw, ox);
    if (ask) draw_quit(cw);
    cv_flatten(CV);
}


// ---------------------------------------------------------------------------
// Touch: the keyboard, the scene's buttons, the menu, and taps into keys
// ---------------------------------------------------------------------------
#define KB_SHIFT (-1)
#define KB_DIGITS (-2)
#define KB_KEY_H 20
#define KB_GAP 2
#define KB_LETTERS_H (4 * (KB_KEY_H + KB_GAP) + 6)
#define KB_NUMBERS_H (KB_KEY_H + 2 * KB_GAP + 6)

typedef struct { const char *label; int key; int units; } KbKey;
static Rect s_kb_rect[48];
static int s_kb_code[48], s_nkb;
static Rect s_btn_menu;

static int only_digits(const MScene *sc) { return !strcmp(sc->allow, "-09"); }
static int allows(const MScene *sc, const char *what) { return strstr(sc->allow, what) != NULL; }

static float kb_height(const MScene *sc) {
    if (!sc->touch || sc->prompt != MP_TEXT) return 0;
    return only_digits(sc) ? KB_NUMBERS_H : KB_LETTERS_H;
}

// A key or a button: the wood, the label in the middle.
static void button(Rect r, const char *label, int lit) {
    const CvImage *bt = modern_image("ui_button");
    if (bt) cv_npatch(CV, bt, 4, r.x, r.y, r.width, r.height, lit ? CV_WHITE : (CvColor){ 225, 215, 200, 255 });
    else rect_rec(r, (CvColor){ 120, 90, 50, 255 });
    text(label, (float)(int)(r.x + (r.width - text_w(label)) / 2), (float)(int)(r.y + (r.height - FONT) / 2), (CvColor){ 40, 26, 12, 255 });
}

// Capitals at a word's start, or once after Shift.
static int kb_capital(const MScene *sc) {
    size_t n = strlen(sc->input);
    return s_shift ^ (n == 0 || sc->input[n - 1] == ' ');
}

static void kb_row(const KbKey *row, int n, float x, float y, float unit) {
    for (int i = 0; i < n; i++) {
        float w = unit * row[i].units - KB_GAP;
        Rect r = { x, y, w, KB_KEY_H };
        button(r, row[i].label, row[i].key == KB_SHIFT && s_shift);
        if (s_nkb < 48) { s_kb_rect[s_nkb] = r; s_kb_code[s_nkb++] = row[i].key; }
        x += unit * row[i].units;
    }
}

static void draw_keyboard(const MScene *sc, int cw) {
    s_nkb = 0;
    float h = kb_height(sc);
    if (h <= 0) return;
    cv_rect(CV, 0, CH - h, cw, h, (CvColor){ 20, 16, 12, 235 });
    cv_rect(CV, 0, CH - h, cw, 1, (CvColor){ 120, 100, 70, 255 });
    float y = CH - h + 4;
    if (only_digits(sc)) {                       // a number: one row
        static const KbKey NUM[12] = { { "1", '1', 1 }, { "2", '2', 1 }, { "3", '3', 1 }, { "4", '4', 1 }, { "5", '5', 1 },
            { "6", '6', 1 }, { "7", '7', 1 }, { "8", '8', 1 }, { "9", '9', 1 }, { "0", '0', 1 }, { "Del", 0x08, 1 }, { "OK", 0x0D, 1 } };
        float unit = (cw - 4.0f) / 12;
        kb_row(NUM, 12, 2 + KB_GAP / 2.0f, y, unit);
        return;
    }
    float unit = (float)(int)((cw - 4) / 10), x = (float)(int)((cw - unit * 10) / 2) + KB_GAP / 2.0f;
    int cap = kb_capital(sc);
    static const char *const ROWS[3] = { "QWERTYUIOP", "ASDFGHJKL'", "ZXCVBNM.-" };
    KbKey row[10];
    for (int r = 0; r < 3; r++) {
        int n = 0;
        if (r == 2) row[n++] = (KbKey){ "^", KB_SHIFT, 1 };
        const char *keys = r == 0 && s_digits ? "1234567890" : ROWS[r];
        for (int i = 0; keys[i]; i++) {
            char c = keys[i];
            if (c >= 'A' && c <= 'Z' && !cap) c = (char)(c - 'A' + 'a');
            static char one[3][10][2];
            one[r][i][0] = c; one[r][i][1] = 0;
            row[n++] = (KbKey){ one[r][i], (unsigned char)keys[i], 1 };   // the case is the tap's (modern_tap)
        }
        kb_row(row, n, x, y + r * (KB_KEY_H + KB_GAP), unit);
    }
    int n = 0;
    if (allows(sc, "-09")) row[n++] = (KbKey){ s_digits ? "ABC" : "123", KB_DIGITS, 1 };
    else row[n++] = (KbKey){ "", 0, 1 };
    row[n++] = (KbKey){ ",", ',', 1 };
    row[n++] = (KbKey){ "Space", ' ', 4 };
    row[n++] = (KbKey){ "Del", 0x08, 2 };
    row[n++] = (KbKey){ "OK", 0x0D, 2 };
    kb_row(row, n, x, y + 3 * (KB_KEY_H + KB_GAP), unit);
}

// The scene's buttons (labelled ones; the others are areas only).
static void draw_buttons(const MScene *sc, float ox) {
    for (int i = 0; i < sc->nbtn; i++)
        if (sc->btn[i].label[0])
            button((Rect){ ox + sc->btn[i].x, sc->btn[i].y, sc->btn[i].w, sc->btn[i].h }, sc->btn[i].label, 0);
}

// The menu button (three bars, top right) and, when open, the menu.
static const char *menu_item(const MScene *sc, int i) {
    if (i == 0) return "Continue";
    if (i == 1) return sc->sound ? "Turn sound off" : "Turn sound on";
    return sc->hunting ? "Stop hunting" : sc->in_game ? "Quit to the main menu" : "Management options";
}

static void draw_menu(const MScene *sc, int cw) {
    s_btn_menu = (Rect){ cw - 24.0f, 2, 22, 18 };
    cv_rect(CV, s_btn_menu.x, s_btn_menu.y, s_btn_menu.width, s_btn_menu.height, (CvColor){ 20, 16, 12, 170 });
    for (int k = 0; k < 3; k++) cv_rect(CV, s_btn_menu.x + 5, s_btn_menu.y + 4 + k * 4, 12, 2, (CvColor){ 240, 230, 200, 255 });
    if (!s_pause) return;
    cv_rect(CV, 0, 0, cw, CH, (CvColor){ 0, 0, 0, 120 });
    Rect r = { (float)(int)((cw - 220) / 2.0f), 60, 220, 118 };
    panel_box(r);
    text("Menu", (float)(int)(r.x + (r.width - text_w("Menu")) / 2), r.y + 10, TITLE_INK);
    for (int i = 0; i < 3; i++) {
        char s[40];
        snprintf(s, sizeof s, "%d. %s", i + 1, menu_item(sc, i));
        Rect b = { r.x + 12, r.y + 28 + i * 28, r.width - 24, 24 };
        s_opt[i] = b;
        button(b, s, 0);
    }
}

int modern_paused(void) { return s_pause; }

int modern_option_rect(int i, float *x, float *y, float *w, float *h) {
    if (i < 0 || i >= s_nopt) return 0;
    *x = s_opt[i].x; *y = s_opt[i].y; *w = s_opt[i].width; *h = s_opt[i].height;
    return 1;
}
void modern_menu_open(void) { s_pause = 1; }

int modern_quit_hit(float x, float y) {
    Vec2 p = { x, y };
    return in_rect(p, s_quit[0]) ? 1 : in_rect(p, s_quit[1]) ? 2 : 0;
}

int modern_tap(MScene *sc, float x, float y, int *keys, int max) {
    Vec2 p = { x, y };
    float ox = (CV ? CV->w - 384 : 0) / 2.0f;
    int n = 0;
    if (max < 2) return 0;
    if (s_pause) {                               // the menu: its items, or a tap elsewhere closes it
        for (int i = 0; i < 3; i++)
            if (in_rect(p, s_opt[i])) {
                if (i == 1) keys[n++] = 0x13;                         // Ctrl-S
                if (i == 2) {
                    if (sc->in_game || sc->hunting) { keys[n++] = 0x1B; keys[n++] = 0x1B; }   // Esc Esc
                    else keys[n++] = 0x01;                            // Ctrl-A: the management options
                }
                break;
            }
        s_pause = 0;
        return n;
    }
    if (sc->touch && in_rect(p, s_btn_menu)) { s_pause = 1; return 0; }
    if (sc->touch && sc->prompt == MP_TEXT)      // the keyboard
        for (int i = 0; i < s_nkb; i++)
            if (in_rect(p, s_kb_rect[i])) {
                int k = s_kb_code[i];
                if (k == KB_SHIFT) s_shift = !s_shift;
                else if (k == KB_DIGITS) s_digits = !s_digits;
                else if (k) {
                    if (k >= 'A' && k <= 'Z' && !kb_capital(sc)) k += 'a' - 'A';
                    keys[n++] = k;
                    if (k != 0x08 && k != ' ') s_shift = 0;
                }
                return n;
            }
    if (sc->prompt == MP_CHOOSE || sc->prompt == MP_YESNO)
        for (int i = 0; i < s_nopt && i < sc->nopts; i++)
            if (in_rect(p, s_opt[i])) { keys[n++] = modern_option_key(sc, i); return n; }
    for (int i = 0; i < sc->nbtn; i++) {
        const MButton *b = &sc->btn[i];
        Rect r = { ox + b->x, b->y, b->w, b->h };
        if (!b->label[0]) {                       // an area at the 384 columns' edge reaches the canvas's
            float x0 = b->x <= 0 ? 0 : ox + b->x, x1 = b->x + b->w >= 384 ? (float)CV->w : ox + b->x + b->w;
            r = (Rect){ x0, b->y, x1 - x0, b->h };
        }
        if (in_rect(p, r)) { keys[n++] = b->key; return n; }
    }
    if (sc->hunting && y < MS_ART_H) {          // the field: a tap walks there, a second tap soon after shoots
        static double last_t = -1;
        static float last_x, last_y;
        int dbl = s_time - last_t < 0.4 && fabsf(x - last_x) < 32 && fabsf(y - last_y) < 32;
        sc->hunt_goal = dbl ? 2 : 1;
        sc->hunt_gx = x - ox; sc->hunt_gy = y;
        last_t = dbl ? -1 : s_time; last_x = x; last_y = y;
        return 0;
    }
    if (sc->prompt == MP_CONTINUE) { keys[n++] = ' '; return n; }
    if (sc->tap_key) keys[n++] = sc->tap_key;
    return n;
}

// Over the scene: its buttons, the keyboard, the menu (touch only).
static void draw_touch(const MScene *sc, int cw, float ox) {
    s_nkb = 0;
    draw_buttons(sc, ox);
    if (!sc->touch) { s_pause = 0; return; }
    draw_keyboard(sc, cw);
    draw_menu(sc, cw);
}
