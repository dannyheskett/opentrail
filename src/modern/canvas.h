// A small software canvas: modern mode draws its scene into an RGBA buffer
// with these, so every platform (raylib's, iOS's Metal) shows the same pixels
// and only has to put one texture on the screen. raylib-free.
//
// Shapes and images cover the pixels whose centres fall inside them (as a GPU
// rasterises a quad), images are sampled nearest, and colour and alpha are
// blended by alpha over what is there; cv_flatten() shows the result over
// black, opaque (as the window always showed it).
#ifndef OT_CANVAS_H
#define OT_CANVAS_H

#include <stdint.h>

typedef struct { uint8_t r, g, b, a; } CvColor;

typedef struct {
    int w, h;
    uint8_t *px;                    // RGBA, w * h * 4
} CvImage;

typedef struct {
    int w, h, cap;                  // cap: the pixels allocated
    uint8_t *px;                    // RGBA, opaque
} Canvas;

#define CV_WHITE ((CvColor){ 255, 255, 255, 255 })
#define CV_BLACK ((CvColor){ 0, 0, 0, 255 })

void cv_size(Canvas *cv, int w, int h);         // (re)allocate; contents undefined
void cv_clear(Canvas *cv, CvColor c);
void cv_flatten(Canvas *cv);                    // over black: every pixel opaque
CvColor cv_fade(CvColor c, float alpha);

void cv_rect(Canvas *cv, float x, float y, float w, float h, CvColor c);
void cv_rect_lines(Canvas *cv, float x, float y, float w, float h, CvColor c);   // 1 pixel inside the edge
void cv_line(Canvas *cv, float x0, float y0, float x1, float y1, float thick, CvColor c);
void cv_ellipse_lines(Canvas *cv, float cx, float cy, float rx, float ry, CvColor c);

// img drawn into (x, y, w, h), mirrored when flip, its colours times tint.
void cv_image(Canvas *cv, const CvImage *img, int flip, float x, float y, float w, float h, CvColor tint);
// img as a nine-patch with border b on each side, stretched to (x, y, w, h).
void cv_npatch(Canvas *cv, const CvImage *img, int b, float x, float y, float w, float h, CvColor tint);

// Text in a fixed-width 8 x 8 font: characters 32-126 in a 16 x 6 grid
// (ui_font). Characters outside it are skipped.
#define CV_FONT 8
void cv_text(Canvas *cv, const CvImage *font, const char *s, float x, float y, CvColor c);
float cv_text_w(const char *s);

// PNG bytes to an image (stb_image); 0 on failure. cv_image_free releases it.
int cv_decode_png(CvImage *img, const unsigned char *data, int size);
int cv_load_png(CvImage *img, const char *path);
void cv_image_free(CvImage *img);

#endif
