// See canvas.h.
#include "canvas.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_IMAGE_STATIC                // private to this file: raylib links its own copy
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_GIF
#define STBI_NO_STDIO               // files are read here (cv_load_png)
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif
#include "../../third_party/stb/stb_image.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

void cv_size(Canvas *cv, int w, int h) {
    if (w * h > cv->cap) {
        free(cv->px);
        cv->px = malloc((size_t)w * h * 4);
        cv->cap = cv->px ? w * h : 0;
    }
    cv->w = cv->px ? w : 0;
    cv->h = cv->px ? h : 0;
}

void cv_clear(Canvas *cv, CvColor c) {
    for (int i = 0; i < cv->w * cv->h; i++) memcpy(cv->px + 4 * i, &c, 4);
}

CvColor cv_fade(CvColor c, float alpha) {
    if (alpha < 0) alpha = 0;
    if (alpha > 1) alpha = 1;
    c.a = (uint8_t)(c.a * alpha + .5f);
    return c;
}

// One pixel: (r, g, b) at alpha a over what is there, alpha included (as a
// GPU's usual blending does, so a see-through layer leaves the canvas partly
// see-through; cv_flatten() then shows it over black).
static inline void blend(Canvas *cv, int x, int y, unsigned r, unsigned g, unsigned b, unsigned a) {
    if (!a) return;
    uint8_t *p = cv->px + 4 * ((size_t)y * cv->w + x);
    if (a == 255) { p[0] = (uint8_t)r; p[1] = (uint8_t)g; p[2] = (uint8_t)b; p[3] = 255; return; }
    p[0] = (uint8_t)((r * a + p[0] * (255 - a) + 127) / 255);
    p[1] = (uint8_t)((g * a + p[1] * (255 - a) + 127) / 255);
    p[2] = (uint8_t)((b * a + p[2] * (255 - a) + 127) / 255);
    p[3] = (uint8_t)((a * a + p[3] * (255 - a) + 127) / 255);
}

void cv_flatten(Canvas *cv) {
    for (int i = 0; i < cv->w * cv->h; i++) {
        uint8_t *p = cv->px + 4 * i;
        unsigned a = p[3];
        if (a == 255) continue;
        p[0] = (uint8_t)((p[0] * a + 127) / 255);
        p[1] = (uint8_t)((p[1] * a + 127) / 255);
        p[2] = (uint8_t)((p[2] * a + 127) / 255);
        p[3] = 255;
    }
}

// The pixels whose centres fall in [a, a + len): first and one past the last.
static void span(float a, float len, int max, int *lo, int *hi) {
    *lo = (int)ceilf(a - .5f);
    *hi = (int)ceilf(a + len - .5f);
    if (*lo < 0) *lo = 0;
    if (*hi > max) *hi = max;
}

void cv_rect(Canvas *cv, float x, float y, float w, float h, CvColor c) {
    int x0, x1, y0, y1;
    span(x, w, cv->w, &x0, &x1);
    span(y, h, cv->h, &y0, &y1);
    for (int j = y0; j < y1; j++)
        for (int i = x0; i < x1; i++) blend(cv, i, j, c.r, c.g, c.b, c.a);
}

void cv_rect_lines(Canvas *cv, float x, float y, float w, float h, CvColor c) {
    cv_rect(cv, x, y, w, 1, c);
    cv_rect(cv, x, y + h - 1, w, 1, c);
    cv_rect(cv, x, y + 1, 1, h - 2, c);
    cv_rect(cv, x + w - 1, y + 1, 1, h - 2, c);
}

// A thick line: the pixels whose centres are within thick / 2 of the segment
// (between its ends, square to it).
void cv_line(Canvas *cv, float x0, float y0, float x1, float y1, float thick, CvColor c) {
    float dx = x1 - x0, dy = y1 - y0, len2 = dx * dx + dy * dy, r = thick / 2;
    if (len2 <= 0) return;
    int bx0 = (int)floorf(fminf(x0, x1) - r), bx1 = (int)ceilf(fmaxf(x0, x1) + r);
    int by0 = (int)floorf(fminf(y0, y1) - r), by1 = (int)ceilf(fmaxf(y0, y1) + r);
    if (bx0 < 0) bx0 = 0;
    if (by0 < 0) by0 = 0;
    if (bx1 > cv->w) bx1 = cv->w;
    if (by1 > cv->h) by1 = cv->h;
    float len = sqrtf(len2);
    for (int j = by0; j < by1; j++)
        for (int i = bx0; i < bx1; i++) {
            float px = i + .5f - x0, py = j + .5f - y0;
            float t = (px * dx + py * dy) / len2;
            if (t < 0 || t >= 1) continue;
            float d = fabsf(px * dy - py * dx) / len;
            if (d < r) blend(cv, i, j, c.r, c.g, c.b, c.a);
        }
}

// An ellipse's outline, a pixel wide, each pixel once.
void cv_ellipse_lines(Canvas *cv, float cx, float cy, float rx, float ry, CvColor c) {
    float rmax = fmaxf(rx, ry);
    int n = (int)(rmax * 8) + 16;
    int lastx = -(1 << 30), lasty = -(1 << 30);
    for (int k = 0; k <= n; k++) {
        float a = (float)k / n * 6.2831853f;
        int x = (int)floorf(cx + sinf(a) * rx), y = (int)floorf(cy + cosf(a) * ry);
        if (x == lastx && y == lasty) continue;
        lastx = x; lasty = y;
        if (x >= 0 && y >= 0 && x < cv->w && y < cv->h) blend(cv, x, y, c.r, c.g, c.b, c.a);
    }
}

// The source rectangle (sx, sy, sw, sh) of img into (x, y, w, h).
static void blit(Canvas *cv, const CvImage *img, int flip, float sx, float sy, float sw, float sh,
                 float x, float y, float w, float h, CvColor tint) {
    if (w <= 0 || h <= 0 || sw <= 0 || sh <= 0) return;
    int x0, x1, y0, y1;
    span(x, w, cv->w, &x0, &x1);
    span(y, h, cv->h, &y0, &y1);
    for (int j = y0; j < y1; j++) {
        int v = (int)floorf(sy + (j + .5f - y) / h * sh);
        if (v < 0) v = 0;
        if (v >= img->h) v = img->h - 1;
        for (int i = x0; i < x1; i++) {
            int u = (int)floorf((i + .5f - x) / w * sw);
            if (flip) u = (int)sw - 1 - u;
            u += (int)sx;
            if (u < 0) u = 0;
            if (u >= img->w) u = img->w - 1;
            const uint8_t *s = img->px + 4 * ((size_t)v * img->w + u);
            unsigned a = s[3] * tint.a / 255;
            if (!a) continue;
            blend(cv, i, j, s[0] * tint.r / 255, s[1] * tint.g / 255, s[2] * tint.b / 255, a);
        }
    }
}

void cv_image(Canvas *cv, const CvImage *img, int flip, float x, float y, float w, float h, CvColor tint) {
    if (!img || !img->px) return;
    blit(cv, img, flip, 0, 0, (float)img->w, (float)img->h, x, y, w, h, tint);
}

void cv_npatch(Canvas *cv, const CvImage *img, int b, float x, float y, float w, float h, CvColor tint) {
    if (!img || !img->px) return;
    float bl = b, bt = b;
    if (w < 2 * b) bl = w / 2;
    if (h < 2 * b) bt = h / 2;
    float iw = (float)img->w, ih = (float)img->h;
    float sxs[3] = { 0, (float)b, iw - b }, sws[3] = { (float)b, iw - 2 * b, (float)b };
    float sys[3] = { 0, (float)b, ih - b }, shs[3] = { (float)b, ih - 2 * b, (float)b };
    float dxs[3] = { x, x + bl, x + w - bl }, dws[3] = { bl, w - 2 * bl, bl };
    float dys[3] = { y, y + bt, y + h - bt }, dhs[3] = { bt, h - 2 * bt, bt };
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 3; c++)
            blit(cv, img, 0, sxs[c], sys[r], sws[c], shs[r], dxs[c], dys[r], dws[c], dhs[r], tint);
}

void cv_text(Canvas *cv, const CvImage *font, const char *s, float x, float y, CvColor c) {
    if (!font || !font->px) return;
    for (; *s; s++, x += CV_FONT) {
        int ch = (unsigned char)*s;
        if (ch < 32 || ch > 126) continue;
        int g = ch - 32;
        blit(cv, font, 0, (float)(g % 16 * 8), (float)(g / 16 * 8), 8, 8, x, y, 8, 8, c);
    }
}

float cv_text_w(const char *s) { return (float)strlen(s) * CV_FONT; }

int cv_decode_png(CvImage *img, const unsigned char *data, int size) {
    int n;
    img->px = stbi_load_from_memory(data, size, &img->w, &img->h, &n, 4);
    return img->px != NULL;
}

int cv_load_png(CvImage *img, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *buf = n > 0 ? malloc((size_t)n) : NULL;
    int ok = buf && fread(buf, 1, (size_t)n, f) == (size_t)n && cv_decode_png(img, buf, (int)n);
    free(buf);
    fclose(f);
    return ok;
}

void cv_image_free(CvImage *img) {
    stbi_image_free(img->px);
    img->px = NULL;
    img->w = img->h = 0;
}
