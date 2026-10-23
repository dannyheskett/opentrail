// The extension's hi-res text driver (& HOOK and the PRINT hook, fonts,
// windows, cursor, indents, spacing, & INP, & SPACE, USR keyboard helpers).
// Implemented in text.c, from the original's machine code.
//
// The original keeps this state at $0300-$030F, $03C0, $653A-$653F,
// $66C0-$66C5, the window table at $68A7 and the row tables at $6540/$6600;
// the fields below are named after those addresses. The font itself stays in
// Mach.ram (the host loads it at $6BEB) because & HSP / & VSP / & TSP rewrite
// its header bytes there, exactly as the original did.
//
// Shared with gfx.c: m->gfx.set[] (the & DFT slots; slot 0 is the font), and
// m->gfx.win / m->gfx.attr, which text.c keeps current whenever it selects a
// window or changes the attribute. The host should point m->gfx.text_flush at
// text_flush and m->gfx.text_cout at text_putc.
#ifndef OT_TEXT_H
#define OT_TEXT_H

#include <stdint.h>

#ifndef OT_MACH_TYPEDEF                 // C99 allows one typedef of a name
#define OT_MACH_TYPEDEF
typedef struct Mach Mach;
#endif

typedef struct {
    int inited;
    int hooked;                 // & HOOK has routed PRINT here
    uint16_t cx;                // $0300/$0301: cursor x within the window
    uint8_t cy;                 // $0302: cursor row within the window
    uint16_t wx, ww;            // $0303-$0306: window left and width (pixels)
    uint8_t wy, wh;             // $0307-$0308: window top and height (rows)
    uint8_t wbit;               // $0309: window left mod 7
    uint8_t indent;             // $030A: & IN (bit 7: hanging indent, wrapped lines only)
    uint8_t pal;                // $030B: $80 after & SHIGH, 0 after & CHIGH
    uint16_t font;              // $030C: current font
    uint8_t font_n;             // $030E: current font slot
    uint8_t win;                // $030F: current window
    uint8_t noparse;            // $03C0 bit 7: & NOPARSE (no line buffering)
    uint8_t met[6];             // $66C0-$66C5: cell width, line height, HSP, VSP, TSP, cursor char
    uint8_t g[5];               // $01-$05: metrics of the last glyph looked up
    uint16_t bw;                // $653A: width of the buffered line (pixels)
    uint8_t bn;                 // $653C: characters buffered
    uint8_t bcont;              // $653D: $80 on a line produced by wrapping
    uint8_t boff;               // $653E: offset (indent or centring) for the buffer
    uint8_t bspace;             // $653F: index of the last space in the buffer
    uint8_t low[256];           // $0400-$04FF: fill pattern ($0400) and line buffer ($0428)
    uint8_t wtab[8][8];         // $68A7: per window x lo/hi, w lo/hi, y, h, buffer lo, page
    uint16_t row[192];          // $6540/$6600: address of each window row (plus left byte)
    uint8_t usr_esc;            // $873E: an Esc was just read by USR
    // Esc Esc (& SPACE, & INP, USR) makes the original GOTO 31000, the main
    // menu. The host sets on_escape (it may longjmp); without it `escaped` is
    // set and the command returns.
    void (*on_escape)(Mach *m);
    int escaped;
} TextState;

void text_init(Mach *m);                        // the extension's state as loaded
void text_putc(Mach *m, int ch);                // one character through the PRINT hook ($6115)
void text_flush(Mach *m);                       // $6365: draw the buffered line
void amp_print(Mach *m, const char *s);         // PRINT: s carries its own "\r"

// & commands; omitted arguments are ARG_NONE
void amp_hook(Mach *m);
void amp_unhook(Mach *m);
void amp_dfw(Mach *m, int n, int x, int y, int w, int h);  // & DFW n AT x,y,w,h
void amp_wind(Mach *m, int n);
void amp_co(Mach *m, int x, int y);             // & CO alone: both ARG_NONE (home)
void amp_gcp(Mach *m, int *x, int *y);          // NULL for an omitted variable
void amp_in(Mach *m, int n);
void amp_hsp(Mach *m, int n, int font);         // & HSP[,n[,font]]
void amp_vsp(Mach *m, int n, int font);
void amp_tsp(Mach *m, int n, int font);
void amp_aft(Mach *m, int n);
void amp_parse(Mach *m);
void amp_noparse(Mach *m);
void amp_cel(Mach *m);
void amp_cew(Mach *m);
void amp_ht(Mach *m, int n);
void amp_vt(Mach *m, int n);
void amp_chigh(Mach *m);
void amp_shigh(Mach *m);
void amp_space(Mach *m, int n);                 // & SPACE[,n]; n >= 1 stays in the current window
// & INP,len,"ranges",flag,var$ (ranges may be NULL, rlen < 0 means strlen).
// Returns the new length of var$; out receives it NUL-terminated.
int  amp_inp(Mach *m, int len, const char *ranges, int flag, char *out, int outlen);
int  amp_inp_n(Mach *m, int len, const char *ranges, int rlen, int flag, char *out, int outlen);

#endif
