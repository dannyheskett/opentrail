// The extension's graphics commands and the BASIC's HPLOT. See gfx.h.
// Addresses in comments are the extension's ($6000-$92FF).
#include "mach.h"
#include <stdlib.h>
#include <string.h>

#define RAM(a) m->ram[(uint16_t)(a)]

void gfx_init(Mach *m) {
    GfxState *g = &m->gfx;
    g->set[0] = 0x6BEB;                             // & HOOK: the built-in font ($60CD)
    g->win = (GfxWindow){ 0, 280, 0, 192, m->hpag };
    g->attr = 0;
}

uint16_t gfx_set_addr(const Mach *m, int n) { return m->gfx.set[n & 15]; }

static void gfx_flush(Mach *m) { if (m->gfx.text_flush) m->gfx.text_flush(m); }

// ---------------------------------------------------------------------------
// & BOX ($697A, fill at $76A9): a rectangle in one of 8 colours, coordinates
// relative to the current window. Each row is merged with a 40-byte pattern
// that starts at the window's left byte ($70CE, built at $0400): the pattern
// bit of absolute pixel p is bit ((p + 1) & 1) of the colour, the high bit is the
// colour's. The partial bytes at either end also take the pattern's high bit.
// ---------------------------------------------------------------------------
// Row i of the current window: the row table at $6540/$6600 that selecting a
// window builds ($7024). Selecting writes only the window's own rows, so a
// box reaching below the window draws on rows left from an earlier, taller
// window (WIN:1005 clears the bottom of the screen from the 57-row status
// window this way). Without the text driver, the rows as $7024 builds them.
static uint16_t gfx_win_row(const Mach *m, const GfxWindow *w, int i) {
    if (m->text.inited && i < 192) return m->text.row[i];
    if (w->page == 0x20 || w->page == 0x40)                         // $7030
        return (uint16_t)(w->page * 256 + hires_row((w->y + i) & 0xFF) + w->x / 7);
    return (uint16_t)(w->page * 256 + i * (w->w / 7));              // $7072: a buffer window
}

static const uint8_t GFX_LMASK[7] = { 0xFF, 0xFE, 0xFC, 0xF8, 0xF0, 0xE0, 0xC0 };   // $775B
static const uint8_t GFX_RMASK[7] = { 0x81, 0x83, 0x87, 0x8F, 0x9F, 0xBF, 0xFF };   // $7763

void amp_box(Mach *m, int x1, int y1, int x2, int y2, int colour) {
    GfxState *g = &m->gfx;
    if (x1 == ARG_NONE) {                                           // & BOX alone: COUT $82
        if (g->text_cout) g->text_cout(m, 0x82);
        return;
    }
    // defaults from the window ($688D): its far corner and the text attribute
    int c = g->attr | ((m->invflg & 0x80) ? 0 : 3);
    if (x2 == ARG_NONE) { x2 = g->win.w - 1; y2 = g->win.h - 1; }
    if (colour != ARG_NONE) c = (colour & 3) | (colour >= 4 ? 0x80 : 0);    // $69B9
    gfx_flush(m);                                                       // $710A -> $6365

    int off = g->win.x % 7;                                         // $0309
    int left = g->win.x / 7;
    int bits = c & 3, hi = c & 0x80;
    uint8_t pat[40];                                                // $70CE -> $0400
    for (int b = 0; b < 40; b++) {
        int v = 0;
        for (int k = 0; k < 7; k++) {
            int p = (left + b) * 7 + k;
            v |= ((bits >> ((p + 1) & 1)) & 1) << k;
        }
        pat[b] = (uint8_t)(v | hi);
        RAM(0x400 + b) = pat[b];
    }
    int ax = (x1 + off) & 0xFFFF, bx = (x2 + off) & 0xFFFF;
    int b1 = ax / 7, s1 = ax % 7, b2 = bx / 7, s2 = bx % 7;
    uint8_t lm = GFX_LMASK[s1], rm = GFX_RMASK[s2];
    for (int yy = y1 & 0xFF; ; yy = (yy + 1) & 0xFF) {              // $770D-$7747, y1..y2 inclusive
        uint16_t row = gfx_win_row(m, &g->win, yy);
        int y = b1 & 0xFF;
        #define P(i) RAM(0x400 + ((i) & 0xFF))
        #define S(i) RAM(row + ((i) & 0xFF))
        uint8_t a = (uint8_t)((S(y) ^ P(y)) & lm);
        if (y >= (b2 & 0xFF)) {
            a &= rm;
            S(y) = a ^ S(y);
        } else {
            S(y) = a ^ S(y);
            y++;
            while (y != (b2 & 0xFF)) { S(y) = P(y); y++; }
            S(y) = (uint8_t)(((P(y) ^ S(y)) & rm) ^ S(y));
        }
        #undef P
        #undef S
        if (yy >= (y2 & 0xFF)) break;
    }
}

// & DFT n,addr ($69CD, control 10 at $6733)
void amp_dft(Mach *m, int n, int addr) {
    gfx_flush(m);
    m->gfx.set[n & 15] = (uint16_t)addr;
}

// ---------------------------------------------------------------------------
// Image sets ($7B38): byte 1 is the offset of an index of 2-byte entries;
// entry n holds the image's offset in bits 0-13, bit 15 = raw row-order data.
// ---------------------------------------------------------------------------
static int gfx_lookup(const Mach *m, int n, uint16_t *data, int *w, int *h, int *raw) {
    uint16_t set = m->gfx.set[7];
    int idx = (m->ram[(uint16_t)(set + 1)] + 2 * n) & 0xFF;         // Y = set[1] + 2n, 8-bit
    uint8_t lo = m->ram[(uint16_t)(set + idx)], hi = m->ram[(uint16_t)(set + ((idx + 1) & 0xFF))];
    uint16_t p = (uint16_t)(set + (lo | (hi & 0x3F) << 8));
    *w = m->ram[p];
    *h = m->ram[(uint16_t)(p + 1)];
    *data = (uint16_t)(p + 2);
    *raw = hi >> 7;
    return 1;
}

int image_size(const Mach *m, int n, int *w, int *h) {
    uint16_t d; int raw;
    return gfx_lookup(m, n, &d, w, h, &raw);
}

// x -> byte column and shift, rounding down for negative x ($8475)
static void gfx_xsplit(int x, uint8_t *col, int *shift) {
    x = (int16_t)x;
    int q = x >= 0 ? x / 7 : -((-x + 6) / 7);
    *col = (uint8_t)q;
    *shift = x - q * 7;
}

static uint16_t gfx_row(const Mach *m, int y) {                    // $826A/$832A | $E6
    return (uint16_t)(m->hpag * 256 + hires_row(y));
}

// shift tables built by & CST: lo[v] = v << s (7 bits), hi[v] = v >> (7 - s)
static uint8_t gfx_t1(const Mach *m, int s, int v) { return m->ram[(uint16_t)(m->gfx.shift_base + (s - 1) * 256 + v)]; }
static uint8_t gfx_t2(const Mach *m, int s, int v) { return m->ram[(uint16_t)(m->gfx.shift_base + (s - 1) * 256 + 128 + v)]; }

// The RLE blitter ($8511). Data is column by column; to the screen at
// (col, y) with a shift, or (mem != 0) unpacked row-major into memory.
typedef struct {
    Mach *m;
    int shift, mem;
    uint8_t col, row, row0, rowend;
    int width;
    uint16_t dst, colstart;         // memory mode
    long count;
} GfxRle;

static void gfx_rle_put(GfxRle *r, uint8_t v) {                            // $85D9
    Mach *m = r->m;
    if (r->mem) {
        m->ram[r->dst] = v;
        r->dst = (uint16_t)(r->dst + r->width);
    } else if (r->row < 192) {
        uint16_t a = gfx_row(m, r->row);
        uint8_t y = r->col;
        if (r->shift == 0) {
            if (y < 40) m->ram[(uint16_t)(a + y)] = v;
        } else {                                                    // $7E5E
            int s = r->shift;
            uint8_t hb = v & 0x80, x = v & 0x7F;
            static const uint8_t M1[7] = { 0, 0xFE, 0xFC, 0xF8, 0xF0, 0xE0, 0xC0 };
            static const uint8_t M2[7] = { 0, 0x81, 0x83, 0x87, 0x8F, 0x9F, 0xBF };
            if (y < 40) {
                uint8_t *p = &m->ram[(uint16_t)(a + y)];
                *p = (uint8_t)((((gfx_t1(m, s, x) | hb) ^ *p) & M1[s]) ^ *p);
            }
            y++;
            if (y < 40) {
                uint8_t *p = &m->ram[(uint16_t)(a + y)];
                *p = (uint8_t)((((gfx_t2(m, s, x) | hb) ^ *p) & M2[s]) ^ *p);
            }
        }
    }
    r->count++;
    r->row++;                                                       // $8632
    if (r->row == r->rowend) {
        if (r->mem) { r->colstart++; r->dst = r->colstart; }
        r->row = r->row0;
        r->col++;
    }
}

static uint16_t gfx_rle_blit(Mach *m, uint16_t src, int w, int h, int shift, uint8_t col, uint8_t y,
                         int mem, uint16_t dst, long *count) {
    GfxRle r = { m, shift, mem, col, y, y, (uint8_t)(y + h), w, dst, dst, 0 };
    for (;;) {                                                      // $856D
        uint8_t c = m->ram[src++];
        if (c < 0x40) {
            for (int n = c; n >= 0; n--) gfx_rle_put(&r, m->ram[src++]);
        } else if (c < 0x80) {
            uint8_t v = m->ram[src++];
            for (int n = c & 0x3F; n >= 0; n--) gfx_rle_put(&r, v);
        } else if (c == 0xC0) {
            break;
        } else {                                                    // $80-$BF and $C1-$FF
            uint8_t a = m->ram[src++], b = m->ram[src++];
            for (int n = c & 0x3F; n >= 0; n--) { gfx_rle_put(&r, a); gfx_rle_put(&r, b); }
        }
    }
    if (count) *count = r.count;
    return src;
}

// The raw row-order blitter ($80D5 shifted, $8084 unshifted).
static long gfx_raw_blit(Mach *m, uint16_t src, int w, int h, int shift, uint8_t col, uint8_t y) {
    uint8_t end = (uint8_t)(y + h);
    long n = 0;
    static const uint8_t MASK[7] = { 0x00, 0x01, 0x03, 0x07, 0x0F, 0x1F, 0x3F };   // $81A5
    for (uint8_t row = y; row != end; row++, src = (uint16_t)(src + w)) {
        if (row >= 192) continue;
        uint16_t a = gfx_row(m, row);
        uint8_t yy = col;
        if (shift == 0) {                                           // $8084
            for (int x = 0; x < w; x++, yy++, n++)
                if (yy < 40) m->ram[(uint16_t)(a + yy)] = m->ram[(uint16_t)(src + x)];
            continue;
        }
        uint8_t carry = m->ram[(uint16_t)(a + yy)] & MASK[shift];   // $8132: keep the pixels left of x
        uint8_t last = 0;
        for (int x = 0; x < w; x++, yy++, n++) {
            uint8_t d = m->ram[(uint16_t)(src + x)];
            uint8_t v = d & 0x7F;
            if (yy < 40) m->ram[(uint16_t)(a + yy)] = (uint8_t)(gfx_t1(m, shift, v) | carry | (d & 0x80));
            carry = gfx_t2(m, shift, v);
            last = d & 0x80;
        }
        if (yy < 40) {                                              // $8163: the spill byte
            uint8_t *p = &m->ram[(uint16_t)(a + yy)];
            uint8_t mk = MASK[shift] | 0x80;
            *p = (uint8_t)((((carry | last) ^ *p) & mk) ^ *p);
        }
    }
    return n;
}

// & IMAGE n,x,y ($891E): draw image n of the set in slot 7 at pixel x, row y.
void amp_image(Mach *m, int n, int x, int y) {
    uint16_t data; int w, h, raw, shift;
    uint8_t col;
    gfx_lookup(m, n & 0xFF, &data, &w, &h, &raw);
    gfx_xsplit(x, &col, &shift);
    long cnt;
    if (raw) gfx_raw_blit(m, data, w, h, shift, col, (uint8_t)y);
    else gfx_rle_blit(m, data, w, h, shift, col, (uint8_t)y, 0, 0, &cnt);
}

// & UIM n,addr[,var] ($8FB5, $7E93): unpack image n into memory at addr as a
// one-image raw set (header 01 00 06 80 00 00, then width, height, rows).
void amp_uim(Mach *m, int n, int addr, int *len) {
    static const uint8_t HDR[6] = { 0x01, 0x00, 0x06, 0x80, 0x00, 0x00 };   // $7F21
    uint16_t a = (uint16_t)addr;
    for (int i = 0; i < 6; i++) RAM(a + i) = HDR[i];
    uint16_t data; int w, h, raw;
    gfx_lookup(m, n & 0xFF, &data, &w, &h, &raw);
    RAM(a + 6) = (uint8_t)w;
    RAM(a + 7) = (uint8_t)h;
    long cnt;
    gfx_rle_blit(m, data, w, h, 0, 0, 0, 1, (uint16_t)(a + 8), &cnt);
    if (len) *len = (int)((w * h + 2 + 6) & 0xFFFF);
}

// & CST addr ($8787, $7A96): build the six shift tables at addr.
void amp_cst(Mach *m, int addr) {
    uint16_t base = (uint16_t)addr;
    m->gfx.shift_base = base;
    for (int s = 1; s <= 6; s++)
        for (int v = 0; v < 128; v++) {
            RAM(base + (s - 1) * 256 + v) = (uint8_t)((v << s) & 0x7F);
            RAM(base + (s - 1) * 256 + 128 + v) = (uint8_t)(v >> (7 - s));
        }
}

// & RFL "file" AT addr: the file's body (after the DOS address/length header).
int amp_rfl(Mach *m, const char *file, int addr) {
    static uint8_t buf[65536];
    int n = m->load ? m->load(m->load_ctx, file, buf, sizeof buf) : -1;
    if (n < 0) return 6;                                            // FILE NOT FOUND
    for (int i = 0; i < n; i++) RAM(addr + i) = buf[i];
    return 0;
}

// & DUN "file" [AT addr,pages] ($87D4): unpack a .PCK picture onto the page.
// The file streams through a buffer (default one page of DOS's own); the
// buffer's final contents are not modelled.
int amp_dun(Mach *m, const char *file, int addr, int pages) {
    (void)addr; (void)pages;
    static uint8_t buf[16384];
    int n = m->load ? m->load(m->load_ctx, file, buf, sizeof buf) : -1;
    if (n < 0) return 6;
    int p = 0;
    uint8_t row = 0, col = 0;
    #define OUT(v) do { RAM(m->hpag * 256 + hires_row(row) + col) = (v); \
                        if (++row == 192) { row = 0; col++; } } while (0)
    while (p < n) {                                                 // $8829
        uint8_t c = buf[p++];
        if ((c & 0xC0) == 0) {
            for (int k = c; k >= 0 && p < n; k--) OUT(buf[p++]);
        } else if (c >= 0xC0) {                                     // $8879: any $C0-$FF ends
            break;
        } else if (c < 0x80) {
            uint8_t v = p < n ? buf[p++] : 0;
            for (int k = c & 0x3F; k >= 0; k--) OUT(v);
        } else {
            uint8_t a = p < n ? buf[p++] : 0, b = p < n ? buf[p++] : 0;
            for (int k = c & 0x3F; k >= 0; k--) { OUT(a); OUT(b); }
        }
    }
    #undef OUT
    return 0;
}

// & TAKE x1,y1,x2,y2,addr ($8E22, $8678): save a byte-aligned rectangle of the
// page: width, height, then the rows.
void amp_take(Mach *m, int x1, int y1, int x2, int y2, int addr) {
    uint8_t c1 = (uint8_t)((x1 & 0xFFFF) / 7), c2 = (uint8_t)((x2 & 0xFFFF) / 7);
    uint8_t r = (uint8_t)y1, h = (uint8_t)(y2 - y1 + 1);
    uint16_t a = (uint16_t)addr;
    RAM(a) = (uint8_t)(c2 - c1 + 1);
    RAM(a + 1) = h;
    uint16_t d = (uint16_t)(a + 2);
    do {
        uint16_t row = (uint16_t)(m->hpag * 256 + hires_row(r));   // $7954
        uint8_t y = c1;
        do { RAM(d++) = RAM(row + y); y++; } while (y <= c2 && y != 0);
        r++;
    } while (--h);
}

// & PUT x,y,addr ($8BCC, $8200): restore a saved rectangle at byte column
// floor(x/7), row y; rows off the page and columns past 39 are skipped.
void amp_put(Mach *m, int x, int y, int addr) {
    uint8_t col; int shift;
    gfx_xsplit(x, &col, &shift);
    uint16_t a = (uint16_t)addr;
    int w = RAM(a), h = RAM(a + 1);
    uint16_t src = (uint16_t)(a + 2);
    int yy = y & 0xFFFF;
    do {
        int hi = yy >> 8, lo = yy & 0xFF;
        if (!(hi & 0x80) && hi != 1 && lo < 192) {
            uint16_t row = gfx_row(m, lo);
            uint8_t c = col;
            for (int i = 0; i < w; i++, c++)
                if (c < 40) RAM(row + c) = RAM(src + i);
        }
        src = (uint16_t)(src + w);
        yy = (yy + 1) & 0xFFFF;
        h = (h - 1) & 0xFF;
    } while (h);
}

// & DPC x,y,var ($8988, $84DD): colour 0-3 of the even-aligned pixel pair.
void amp_dpc(Mach *m, int x, int y, int *colour) {
    static const uint8_t SWAP[4] = { 0, 2, 1, 3 };                  // $850D
    int xe = x & 0xFFFE;
    uint16_t row = gfx_row(m, y & 0xFF);
    int b = xe / 7, s = xe % 7;
    uint8_t v = RAM(row + b);
    int bits;
    if (s == 6) bits = ((v >> 6) & 1) | ((RAM(row + b + 1) & 1) << 1);
    else bits = (v >> s) & 3;
    if (colour) *colour = SWAP[bits];
}

// ---------------------------------------------------------------------------
// HGR2, HCOLOR= and HPLOT: the BASIC's own hi-res drawing. A dot at x sets
// bit x % 7 of byte x / 7 and the byte's colour bit (7) to the colour's. A
// colour is a byte pattern as on an even byte; on an odd byte, a pattern
// whose dots alternate is shifted by one dot.
// ---------------------------------------------------------------------------
static void hdot(Mach *m, int x, int y) {
    int col = x / 7;
    uint8_t c = m->gfx.hcolor;
    if ((col & 1) && ((c >> 6 ^ c >> 5) & 1)) c ^= 0x7F;
    uint8_t mask = (uint8_t)(0x80 | 1 << (x % 7));
    uint8_t *p = &m->ram[(uint16_t)(m->hpag * 256 + hires_row(y) + col)];
    *p = (uint8_t)((*p & ~mask) | (c & mask));
}

// HGR2: page 2 cleared to black and shown
void hgr2(Mach *m) {
    m->hpag = 0x40;
    m->page2 = 1;
    m->text_mode = 0;
    memset(&m->ram[0x4000], 0, 0x2000);
}

// HCOLOR=
void hcolor(Mach *m, int c) {
    static const uint8_t PATTERN[8] = { 0x00, 0x2A, 0x55, 0x7F, 0x80, 0xAA, 0xD5, 0xFF };
    m->gfx.hcolor = PATTERN[c & 7];
}

// HPLOT x,y. Out-of-range points are ignored (the original stopped with
// ?ILLEGAL QUANTITY, which the game never provokes).
void hplot(Mach *m, int x, int y) {
    if (x < 0 || x >= 280 || y < 0 || y >= 192) return;
    m->gfx.hx = x; m->gfx.hy = y;
    hdot(m, x, y);
    mach_advance(m, 7700);                                          // calibrated
}

// HPLOT TO x,y, from the last point: a line of dx + dy + 1 dots, each a step
// across or a step up or down from the one before, never both.
void hplot_to(Mach *m, int x, int y) {
    if (x < 0 || x >= 280 || y < 0 || y >= 192) return;
    int px = m->gfx.hx, py = m->gfx.hy;
    int dx = abs(x - px), dy = abs(y - py), sx = x < px ? -1 : 1, sy = y < py ? -1 : 1;
    int err = dx - dy;
    for (int n = dx + dy; ; n--) {
        hdot(m, px, py);
        if (!n) break;
        if (err >= 0) { px += sx; err -= dy; }
        else { py += sy; err += dx; }
    }
    m->gfx.hx = x; m->gfx.hy = y;
    // calibrated: 6550 for "TO x,y", then 87 per step
    mach_advance(m, 6550 + 87ULL * (unsigned)(dx + dy + 1));
}
