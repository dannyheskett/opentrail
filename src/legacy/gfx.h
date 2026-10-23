// The extension's graphics commands (& BOX, & DFT, & IMAGE, & UIM, & CST,
// & DUN, & RFL, & TAKE, & PUT, & DPC) and the BASIC's HGR2 / HCOLOR / HPLOT.
// Implemented in gfx.c; the extension's commands from its machine code.
//
// Addresses are the ones the BASIC passes. The language-card mapping ($7BED,
// spec REQ-018c) sends $C000-$CFFF to bank 1 and $D000+ to bank 2; Mach.ram
// keeps bank 1 at $C000-$CFFF and bank 2 at $D000-$FFFF, so the mapping is the
// identity here.
//
// Shared with the text driver (text.c): the & DFT slot table (slot 0 is the
// font, 7 the image set, 15 the tune), and the current window, whose origin
// & BOX coordinates are relative to. The original keeps these at $0310-$032F
// and $0303-$0309; here they live in GfxState, and text.c must keep `win` and
// `attr` current when it selects a window or changes the attribute.
#ifndef OT_GFX_H
#define OT_GFX_H

#include <stdint.h>

#ifndef OT_MACH_TYPEDEF                 // C99 allows one typedef of a name
#define OT_MACH_TYPEDEF
typedef struct Mach Mach;
#endif

typedef struct {
    int x, w, y, h;     // $0303-$0308: left, width, top, height (pixels)
    int page;           // hi-res page ($20 / $40) the window was defined on, or a buffer's high byte
} GfxWindow;

typedef struct {
    uint16_t set[16];               // $0310+2n: & DFT slots
    uint16_t shift_base;            // $032A: & CST tables (6 x 256 bytes)
    GfxWindow win;                  // $0303-$0308 + page: the current text window
    uint8_t attr;                   // $030B: text attribute, & BOX's default colour
    // Hooks into the text driver (set by the host; NULL = none):
    void (*text_flush)(Mach *m);            // $6365, run before & BOX / & DFT like every driver control
    void (*text_cout)(Mach *m, int ch);     // COUT: & BOX with no arguments prints $82
    // HPLOT (gfx.c):
    uint8_t hcolor;                 // HCOLOR='s byte pattern
    int hx, hy;                     // the last point plotted
} GfxState;

void gfx_init(Mach *m);                     // the state after & HOOK: window 0 = full page, font slot 0
uint16_t gfx_set_addr(const Mach *m, int n);    // address registered by & DFT n

// & commands; omitted arguments are ARG_NONE
void amp_box(Mach *m, int x1, int y1, int x2, int y2, int colour);
void amp_dft(Mach *m, int n, int addr);
void amp_image(Mach *m, int n, int x, int y);
void amp_uim(Mach *m, int n, int addr, int *len);
void amp_cst(Mach *m, int addr);
int  amp_dun(Mach *m, const char *file, int addr, int pages);  // 0 ok, else DOS error (6 not found)
int  amp_rfl(Mach *m, const char *file, int addr);             // load a binary file's body at addr
void amp_take(Mach *m, int x1, int y1, int x2, int y2, int addr);
void amp_put(Mach *m, int x, int y, int addr);
void amp_dpc(Mach *m, int x, int y, int *colour);

// Image-set helpers for the host and the hunt/raft code.
int  image_size(const Mach *m, int n, int *w, int *h);         // 1 if image n exists in slot 7's set

#endif
