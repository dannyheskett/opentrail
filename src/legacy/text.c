// The extension's hi-res text driver, ported from the machine code at
// $60CD-$6BDE, $7024-$710A, $7157-$72C3 and the glyph / fill blitters at
// $72C4-$7769. See text.h. Addresses in comments are the original routines.
#include "mach.h"
#include <string.h>

#define TS (&m->text)

// Masks: $7602 (first byte of a glyph, then the last-byte masks from $7609),
// $775B / $7763 (the fill's first and last bytes), $776B (colour swap).
static const uint8_t M7602[14] = { 0xFF, 0xFE, 0xFC, 0xF8, 0xF0, 0xE0, 0xC0,
                                   0x81, 0x83, 0x87, 0x8F, 0x9F, 0xBF, 0xFF };
static const uint8_t M775B[8] = { 0xFF, 0xFE, 0xFC, 0xF8, 0xF0, 0xE0, 0xC0, 0x80 };
static const uint8_t M7763[7] = { 0x81, 0x83, 0x87, 0x8F, 0x9F, 0xBF, 0xFF };
static const uint8_t SWAP776B[4] = { 0, 2, 1, 3 };

static uint8_t rd(Mach *m, unsigned a) { return m->ram[a & 0xFFFF]; }
static void wr(Mach *m, unsigned a, uint8_t v) { m->ram[a & 0xFFFF] = v; }

// $709C: pixel x (16-bit) to byte column; *rem = x mod 7.
static int div7(unsigned x, int *rem) {
    unsigned lo = x & 0xFF, base = 0, a = lo;
    if (x & 0xFF00) { a = (lo - 0xE0) & 0xFF; base = 0x20; }
    static const unsigned t[6] = { 7, 14, 28, 56, 112, 224 };
    unsigned q = 0;
    for (int i = 5; i >= 0; i--) {
        q <<= 1;
        if (a >= t[i]) { a -= t[i]; q |= 1; }
    }
    if (rem) *rem = (int)a;
    return (int)((base + q) & 0xFF);
}

// ---------------------------------------------------------------------------
// Windows
// ---------------------------------------------------------------------------
void text_init(Mach *m) {
    TextState *t = TS;
    memset(t, 0, sizeof *t);
    t->inited = 1;
    for (int n = 0; n < 8; n++) {                   // as loaded: every window the full page
        uint8_t w[8] = { 0x00, 0x00, 0x18, 0x01, 0x00, 0xC0, 0x00, 0x40 };
        memcpy(t->wtab[n], w, 8);
    }
    t->font = 0x6BEB;
    m->gfx.text_flush = text_flush;                 // & BOX flushes and prints through here
    m->gfx.text_cout = text_putc;
}

// $7024: the address of each window row, plus the window's first byte.
static void build_rows(Mach *m, uint8_t b6, uint8_t page) {
    TextState *t = TS;
    if (page == 0x40 || page == 0x20) {
        int col = div7(t->wx, NULL);
        for (int i = 0; i < t->wh && i < 192; i++) {
            int y = (t->wy + i) & 0xFF;
            // lo = row lo + column (no carry into the high byte), hi = row hi | page
            uint16_t r = hires_row(y & 0xFF);
            uint8_t lo = (uint8_t)((r & 0xFF) + col);
            uint8_t hi = (uint8_t)(((r >> 8) & 0x1F) | page);
            t->row[i] = (uint16_t)(lo | hi << 8);
        }
        return;
    }
    // a buffer: rows of (width / 7) bytes from b6 | page << 8
    int stride = div7(t->ww, NULL);
    unsigned a = (unsigned)(b6 | page << 8);
    for (int i = 0; i < t->wh && i < 192; i++) {
        if (i) a = (a + (unsigned)stride) & 0xFFFF;
        t->row[i] = (uint16_t)a;
    }
}

static void sync_gfx(Mach *m) {
    TextState *t = TS;
    m->gfx.win.x = t->wx; m->gfx.win.w = t->ww;
    m->gfx.win.y = t->wy; m->gfx.win.h = t->wh;
    m->gfx.win.page = t->wtab[t->win & 7][7];
    m->gfx.attr = t->pal;
}

static void home(Mach *m) { TS->cy = 0; TS->cx = 0; }         // $64B8

// $6939: make window n current, home the cursor.
static void select_window(Mach *m, int n) {
    TextState *t = TS;
    t->win = (uint8_t)n;
    const uint8_t *w = t->wtab[n & 7];
    t->wx = (uint16_t)(w[0] | w[1] << 8);
    t->ww = (uint16_t)(w[2] | w[3] << 8);
    t->wy = w[4];
    t->wh = w[5];
    build_rows(m, w[6], w[7]);
    int rem;
    div7(t->wx, &rem);
    t->wbit = (uint8_t)rem;
    home(m);
    sync_gfx(m);
}

// ---------------------------------------------------------------------------
// Fonts
// ---------------------------------------------------------------------------
static uint16_t slot(Mach *m, int n) {
    return n == 0 ? (m->gfx.set[0] ? m->gfx.set[0] : 0x6BEB) : m->gfx.set[n & 15];
}

// $622F: the current font's header into $66C0-$66C5.
static void load_metrics(Mach *m) {
    TextState *t = TS;
    for (int i = 0; i < 5; i++) t->met[i] = rd(m, t->font + 3u + (unsigned)i);
    t->met[5] = rd(m, t->font);
}

// $66DF: character ch's record into $01-$05.
static void glyph_metrics(Mach *m, int ch) {
    TextState *t = TS;
    unsigned idx = (unsigned)((ch - rd(m, t->font + 1u)) & 0xFF);
    unsigned rec = (t->font + idx * 5u + 8u) & 0xFFFF;
    for (int i = 0; i < 5; i++) t->g[i] = rd(m, rec + (unsigned)i);
}

// ---------------------------------------------------------------------------
// The blitters
// ---------------------------------------------------------------------------
static int row_addr(Mach *m, int r, unsigned *a) {
    if (r < 0 || r >= 192) return 0;                // beyond the row tables: not drawn
    *a = m->text.row[r];
    return 1;
}

static void put_masked(Mach *m, unsigned a, uint8_t v, uint8_t mask) {
    uint8_t d = rd(m, a);
    wr(m, a, (uint8_t)(((v ^ d) & mask) ^ d));
}

// $762A: draw character ch (bit 7 = inverse) at the cursor, pal = $030B.
static void draw_glyph(Mach *m, int ch, int pal) {
    TextState *t = TS;
    uint8_t eor = (ch & 0x80) ? 0x7F : 0x00;        // $93
    glyph_metrics(m, ch & 0x7F);
    uint8_t w = t->g[0], fl = t->g[1], h = t->g[2];
    if (!h) return;
    int bit;
    int col = div7((unsigned)(t->cx + t->wbit) & 0xFFFF, &bit);     // $88, $89
    int n = fl >> 5;                                                // $94: bytes per row - 1
    uint8_t r0 = (uint8_t)((fl & 0x1F) + t->met[4] + t->cy);        // $87
    uint8_t r1 = (uint8_t)(r0 + h);                                 // $86
    unsigned src = (unsigned)(t->g[3] | t->g[4] << 8) + t->font;    // $95/$96
    int s = bit + w - (7 * n + 7);
    int spill, last;                                                // $99, $98
    if (s >= 0) { spill = s & 0xFF; last = 8; }
    else { spill = 0; last = s & 7; }

    if (s < 0 && n == 0) {                          // $72C4: one byte per row
        uint8_t mask = (uint8_t)(M7602[bit] & M7602[5 + last]);
        unsigned x = 0;
        uint8_t r = r0;
        do {
            unsigned a;
            uint8_t v = (uint8_t)(rd(m, src + x) ^ eor);
            if (bit) v = (uint8_t)(((unsigned)v << bit) & 0x7F);
            v |= (uint8_t)pal;
            if (row_addr(m, r, &a)) put_masked(m, a + (unsigned)col, v, mask);
            x++;
            r++;
        } while (r != r1);
        return;
    }
    uint8_t lastmask = M7602[5 + last];
    if (!bit) {                                     // $734E: byte-aligned rows
        uint8_t r = r0;
        do {
            unsigned a;
            int have = row_addr(m, r, &a);
            unsigned y = (unsigned)col;
            for (int x = n; ; x--) {
                uint8_t v = (uint8_t)((rd(m, src + (unsigned)x) ^ eor) | pal);
                if (x == 0) { if (have) put_masked(m, a + y, v, lastmask); break; }
                if (have) wr(m, a + y, v);
                y++;
            }
            src += (unsigned)n + 1;
            r++;
        } while (r < r1);
        return;
    }
    uint8_t keep = M7602[6 + bit];                  // $739C: shifted rows
    uint8_t spillmask = M7602[6 + (spill > 7 ? 7 : spill)];
    uint8_t r = r0;
    do {
        unsigned a;
        int have = row_addr(m, r, &a);
        unsigned y = (unsigned)col;
        uint8_t carry = (uint8_t)((have ? (rd(m, a + y) & keep) : 0) | pal);     // $9A
        uint8_t z;
        for (int x = n; ; x--) {
            uint8_t o = (uint8_t)(rd(m, src + (unsigned)x) ^ eor);
            z = (uint8_t)((((unsigned)o << bit) & 0x7F) | carry);
            carry = (uint8_t)((o >> (7 - bit)) | pal);
            if (x == 0) break;
            if (have) wr(m, a + y, z);
            y++;
        }
        if (have) {                                 // $73B7
            put_masked(m, a + y, z, lastmask);
            if (spill) put_masked(m, a + y + 1, carry, spillmask);
        }
        src += (unsigned)n + 1;
        r++;
    } while (r < r1);
}

// $76A9: fill window-relative x1..x2 (16-bit), rows y1..y2 with colour c
// (bits 0-1 the colour, bit 7 the palette), through the window's row table.
static void fill(Mach *m, unsigned x1, int y1, int y2, unsigned x2, int c) {
    TextState *t = TS;
    int ci = c & 3;
    uint8_t hi = (uint8_t)(c & 0x80);
    if (!(div7(t->wx, NULL) & 1)) ci = SWAP776B[ci];
    // $70CE: 40 bytes of the colour's pixel pattern at $0400
    for (int b = 0; b < 40; b++) {
        uint8_t v = 0;
        for (int k = 0; k < 7; k++) {
            int s = 7 * b + k;
            v |= (uint8_t)(((ci >> (s & 1)) & 1) << k);
        }
        t->low[b] = v | hi;
    }
    int r1, r2;
    int col1 = div7((x1 + t->wbit) & 0xFFFF, &r1);
    int col2 = div7((x2 + t->wbit) & 0xFFFF, &r2);
    uint8_t fm = M775B[r1], lm = M7763[r2];
    unsigned x = (unsigned)y1 & 0xFF;
    for (int guard = 0; guard < 256; guard++) {
        unsigned a;
        if (row_addr(m, (int)x, &a)) {
            unsigned y = (unsigned)col1;
            uint8_t v = (uint8_t)((rd(m, a + y) ^ t->low[y & 0xFF]) & fm);
            if (y < (unsigned)col2) {
                wr(m, a + y, (uint8_t)(v ^ rd(m, a + y)));
                y = (y + 1) & 0xFF;
                while (y != (unsigned)col2) {
                    wr(m, a + y, t->low[y]);
                    y = (y + 1) & 0xFF;
                }
                v = (uint8_t)(t->low[y] ^ rd(m, a + y));
            }
            v &= lm;
            wr(m, a + y, (uint8_t)(v ^ rd(m, a + y)));
        }
        x = (x + 1) & 0xFF;
        if (x > (unsigned)(y2 & 0xFF)) break;
    }
}

// The background colour for erasing: black, or white in INVERSE ($64F2, $689B).
static int background(Mach *m) {
    return TS->pal | ((m->invflg & 0x80) ? 0 : 3);
}

// $64C6: erase the cell at the cursor (the width of the last glyph looked up).
static void erase_cell(Mach *m) {
    TextState *t = TS;
    unsigned x1 = t->cx;
    unsigned x2 = (x1 + (unsigned)((t->g[0] - 1) & 0xFF) + t->met[2]) & 0xFFFF;
    uint8_t y1 = (uint8_t)(t->cy + t->met[4]);
    uint8_t y2 = (uint8_t)(y1 + t->met[1] - 1);
    fill(m, x1, y1, y2, x2, background(m));
}

// $688D: the window's right edge.
static unsigned right_edge(Mach *m) { return (unsigned)(TS->ww - 1) & 0xFFFF; }

// $6852: clear to the end of the line (CE$, & CEL).
static void clear_eol(Mach *m) {
    TextState *t = TS;
    fill(m, t->cx, t->cy, (uint8_t)(t->cy + t->met[1] - 1), right_edge(m), background(m));
}

// $686F: clear to the end of the window (CF$, & CEW).
static void clear_eow(Mach *m) {
    TextState *t = TS;
    clear_eol(m);
    fill(m, 0, (uint8_t)(t->cy + t->met[1]), (uint8_t)(t->wh - 1), right_edge(m), background(m));
}

// ---------------------------------------------------------------------------
// Cursor movement
// ---------------------------------------------------------------------------
// $6451: line feed. At the bottom the cursor wraps to the top line.
static void line_feed(Mach *m) {
    TextState *t = TS;
    unsigned a = (unsigned)t->met[1] + t->met[4];
    a = (a & 0xFF) + t->met[3] + (a >> 8);          // the carries run on through the ADCs
    unsigned y = (a & 0xFF) + t->cy + (a >> 8);
    if (y > 255) { t->cy = 0; return; }
    t->cy = (uint8_t)y;
    unsigned end = y + t->met[1] + 1;
    if (end > 255 || end > t->wh) t->cy = 0;
}

// $6500: carriage return and line feed.
static void newline(Mach *m) { TS->cx = 0; line_feed(m); }

// $6476: reverse line feed.
static void line_up(Mach *m) {
    TextState *t = TS;
    int y = t->cy - t->met[1];
    if (y < 0) { t->cy = 0; return; }
    y -= t->met[3];
    if (y < 0) { t->cy = 0; return; }
    y = (y - t->met[4]) & 0xFF;                     // no borrow check here
    if (y < t->wy) { t->cy = 0; return; }           // compares with the window top
    t->cy = (uint8_t)y;
}

// $6428: forward one cell; a new line if the next cell would not fit.
static void forward(Mach *m) {
    TextState *t = TS;
    t->cx = (uint16_t)(t->cx + t->met[0] + t->met[2]);
    if ((unsigned)t->cx + t->met[0] >= t->ww) newline(m);
}

// $63EA: back one cell; from the left edge to the end of the line above.
static void backward(Mach *m) {
    TextState *t = TS;
    t->cx = (uint16_t)(t->cx - t->met[0] - t->met[2]);
    if (t->cx & 0x8000) {
        line_up(m);
        t->cx = (uint16_t)(t->ww - t->met[0] - t->met[2]);
    }
}

// ---------------------------------------------------------------------------
// The line buffer and PRINT
// ---------------------------------------------------------------------------
static void set_offset(Mach *m, int a) {            // $652A
    TS->boff = (uint8_t)a;
    TS->bw = (uint16_t)(TS->bw + (a & 0xFF));
}

// $6365 (all = 1) / $6367 (all = 0): draw the buffered characters.
static void flush(Mach *m, int all) {
    TextState *t = TS;
    if (t->bn) {
        t->cx = (uint16_t)(t->cx + t->boff);
        for (int i = 0; i < t->bn; i++) {
            draw_glyph(m, t->low[0x28 + i], t->pal);
            t->cx = (uint16_t)(t->cx + t->g[0] + t->met[2]);
        }
    }
    if (all) { t->bw = 0; t->bn = 0; t->bcont = 0; }
    t->boff = 0;
    t->bspace = 0;
}

void text_flush(Mach *m) { flush(m, 1); }

// $6251: buffer one character (bit 7 = inverse), wrapping at a space.
static void buffer_char(Mach *m, int ch) {
    TextState *t = TS;
    for (int again = 0; again < 64; again++) {
        glyph_metrics(m, ch & 0x7F);
        int x = t->bn;
        if (!x && !t->boff && (t->indent & 0x80) == t->bcont) set_offset(m, t->indent & 0x7F);
        if ((ch & 0x7F) == 0x20) {
            t->bspace = (uint8_t)x;
            if (!x && (t->bcont & 0x80)) return;    // no leading space on a wrapped line
        }
        t->low[0x28 + x] = (uint8_t)ch;
        t->bn++;
        t->bw = (uint16_t)(t->bw + t->g[0] + t->met[2]);
    check:
        if ((unsigned)t->cx + t->bw < t->ww && x < 0x4F) return;
        if (!t->bspace) {                           // $62C1: no space: break the word here
            t->bn--;
            ch = t->low[0x28 + x];
            flush(m, 1);
            newline(m);
            continue;
        }
        // $62D2: draw up to the last space, carry the rest to the next line
        int count = t->bn;
        uint16_t before = t->cx;
        int sp = t->bspace;
        t->bn = (uint8_t)sp;
        flush(m, 0);
        uint16_t drawn = (uint16_t)(t->cx - before);
        t->bw = (uint16_t)(t->bw - drawn);
        newline(m);
        glyph_metrics(m, 0x20);
        t->bw = (uint16_t)(t->bw - ((t->g[0] + t->met[2]) & 0xFF));
        int from = sp + 1;
        t->bn = (uint8_t)(count - sp - 1);
        for (int i = 0; i < t->bn; i++) t->low[0x28 + i] = t->low[0x28 + from + i];
        t->bcont = 0x80;
        if (!(t->indent & 0x80)) return;
        set_offset(m, t->indent & 0x7F);
        x = from + t->bn;                           // X as the move loop left it
        goto check;                                 // re-check the fit ($629F)
    }
}

// $6154: control characters.
static void control(Mach *m, int ch) {
    TextState *t = TS;
    if (ch != 3) flush(m, 1);
    switch (ch) {
    case 1: flush(m, 1); break;                     // $6365
    case 2: home(m); clear_eow(m); break;           // $64B2
    case 3:                                         // $63C3: centre the buffered line
        if (!t->boff && t->bn) set_offset(m, ((t->ww - t->bw) & 0xFFFF) >> 1);
        break;
    case 5: clear_eol(m); break;                    // CE$
    case 6: clear_eow(m); break;                    // CF$
    case 7: mach_click(m); break;                   // $63E2: the bell
    case 8: backward(m); break;
    case 9: forward(m); break;
    case 10: line_feed(m); break;
    case 11: line_up(m); break;
    case 12:                                        // $6490: clear the whole page, home
        newline(m);
        for (unsigned a = 0; a < 0x2000; a++) wr(m, (unsigned)(m->hpag == 0x40 ? 0x40 : 0x20) << 8 | a, 0);
        home(m);
        break;
    case 13: newline(m); break;
    default: break;                                 // 0, 4 and 14-31 do nothing
    }
}

// $6128: one character from PRINT.
void text_putc(Mach *m, int ch) {
    TextState *t = TS;
    ch &= 0x7F;
    if (ch < 0x20) { control(m, ch); return; }
    if (!(m->invflg & 0x80)) ch |= 0x80;            // INVERSE
    if (t->noparse & 0x80) {                        // & NOPARSE: straight to the screen
        draw_glyph(m, ch, t->pal);
        t->cx = (uint16_t)(t->cx + t->g[0] + t->met[2]);
        return;
    }
    buffer_char(m, ch);
}

void amp_print(Mach *m, const char *s) {
    for (; *s; s++) text_putc(m, (unsigned char)*s);
}

// ---------------------------------------------------------------------------
// & commands
// ---------------------------------------------------------------------------
// $710A: every driver control first draws the buffered line.
static void control_entry(Mach *m) { flush(m, 1); }

void amp_hook(Mach *m) {                            // $60CD
    TextState *t = TS;
    if (!t->inited) text_init(m);
    m->gfx.text_flush = text_flush;
    m->gfx.text_cout = text_putc;
    t->font = 0x6BEB;
    m->gfx.set[0] = 0x6BEB;
    t->font_n = 0;
    t->noparse = 0;
    t->boff = 0;
    t->indent = 0;
    t->pal = 0;
    if (m->hpag != 0x40) m->hpag = 0x20;
    t->wtab[0][7] = m->hpag;
    select_window(m, 0);
    t->hooked = 1;
    m->text_mode = 0;
    m->page2 = m->hpag == 0x40;
    load_metrics(m);
}

void amp_unhook(Mach *m) {                          // $6BC0: a CR through the hook, then COUT1
    text_putc(m, 0x0D);
    TS->hooked = 0;
}

void amp_dfw(Mach *m, int n, int x, int y, int w, int h) {     // $69E7 / $68E7
    TextState *t = TS;
    control_entry(m);
    uint8_t *r = t->wtab[n & 7];
    r[0] = (uint8_t)x; r[1] = (uint8_t)(x >> 8);
    r[2] = (uint8_t)w; r[3] = (uint8_t)(w >> 8);
    r[4] = (uint8_t)y; r[5] = (uint8_t)h;
    r[6] = 0; r[7] = m->hpag;
}

void amp_wind(Mach *m, int n) {                     // $6B33 / $6939
    control_entry(m);
    select_window(m, n == ARG_NONE ? 0 : n);
}

void amp_co(Mach *m, int x, int y) {                // $6ABE / $696B
    TextState *t = TS;
    if (x == ARG_NONE && y == ARG_NONE) {           // CHR$(1), then home
        text_putc(m, 1);
        home(m);
        return;
    }
    control_entry(m);
    if (x != ARG_NONE) t->cx = (uint16_t)x;
    if (y != ARG_NONE) t->cy = (uint8_t)y;
}

void amp_gcp(Mach *m, int *x, int *y) {             // $6A38 / $6729
    control_entry(m);
    if (x) *x = TS->cx;
    if (y) *y = TS->cy;
}

void amp_in(Mach *m, int n) {                       // $6B0A
    text_putc(m, 1);
    if (n == ARG_NONE) { TS->indent = 0; return; }
    uint8_t lo = (uint8_t)n;
    if (lo & 0x80) lo = (uint8_t)((lo ^ 0x7F) + 1); // -20 -> $80 | 20
    TS->indent = lo;
}

// $6767: write one byte of a font's header, then reload the current metrics.
static void font_byte(Mach *m, int offset, int n, int font, int dflt) {
    TextState *t = TS;
    control_entry(m);
    if (n == ARG_NONE) n = dflt;
    int s = font == ARG_NONE ? t->font_n : font;
    wr(m, slot(m, s) + (unsigned)offset, (uint8_t)n);
    load_metrics(m);
}

void amp_hsp(Mach *m, int n, int font) { font_byte(m, 5, n, font, 2); }
void amp_vsp(Mach *m, int n, int font) { font_byte(m, 6, n, font, 1); }
void amp_tsp(Mach *m, int n, int font) { font_byte(m, 7, n, font, 0); }

void amp_aft(Mach *m, int n) {                      // $6A22 / $674A
    control_entry(m);
    if (n == ARG_NONE) n = 0;
    TS->font_n = (uint8_t)n;
    TS->font = slot(m, n);
    load_metrics(m);
}

void amp_parse(Mach *m) { control_entry(m); TS->noparse = 0; }
void amp_noparse(Mach *m) { control_entry(m); TS->noparse = 0x80; }
void amp_cel(Mach *m) { control_entry(m); clear_eol(m); }
void amp_cew(Mach *m) { control_entry(m); clear_eow(m); }

void amp_ht(Mach *m, int n) {                       // $650F: n cells from the left edge
    control_entry(m);
    TS->cx = 0;
    for (int i = n & 0xFF; i > 0; i--) forward(m);
}

void amp_vt(Mach *m, int n) {                       // $651D: line n from the top
    control_entry(m);
    TS->cy = 0xFF;
    for (int i = n & 0xFF; i >= 0; i--) line_feed(m);
}

void amp_chigh(Mach *m) { TS->pal = 0x00; sync_gfx(m); }
void amp_shigh(Mach *m) { TS->pal = 0x80; sync_gfx(m); }

static void escape(Mach *m) {
    TS->escaped = 1;
    if (TS->on_escape) TS->on_escape(m);
}

// $679A / $6797: "Press SPACE BAR to continue" on the bottom line of window 0
// (or of the current window when n >= 1). Space or Return continues and stays
// in the keyboard latch, so PEEK(-16384) = 13 afterwards means Return. Esc Esc
// goes to the menu.
void amp_space(Mach *m, int n) {
    TextState *t = TS;
    control_entry(m);
    int stay = n != ARG_NONE && (n & 0xFF) >= 1;
    mach_key_clear(m);
    uint8_t win = t->win;
    uint16_t cx = t->cx; uint8_t cy = t->cy;
    uint8_t indent = t->indent;
    t->wtab[0][7] = m->hpag;
    t->wtab[0][6] = 0;
    t->indent = 0;
    if (!stay) select_window(m, 0);
    t->cy = (uint8_t)(t->wh - rd(m, t->font + 4u));
    clear_eol(m);
    amp_print(m, "Press SPACE BAR to continue\x03\x01");
    int x = 1;
    for (;;) {
        int k = mach_key_wait(m);
        if (!k) break;                              // no host: nothing will come
        if (k == 0x8D || k == 0xA0) break;
        if (k != 0x9B) { x = 1; continue; }
        if (--x != 0) break;                        // the second Esc in a row
    }
    t->cx = 0;                                      // $64BD
    clear_eol(m);
    select_window(m, win);
    t->cx = cx; t->cy = cy;
    t->indent = indent;
    if (x < 0) escape(m);
}

// $61F9: wait up to `polls` keyboard polls (about 20 cycles each) for a key.
static int wait_key(Mach *m, long polls) {
    while (polls > 0) {
        if (mach_key_peek(m) & 0x80) { m->waiting = 0; return 1; }
        if (!m->idle) return 0;
        long step = polls < 800 ? polls : 800;
        m->waiting = 1;
        mach_advance(m, (unsigned long long)step * 20);
        polls -= step;
    }
    m->waiting = 0;
    return (mach_key_peek(m) & 0x80) != 0;
}

// $6187: the input hook. Blinks the font's cursor character at the cursor
// until a key arrives; handles DELETE / left arrow itself by erasing the last
// typed character (*len is the typed length, the caller's X). Returns the key.
static int input_hook(Mach *m, int *len) {
    TextState *t = TS;
    flush(m, 1);
    for (;;) {
        int got;
        for (;;) {
            int c = t->met[5];
            if (!(m->invflg & 0x80)) c |= 0x80;
            draw_glyph(m, c, t->pal);
            got = wait_key(m, 256L * 128);
            if (got) break;
            erase_cell(m);
            got = wait_key(m, 256L * 64);
            if (got) break;
            if (!m->idle && !(mach_key_peek(m) & 0x80)) return 0;   // no host: give up
        }
        int k = mach_key_peek(m);
        mach_key_clear(m);
        erase_cell(m);
        if (k != 0xFF && k != 0x88) return k;
        // $61C6: delete the last character
        if (*len - 1 < 0) return 0x88;
        (*len)--;
        glyph_metrics(m, m->ram[0x200 + *len] & 0x7F);
        uint8_t save = t->met[0];
        t->met[0] = t->g[0];
        backward(m);
        erase_cell(m);
        t->met[0] = save;
    }
}

// $7260 / $727B: is buffer character x allowed by the ranges? Lower case
// letters are tried again as upper case (and then stored upper case).
static int range_match(const uint8_t *r, int rlen, int c) {
    int y = 0;
    for (;;) {
        if (y >= rlen) return 0;
        int a = r[y++];
        if (a == '-') {
            if (y >= rlen) { a = '-'; goto literal; }
            int lo = r[y++];
            if (c == lo || c > lo) {                // CMP: c >= lo
                if (y >= rlen) return 1;            // no upper bound: a match
                int hi = r[y++];
                if (!(hi < c)) return 1;            // c <= hi
                continue;
            }
            if (y >= rlen) return 0;                // below the range: skip its end
            y++;
            continue;
        }
    literal:
        if (a == c) return 1;
    }
}

static int valid(uint8_t *buf, int x, const uint8_t *r, int rlen) {
    if (range_match(r, rlen, buf[x])) return 1;
    int c = buf[x];
    if (c >= 0x7B || c < 0x61) return 0;
    buf[x] = (uint8_t)(c - 0x20);
    return range_match(r, rlen, buf[x]);
}

int amp_inp_n(Mach *m, int len, const char *ranges, int rlen, int flag, char *out, int outlen) {
    TextState *t = TS;
    static const char dflt[3] = { '-', ' ', '~' };
    if (len == ARG_NONE) len = 16;
    if (!ranges) { ranges = dflt; rlen = 3; }
    else if (rlen < 0) rlen = (int)strlen(ranges);
    if (flag == ARG_NONE) flag = 1;
    flush(m, 1);                                    // the dispatcher
    const uint8_t *r = (const uint8_t *)ranges;
    int want_q = 0, want_esc = 0, want_ctla = 0;    // $72C0, $72C1, $72C2
    for (int y = 0; y < rlen; y++) {
        if (r[y] == '-') { y += 2; continue; }
        if (r[y] == 0x01) want_ctla = 1;
        if (r[y] == 0x1B) want_esc = 1;
        if (r[y] == '?') want_q = 1;
    }
    uint8_t *buf = m->ram + 0x200;
    mach_key_clear(m);
    int last_esc = 0;                               // $725E bit 7
    int maxlen = len & 0xFF, minlen = flag & 0xFF;
    int x = 0;
    if (maxlen) {                                   // clear the field
        flush(m, 1);
        for (int i = 0; i < maxlen; i++) forward(m);
        t->g[0] = t->met[0];
        for (int i = 0; i < maxlen; i++) { erase_cell(m); backward(m); }
        erase_cell(m);
    }
    for (;;) {
        int k = input_hook(m, &x) & 0x7F;
        if (!k) break;                              // no host
        if (k == 0x1B) {
            if (want_esc) { buf[0] = 0x1B; x = 1; goto done; }
            if (last_esc) { escape(m); x = 0; goto done; }
            last_esc = 1;
            continue;
        }
        last_esc = 0;
        buf[x] = (uint8_t)k;
        if (!maxlen) {                              // one key, no echo
            if (!valid(buf, x, r, rlen)) continue;
            x++;
            goto done;
        }
        if (x == 0) {
            if (want_q && (k == '?' || k == '/')) { buf[0] = '?'; x = 1; text_putc(m, 0x0D); goto done; }
            if (want_ctla && k == 0x01) { x = 1; text_putc(m, 0x0D); goto done; }
        }
        if (k == 0x0D) {
            if (x >= minlen) { text_putc(m, 0x0D); goto done; }
            continue;
        }
        if (x >= maxlen) continue;
        if (!valid(buf, x, r, rlen)) continue;
        text_putc(m, buf[x] | 0x80);
        x++;
    }
done:
    buf[x] = 0;
    int n = 0;
    for (; n < x && n < outlen - 1; n++) out[n] = (char)buf[n];
    if (outlen > 0) out[n] = 0;
    return n;
}

int amp_inp(Mach *m, int len, const char *ranges, int flag, char *out, int outlen) {
    return amp_inp_n(m, len, ranges, -1, flag, out, outlen);
}

// $86D1: USR(n). Bit 2 waits for a key, bit 0 clears the strobe; USR(0)
// resets the Esc count, USR(1) clears the strobe. Returns the $C000 value
// (bit 7 set if a key was waiting). Esc returns 0; Esc Esc goes to the menu.
int usr(Mach *m, int n) {
    TextState *t = TS;
    n &= 0xFF;
    if (n == 0) { t->usr_esc = 0; return 0; }
    if (n == 1) { mach_key_clear(m); return 0; }
    mach_advance(m, 4500);                          // the BASIC's call: a polling loop's pace
    for (;;) {
        int k = mach_key_peek(m);
        if ((n & 4) && k < 0x80) {
            if (!m->idle) return k;
            m->waiting = 1;
            mach_advance(m, 20 * 400);
            continue;
        }
        m->waiting = 0;
        if (k == 0x9B) {
            mach_key_clear(m);
            int prev = t->usr_esc & 0x80;
            t->usr_esc = (uint8_t)(0x80 | (t->usr_esc >> 1));
            if (prev) { escape(m); return 0; }
            if (n & 4) continue;
            return 0;
        }
        if (k >= 0x80) t->usr_esc = 0;
        if (n & 1) mach_key_clear(m);
        return k;
    }
}
