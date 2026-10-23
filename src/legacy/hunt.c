// The hunting game, rebuilt from the machine code in language-card RAM bank 2
// (spec §21). The port is literal: the original keeps its state in its own
// variables at $EDE9-$EEDF (and in a few bytes inside its code) and passes
// boxes and pointers through zero page, so this file does the same with two
// byte arrays, Z() for the zero page it uses and V() for $E000-$FFFF. The
// 8-bit arithmetic, and so the original's quirks, come out unchanged.
// Comments cite the original addresses.
//
// Coordinates are 16-bit: x 0..279 and y 0..191 on the field, negative or past
// the edge while an animal enters or leaves. Every object is a box
// (x1, y1, x2, y2), inclusive.
#include "hunt.h"
#include <string.h>

static struct {
    uint8_t zp[256];                // the zero-page locations the hunt uses
    uint8_t lc[0x2000];             // $E000-$FFFF: tables and variables
    uint8_t set[3][4096];           // the three sprite sets, mirrored in place
    int ready;
    int cur;                        // $031E/$031F: the set E537 selected
    Mach *m;
    unsigned long long cyc;         // cycles spent since the last pacing
    // for hunt_view only (the original keeps none of these): the scenery's
    // images, the shot animals' types and facing
    uint8_t scen_id[8], dead_type[6], dead_flip[6];
    void (*hook)(void *);
    void *hook_ctx;
    int running;                    // inside hunt_play, the field set up
} H;

#define Z(a) H.zp[(a) & 0xFF]
#define V(a) H.lc[(a) - 0xE000]

enum { SET_ANIMALS, SET_SCENERY, SET_HUNTER };      // $D000, $D430, $F780
static const char *const SET_FILE[3] = { "HUNT_ANIMALS.IMA", "HUNT_SCENERY.IMA", "HUNT_HUNTER.IMA" };

// ---------------------------------------------------------------------------
// Tables from the language card
// ---------------------------------------------------------------------------
// $E006-$E097: per direction (0 = east, counter-clockwise) the step dx/dy
// (lo, hi), the muzzle offset x (lo, hi) and y, the hunter's pose (bit 7 =
// mirrored), the walk frames, the scenery per zone, and the six animals.
static const uint8_t TAB_E006[0x92] = {
    0x02, 0x02, 0x00, 0xFE, 0xFE, 0xFE, 0x00, 0x02,     // $E006 dx lo
    0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0x00, 0x00,     // $E00E dx hi
    0x00, 0xFF, 0xFF, 0xFF, 0x00, 0x01, 0x01, 0x01,     // $E016 dy lo
    0x00, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00,     // $E01E dy hi
    0x15, 0x13, 0x0D, 0x00, 0xFE, 0xFE, 0x07, 0x15,     // $E026 muzzle x lo
    0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00,     // $E02E muzzle x hi
    0x0A, 0x00, 0x00, 0x00, 0x0A, 0x0F, 0x13, 0x0F,     // $E036 muzzle y
    0x87, 0x84, 0x01, 0x04, 0x07, 0x0A, 0x0D, 0x8A,     // $E03E hunter pose
    0x00, 0x01, 0x00, 0x02,                             // $E046 walk frames
    0x00, 0x01, 0x02, 0x06, 0x07, 0x08,                 // $E04A zone 0
    0x06, 0x07, 0x08, 0x06, 0x07, 0x08,                 //        zone 1
    0x03, 0x04, 0x05, 0x0C, 0x0D, 0x0E,                 //        zone 2
    0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E,                 //        zone 3
    0x03, 0x04, 0x05, 0x03, 0x04, 0x05,                 //        zone 4
    // $E068: pace base, pace range, pounds base (16), pounds range (16),
    // width in pixels, height (the last two filled in at $E68A)
    0x03, 0x04, 0x64, 0x00, 0x28, 0x00, 0x00, 0x00,     // 0 deer (buck)
    0x03, 0x08, 0xC8, 0x00, 0xC8, 0x00, 0x00, 0x00,     // 1 bear
    0x0B, 0x04, 0xA4, 0x06, 0x2C, 0x01, 0x00, 0x00,     // 2 buffalo
    0x03, 0x04, 0x78, 0x00, 0x1E, 0x00, 0x00, 0x00,     // 3 deer
    0x03, 0x04, 0x02, 0x00, 0x08, 0x00, 0x00, 0x00,     // 4 rabbit
    0x03, 0x04, 0x03, 0x00, 0x04, 0x00, 0x00, 0x00,     // 5 squirrel
};
static const uint8_t BULLET_L[7] = { 0x03, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x40 };   // $E4AD
static const uint8_t BULLET_R[7] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 };   // $E4B4
static const uint8_t PIXELS[10] = { 0, 7, 14, 21, 28, 35, 42, 49, 56, 63 };         // $E52D
static const uint8_t SEVENS[6] = { 0x07, 0x0E, 0x1C, 0x38, 0x70, 0xE0 };           // $EF46
static const uint8_t KEEP_L[7] = { 0x00, 0x81, 0x83, 0x87, 0x8F, 0x9F, 0xBF };      // $EFEA
static const uint8_t KEEP_R[7] = { 0xFE, 0xFC, 0xF8, 0xF0, 0xE0, 0xC0, 0x00 };      // $EFF1
static const uint8_t EXPERT[8] = { ';', 'P', 'O', 'I', 'K', ',', '.', '/' };        // $ED1E
static const uint8_t LOWBITS[7] = { 0x00, 0x01, 0x03, 0x07, 0x0F, 0x1F, 0x3F };     // $F381
static const uint8_t COLOUR[4] = { 0, 2, 1, 3 };                                    // $F63B

// ---------------------------------------------------------------------------
// Machine plumbing
// ---------------------------------------------------------------------------
static void cost(int n) { H.cyc += (unsigned long long)n; }

static void pace(void) {                    // spend the cycles counted so far
    if (H.hook) H.hook(H.hook_ctx);
    mach_advance(H.m, H.cyc);
    H.cyc = 0;
}

// $F388/$F448: the address of hi-res row y on page $E6.
static uint16_t row_at(uint8_t y) { return (uint16_t)((H.m->hpag << 8) + hires_row(y)); }

static uint8_t *ram(uint16_t a) { return &H.m->ram[a]; }

static void inc_4e(void) {                  // INC $4E : BNE : INC $4F
    if (++H.m->ram[0x4E] == 0) H.m->ram[0x4F]++;
}

static int load_sets(Mach *m) {
    for (int s = 0; s < 3; s++) {
        int n = m->load ? m->load(m->load_ctx, SET_FILE[s], H.set[s], sizeof H.set[s]) : -1;
        if (n <= 0) return 0;
    }
    memcpy(&V(0xE006), TAB_E006, sizeof TAB_E006);
    V(0xEE09) = 0x01;                                       // as the boot leaves them
    V(0xEE32) = 3; V(0xEE33) = 4; V(0xEE34) = 5;            // types always allowed
    H.ready = 1;
    return 1;
}

// ---------------------------------------------------------------------------
// Arithmetic helpers
// ---------------------------------------------------------------------------
// $EF1A: divide the 9-bit value (hi, lo) by 7 -> quotient, remainder.
static uint8_t div7(uint8_t lo, uint8_t hi, uint8_t *rem) {
    uint8_t a = lo, base = 0;
    if (hi) { a = (uint8_t)(a - 0xE0); base = 0x20; }
    uint8_t q = 0;
    for (int x = 5; x >= 0; x--) {
        int c = a >= SEVENS[x];
        if (c) a = (uint8_t)(a - SEVENS[x]);
        q = (uint8_t)((q << 1) | c);
    }
    *rem = a;
    cost(144);                                              // measured on the original
    return (uint8_t)(base + q);
}

// $F5DC: signed x -> byte column and pixel shift (floor division by 7).
static void x_to_col(uint8_t lo, uint8_t hi, uint8_t *col, uint8_t *shift) {
    uint8_t r;
    if (!(hi & 0x80)) { *col = div7(lo, hi, &r); *shift = r; return; }
    uint8_t y = (uint8_t)~lo, x = (uint8_t)~hi;
    if (++y == 0) x++;
    uint8_t q = div7(y, x, &r);
    y = (uint8_t)~q;
    uint8_t a = r ^ 7;
    if (a >= 7) { a = 0; y++; }
    *shift = a; *col = y;
}

// $EEE1: 8 x 8 -> 16-bit product (lo, hi).
static uint16_t mul8(uint8_t a, uint8_t b) { cost(150); return (uint16_t)(a * b); }

// ---------------------------------------------------------------------------
// The random generator ($F508-$F5DA): a 48-bit linear congruential
// generator, S = S * (2^27 + 3) + 1, big-endian at $F5CF-$F5D4.
// ---------------------------------------------------------------------------
static void rol48(uint8_t *s) {
    int c = 0;
    for (int x = 5; x >= 0; x--) { int n = s[x] >> 7; s[x] = (uint8_t)(s[x] << 1 | c); c = n; }
}

static void rng_step(void) {                                // $F56E
    uint8_t *s = &V(0xF5CF), *t = &V(0xF5D5);
    if (!V(0xF5DB)) {
        for (int x = 5; x >= 0; x--) s[x] = H.m->ram[0xC9 + x];
        s[0] |= 1;
        V(0xF5DB) = 1;
    }
    memcpy(t, s, 6);
    for (int y = 0x19; y >= 0; y--) rol48(s);               // x 2^26
    int c = 0;                                              // $F5AA: + S
    for (int x = 5; x >= 0; x--) { int v = s[x] + t[x] + c; s[x] = (uint8_t)v; c = v >> 8; }
    rol48(s);
    c = 0;
    for (int x = 5; x >= 0; x--) { int v = s[x] + t[x] + c; s[x] = (uint8_t)v; c = v >> 8; }
    for (int x = 5; x >= 0; x--) if (++s[x]) break;         // $F5BA: + 1
    cost(2842);
}

// $F50A: a random number 0..n-1 (0 when n is 0), from the generator's top
// two bytes taken low byte first.
static uint16_t rnd16(uint16_t n) {
    if (!n) { cost(30); return 0; }
    rng_step();
    uint16_t v = (uint16_t)(V(0xF5CF) | V(0xF5D0) << 8);
    cost(789);                                              // the 16-bit division
    return (uint16_t)(v % n);
}
static uint8_t rnd8(uint8_t n) { return (uint8_t)rnd16(n); }   // $F508

// ---------------------------------------------------------------------------
// Sprite sets and drawing
// ---------------------------------------------------------------------------
// $E537: pick the set for image id V($EEC5) and make it the set's own number.
static void select_set(void) {
    uint8_t id = V(0xEEC5);
    H.cur = SET_HUNTER;
    if (id < 0x10) return;
    if (id < 0x22) { V(0xEEC5) = (uint8_t)(id - 0x0F); H.cur = SET_ANIMALS; return; }
    V(0xEEC5) = (uint8_t)(id - 0x21);
    H.cur = SET_SCENERY;
}

// $F1FC: image n of the current set -> width (bytes), height, data.
static uint8_t *image(uint8_t n, uint8_t *w, uint8_t *h) {
    uint8_t *s = H.set[H.cur];
    uint8_t y = (uint8_t)(s[1] + 2 * n + 1);
    uint8_t hi = s[y], lo = s[(uint8_t)(y - 1)];
    uint8_t *rec = s + (lo + ((hi & 0x3F) << 8));
    *w = rec[0]; *h = rec[1];
    cost(136);
    return rec + 2;
}

// $E51C: the pixel width and height of image id a (V($EEC5) left as its
// number within its set).
static uint8_t hunt_size(uint8_t a, uint8_t *h) {
    V(0xEEC5) = a;
    select_set();
    uint8_t w;
    image(V(0xEEC5), &w, h);
    return PIXELS[w];
}

// $F07D: mirror an image left to right in place, reversing each byte's seven
// pixels and keeping bit 7 with its byte.
static uint8_t rev7(uint8_t a) {                            // $F139
    uint8_t r = 0;
    for (int i = 0; i < 7; i++) if (a & 1 << i) r |= (uint8_t)(1 << (6 - i));
    return (uint8_t)(r | (a & 0x80));
}

static void mirror(uint8_t n) {                             // $F052 with A = 1
    uint8_t w, h;
    uint8_t *p = image(n, &w, &h);
    uint8_t half = (uint8_t)((w - 1) >> 1);
    for (int row = 0; row < h; row++, p += w) {
        uint8_t y = (uint8_t)(w - 1);
        for (uint8_t x = 0; ; x++, y--) {
            uint8_t a = p[x], b = p[y];
            p[x] = rev7(b);
            p[y] = rev7(a);
            if (x >= half) break;
        }
        cost(45 + (half + 1) * 210);
    }
}

// $E4FB: flip image id y's stored orientation (its flag at $EE9D + id).
static void flip(uint8_t id) {
    V(0xEEC5) = id;
    V(0xE51B) = id;
    V(0xEE9D + id) ^= 0x80;
    select_set();
    mirror(V(0xEEC5));
    V(0xEEC5) = V(0xE51B);
}

// $F2B1: draw w x h bytes of data at byte column Z($EC), pixel shift Z($ED),
// row Z($EE), through the shift tables at $3A00. Opaque; bit 7 of each source
// byte picks the colour group of the screen byte it lands in.
static void blit(const uint8_t *data, uint8_t w, uint8_t h) {
    uint8_t end = (uint8_t)(Z(0xEE) + h), s = Z(0xED);
    if (!s) {                                               // $F260: byte-aligned
        do {
            uint8_t x = Z(0xEE);
            if (x < 0xC0) {
                uint16_t row = row_at(x);
                uint8_t y = Z(0xEC);
                for (uint8_t i = 0; i != w; i++, y++) if (y < 0x28) *ram((uint16_t)(row + y)) = data[i];
            }
            Z(0xEE)++;
            data += w;
            cost(55 + w * 23);
        } while (Z(0xEE) != end);
        return;
    }
    uint16_t lo = (uint16_t)(((s - 1 + 0x3A) & 0xFF) << 8);   // $F63F: table for this shift
    uint16_t hi = (uint16_t)(lo + 0x80);                      // $F64C
    uint8_t mask = LOWBITS[s], edge = (uint8_t)(mask | 0x80);
    do {
        uint8_t x = Z(0xEE);
        if (x < 0xC0) {
            uint16_t row = row_at(x);
            uint8_t y = Z(0xEC);
            uint8_t carry = *ram((uint16_t)(row + y)) & mask;    // $5E
            int top = 0;
            for (uint8_t i = 0; i != w; i++, y++) {
                uint8_t v = data[i];
                top = v & 0x80;
                v &= 0x7F;
                if (y < 0x28) *ram((uint16_t)(row + y)) = (uint8_t)(*ram((uint16_t)(lo + v)) | carry | top);
                carry = *ram((uint16_t)(hi + v));
            }
            if (y < 0x28) {                                 // $F341: the last partial byte
                uint8_t *p = ram((uint16_t)(row + y));
                uint8_t a = (uint8_t)(carry | top);
                *p = (uint8_t)((a & edge) | (*p & ~edge));
            }
            cost(100 + w * 50);
        } else cost(35);
        Z(0xEE)++;
        data += w;
    } while (Z(0xEE) != end);
}

// $EF4C: draw the image described at $EEC5: number, x (16), y (16).
static void draw_block(void) {
    for (int y = 1; y <= 4; y++) Z(0xEB + y) = V(0xEEC5 + y);
    uint8_t w, h;
    cost(250);
    const uint8_t *data = image(V(0xEEC5), &w, &h);
    uint8_t col, shift;
    x_to_col(Z(0xEC), Z(0xED), &col, &shift);
    Z(0xEC) = col; Z(0xED) = shift;
    blit(data, w, h);
}

// $E45F: the bullet, two pixels by two rows, exclusive-ored.
static void bullet_xor(uint8_t xlo, uint8_t xhi, uint8_t row) {
    uint8_t rem;
    uint8_t col = div7(xlo, xhi, &rem);
    uint8_t y = col;
    for (int n = 0; n < 2; n++, row++) {
        if (row < 0xC0) {
            Z(0x93) = (uint8_t)row_at(row); Z(0x94) = (uint8_t)(row_at(row) >> 8);
            y = col;
            if (y < 0x28) *ram((uint16_t)(Z(0x93) | Z(0x94) << 8) + y) ^= BULLET_L[rem];
        }
        y++;                                                // $E494
        if (y < 0x28) *ram((uint16_t)((Z(0x93) | Z(0x94) << 8) + y)) ^= BULLET_R[rem];
    }
    cost(150);
}

// $E295: while a shot is in flight, toggle it (off before drawing, on after).
static void bullet_toggle(void) {
    if (V(0xEDF7) & 0x80) bullet_xor(V(0xEE38), V(0xEE39), V(0xEE3A));
}

// $EDA6: draw the image block at $EEC5 with the shot kept on top.
static void draw(void) {
    inc_4e();
    select_set();
    bullet_toggle();
    draw_block();
    bullet_toggle();
}

// $EFF8: clip the box at $86-$8D to the field; 0 when nothing is left.
static int s16(uint8_t lo, uint8_t hi) { return (int16_t)(uint16_t)(lo | hi << 8); }

static int clip(void) {
    if ((Z(0x8B) & 0x80) || (Z(0x8D) & 0x80)) return 0;
    if (0xBF - s16(Z(0x88), Z(0x89)) < 0) return 0;
    if (0x117 - s16(Z(0x86), Z(0x87)) < 0) return 0;
    if (Z(0x87) & 0x80) { Z(0x86) = 0; Z(0x87) = 0; }
    if (Z(0x89) & 0x80) Z(0x88) = 0;
    if ((Z(0x8A) | Z(0x8B) << 8) >= 0x118) { Z(0x8A) = 0x17; Z(0x8B) = 0x01; }
    if ((Z(0x8C) | Z(0x8D) << 8) >= 0xC0) Z(0x8C) = 0xBF;
    return 1;
}

// $EF82: clear the box at $86-$8D to black (pixels outside it in the edge
// bytes are kept).
static void erase(void) {
    cost(120);
    if (!clip()) return;
    uint8_t r;
    Z(0x86) = div7(Z(0x86), Z(0x87), &r); Z(0x87) = KEEP_L[r];
    Z(0x8A) = div7(Z(0x8A), Z(0x8B), &r); Z(0x8B) = KEEP_R[r];
    Z(0x89) = (uint8_t)(Z(0x8C) - Z(0x88) + 1);
    do {
        if (Z(0x88) < 0xC0) {           // rows past 191 index past the original's tables
            uint16_t row = row_at(Z(0x88));
            uint8_t y = Z(0x86);
            uint8_t *p = ram((uint16_t)(row + y));
            uint8_t a = *p & Z(0x87);
            if (y == Z(0x8A)) *p = (uint8_t)((*p & Z(0x8B)) | a);
            else {
                *p = a;
                for (y++; y < Z(0x8A); y++) *ram((uint16_t)(row + y)) = 0;
                p = ram((uint16_t)(row + y));
                *p &= Z(0x8B);
            }
        }
        cost(61 + (uint8_t)(Z(0x8A) - Z(0x86)) * 14);
        Z(0x88)++;
    } while (--Z(0x89));
}

// $E26D: 1 (carry set) unless the box at $86-$8D lies inside the field.
static int outside(void) {
    if (Z(0x89) || Z(0x8D)) return 1;
    if (Z(0x88) >= 0xC0 || Z(0x8C) >= 0xC0) return 1;
    if ((Z(0x86) | Z(0x87) << 8) >= 0x118 && !(Z(0x87) & 0x80)) return 1;
    if (Z(0x87) & 0x80) return 1;
    return (Z(0x8A) | Z(0x8B) << 8) >= 0x118 || (Z(0x8B) & 0x80);
}

// $EDC8: 1 when the box at $93-$9A has left the field entirely.
static int gone(void) {
    if (Z(0x98) & 0x80) return 1;
    if (Z(0x9A) & 0x80) return 1;
    if (!(Z(0x96) & 0x80) && Z(0x95) >= 0xC0) return 1;
    if (Z(0x94) & 0x80) return 0;
    return (Z(0x93) | Z(0x94) << 8) >= 0x118;
}

// $E4BB: do the boxes at $86-$8D and $93-$9A overlap?
static int overlap(void) {
    cost(51);
    if (s16(Z(0x97), Z(0x98)) < s16(Z(0x86), Z(0x87))) return 0;
    if (s16(Z(0x99), Z(0x9A)) < s16(Z(0x88), Z(0x89))) return 0;
    if (s16(Z(0x8A), Z(0x8B)) < s16(Z(0x93), Z(0x94))) return 0;
    if (s16(Z(0x8C), Z(0x8D)) < s16(Z(0x95), Z(0x96))) return 0;
    return 1;
}

// $E740: animal x's box -> $93-$9A.
static void animal_box(uint8_t x) {
    Z(0x93) = V(0xEE0F + x); Z(0x94) = V(0xEE11 + x);
    Z(0x95) = V(0xEE13 + x); Z(0x96) = V(0xEE15 + x);
    Z(0x97) = V(0xEE17 + x); Z(0x98) = V(0xEE19 + x);
    Z(0x99) = V(0xEE1B + x); Z(0x9A) = V(0xEE1D + x);
}

// $E3AF: the box at $86-$8D against scenery, live animals and carcasses.
// Returns 1 on a hit with *x = $81 (scenery), the animal, or $82 (carcass);
// on a miss *x is whatever the loops left, as in the original.
static int hits_others(uint8_t *x) {
    V(0xE45E) = 0;
    Z(0x96) = 0; Z(0x9A) = 0;
    for (; V(0xE45E) < V(0xEDF9); V(0xE45E)++) {           // scenery
        *x = V(0xE45E);
        Z(0x93) = V(0xEE6E + *x); Z(0x94) = V(0xEE76 + *x); Z(0x95) = V(0xEE7E + *x);
        Z(0x97) = V(0xEE86 + *x); Z(0x98) = V(0xEE8E + *x); Z(0x99) = V(0xEE96 + *x);
        if (overlap()) { *x = 0x81; return 1; }
    }
    V(0xE45E) = 0;
    for (; V(0xE45E) < 2; V(0xE45E)++) {                    // live animals
        *x = V(0xE45E);
        if (V(0xEE0D + *x) & 0x80) continue;
        animal_box(*x);
        if (overlap()) { *x = V(0xE45E); return 1; }
    }
    V(0xE45E) = 0;
    if (!V(0xEDFA)) return 0;
    for (; V(0xE45E) < V(0xEDFA); V(0xE45E)++) {           // carcasses
        *x = V(0xE45E);
        Z(0x93) = V(0xEE3D + *x); Z(0x94) = V(0xEE43 + *x);
        Z(0x95) = V(0xEE49 + *x); Z(0x96) = V(0xEE4F + *x);
        Z(0x97) = V(0xEE55 + *x); Z(0x98) = V(0xEE5B + *x);
        Z(0x99) = V(0xEE61 + *x); Z(0x9A) = V(0xEE67 + *x);
        if (overlap()) { *x = 0x82; return 1; }
    }
    return 0;
}

// $E39D: the box at $86-$8D against the hunter, then everything else.
static int hits(uint8_t *x) {
    for (int i = 7; i >= 0; i--) Z(0x93 + i) = V(0xEDFD + i);
    *x = 0xFF;
    if (overlap()) { *x = 0x80; return 1; }
    return hits_others(x);
}

// $E382: as $E39D, with animal x itself left out.
static int hits_but(uint8_t x, uint8_t *who) {
    V(0xE39C) = x;
    V(0xEE0D + x) |= 0x80;
    int c = hits(who);
    V(0xEE0D + V(0xE39C)) &= 0x7F;
    return c;
}

// $ED26: a sprite moved from the old box ($EC-$EF, $FA-$FD) to the new one
// ($93-$9A) by (dx, dy) = ($3C, $3E): clear the strips it uncovered.
static void erase_trail(void) {
    Z(0xFE) = Z(0x93); Z(0xFF) = Z(0x94);
    bullet_toggle();
    Z(0x93) = Z(0xFE); Z(0x94) = Z(0xFF);
    for (V(0xEDA5) = 2; ; ) {
        for (int x = 3; x >= 0; x--) Z(0x86 + x) = Z(0xEC + x);
        for (int x = 3; x >= 0; x--) Z(0x8A + x) = Z(0xFA + x);
        uint8_t x = V(0xEDA5);
        uint8_t d = Z(0x3C + x);
        if (d) {
            if (!(d & 0x80)) { Z(0x8A + x) = Z(0x93 + x); Z(0x8B + x) = Z(0x94 + x); }
            else { Z(0x86 + x) = Z(0x97 + x); Z(0x87 + x) = Z(0x98 + x); }
            // $ED6C: nothing below the field; the bottom clamped to row 191
            if (0xBF - s16(Z(0x88), Z(0x89)) >= 0) {
                if (s16(Z(0x8C), Z(0x8D)) - 0xBF >= 0) Z(0x8C) = 0xBF;
                erase();
            }
        }
        V(0xEDA5) -= 2;
        if (V(0xEDA5) != 0) break;
    }
    bullet_toggle();
    Z(0x93) = Z(0xFE); Z(0x94) = Z(0xFF);
}

// ---------------------------------------------------------------------------
// The hunter
// ---------------------------------------------------------------------------
// $EB31: draw the hunter in his pose for the current direction and step.
static void draw_hunter(void) {
    uint8_t pose = V(0xE03E + V(0xEE05));
    uint8_t id = (uint8_t)((pose & 0x7F) + V(0xE046 + V(0xEE0C)));
    V(0xEEC5) = id;
    if ((pose ^ V(0xEE9D + id)) & 0x80) flip(id);
    for (int x = 3; x >= 0; x--) V(0xEEC6 + x) = V(0xEDFD + x);
    draw();
}

// $EC0E: stop walking.
static void stop_walking(void) {
    V(0xEE0C) = 0;
    V(0xEE08) = 0;
    V(0xEE09) = 8;
    V(0xEE0B) = 8;
    draw_hunter();
}

// $EB7C: every EE09 ticks, a step if he is walking.
static void walk(void) {
    V(0xEE0B) = V(0xEE09);
    if (!(V(0xEE08) & 0x80)) { stop_walking(); return; }
    V(0xEE0C) = (uint8_t)((V(0xEE0C) + 1) & 3);
    uint8_t d = V(0xEE05);
    for (int x = 4; ; x -= 4) {                             // $EB99: x1 and x2
        int v = V(0xEDFD + x) + V(0xE006 + d);
        Z(0x86 + x) = (uint8_t)v;
        Z(0x87 + x) = (uint8_t)(V(0xEDFE + x) + V(0xE00E + d) + (v >> 8));
        if (x == 0) break;
    }
    for (int x = 4; ; x -= 4) {                             // $EBB5: y1 and y2
        int v = V(0xEDFF + x) + V(0xE016 + d);
        Z(0x88 + x) = (uint8_t)v;
        Z(0x89 + x) = (uint8_t)(V(0xEE00 + x) + V(0xE01E + d) + (v >> 8));
        if (x == 0) break;
    }
    uint8_t who;
    if (outside() || hits_others(&who)) { stop_walking(); return; }
    for (int x = 3; x >= 0; x--) {                          // $EBD8
        Z(0xEC + x) = V(0xEDFD + x);
        Z(0x93 + x) = Z(0x86 + x);
        V(0xEDFD + x) = Z(0x86 + x);
    }
    for (int x = 3; x >= 0; x--) {
        Z(0xFA + x) = V(0xEE01 + x);
        Z(0x97 + x) = Z(0x8A + x);
        V(0xEE01 + x) = Z(0x8A + x);
    }
    Z(0x3C) = V(0xE006 + d);
    Z(0x3E) = V(0xE016 + d);
    erase_trail();
    draw_hunter();
}

// $E6F3: fire.
static void fire(void) {
    uint16_t b = (uint16_t)(V(0xEDF1) | V(0xEDF2) << 8);
    b--;
    V(0xEDF1) = (uint8_t)b; V(0xEDF2) = (uint8_t)(b >> 8);
    uint8_t d = V(0xEE05);
    V(0xEE3C) = d;
    V(0xEDF7) = (uint8_t)(0x80 | V(0xEDF7) >> 1);
    int v = V(0xEDFD) + V(0xE026 + d);
    V(0xEE38) = (uint8_t)v;
    V(0xEE39) = (uint8_t)(V(0xEDFE) + V(0xE02E + d) + (v >> 8));
    v = V(0xEDFF) + V(0xE036 + d);
    V(0xEE3A) = (uint8_t)v;
    V(0xEE3B) = (uint8_t)(V(0xEE00) + (v >> 8));
    for (int x = 0; x < 2; x++)                             // $E72F: startled (unused)
        if (!(V(0xEE0D + x) & 0x80)) V(0xEE2F + x) = (uint8_t)(0x80 | V(0xEE2F + x) >> 1);
    bullet_toggle();
}

// $EC21: the keyboard, every third tick. Returns -1 on Esc Esc.
static int keys(void) {
    for (;;) {
        cost(30);
        int k = mach_key_peek(H.m);
        if (!(k & 0x80)) return 0;
        mach_key_clear(H.m);
        uint8_t a = k & 0x7F;
        if (a >= 0x61 && a < 0x7B) a -= 0x20;
        if (a == 0x1B) {                                    // $EC35: Esc, twice running
            V(0xEDF8) = (uint8_t)(0x80 | V(0xEDF8) >> 1);
            if (V(0xEDF8) & 0x40) return -1;
            return 0;
        }
        V(0xEDF8) >>= 1;                                    // $EC68
        if (a == ' ') {
            if (V(0xEDF7) & 0x80) return 0;
            if (!(V(0xEDF1) | V(0xEDF2))) return 0;
            fire();
            return 0;
        }
        if (a == 0x0D) {                                    // Return: walk / stop
            V(0xEE08) ^= 0x80;
            if (V(0xEE08) & 0x80) return 0;
            V(0xEE0C) = 0;
            V(0xEE09) = 8;
            return 0;
        }
        if (a == 0x08 || a == 0x0B) { V(0xEE06) = 1; V(0xEE07) = 1; return 0; }      // left, up
        if (a == 0x15 || a == 0x0A) { V(0xEE06) = 1; V(0xEE07) = 0xFF; return 0; }   // right, down
        if (a == 0x10) {                                    // Ctrl-P: pause for a key
            for (;;) {                                      // $ECC0
                if (mach_key_peek(H.m) & 0x80) break;
                cost(20);
                pace();
                if (!H.m->idle) return 0;                   // nothing will ever come
            }
            continue;
        }
        int x = 7;
        for (; x >= 0; x--) if (EXPERT[x] == a) break;
        if (x < 0) return 0;
        int diff = (uint8_t)(x - V(0xEE05));                // $ECD3
        if (!diff) {
            V(0xEE06) = 0; V(0xEE07) = 0;
            if (V(0xEE08) & 0x80) {                         // faster, down to 4 ticks a step
                V(0xEE09) -= 2;
                if (!(4 < V(0xEE09))) V(0xEE09) = 4;
            }
            return 0;
        }
        uint8_t dir = 1;
        if (diff & 0x80) { diff = (uint8_t)(-diff); dir = 0xFF; }
        V(0xEE06) = (uint8_t)diff;
        V(0xEE07) = dir;
        if (diff >= 5) {
            V(0xEE06) = (uint8_t)(8 - diff);
            V(0xEE07) = (uint8_t)(-dir);
        }
        return 0;
    }
}

// $EB5C
static int aim(void) {
    if (keys() < 0) return -1;
    if (V(0xEE06)) {
        V(0xEE05) = (uint8_t)((V(0xEE05) + V(0xEE07)) & 7);
        draw_hunter();
        V(0xEE06)--;
    }
    V(0xEE0A) = 3;
    return 0;
}

// ---------------------------------------------------------------------------
// Animals
// ---------------------------------------------------------------------------
// $E769: draw animal x in its current frame, facing its way.
static void draw_animal(uint8_t x) {
    uint8_t id = (uint8_t)(V(0xEE0D + x) * 3 + 0x10 + V(0xEE29 + x));
    V(0xEEC5) = id;
    V(0xEEC6) = V(0xEE0F + x); V(0xEEC7) = V(0xEE11 + x);
    V(0xEEC8) = V(0xEE13 + x); V(0xEEC9) = V(0xEE15 + x);
    if ((V(0xEE9D + id) ^ V(0xEE2B + x)) & 0x80) flip(id);
    draw();
}

// $E859: a new vertical drift, -2..2.
static void new_dy(uint8_t x) {
    int d = rnd8(5) - 2;
    V(0xEE23 + x) = (uint8_t)d;
    V(0xEE25 + x) = d < 0 ? 0xFF : 0;
}

// $E150: an empty slot's timer ran out: maybe a new animal.
static void spawn(uint8_t x) {
    V(0xE26C) = x;
    if (V(0xEDFA) < 4) {
        uint16_t r = rnd16(1000);
        // $E163: the high bytes' compare always leaves carry set, so only
        // the low byte is tested against the threshold
        int c = (r >> 8) >= V(0xEDFC);
        int lo = (r & 0xFF) - V(0xEDFB) - !c;
        if (lo < 0) goto appear;
    }
    V(0xEE2D + V(0xE26C)) = (uint8_t)(rnd8(7) | 1);         // $E16C
    return;
appear: ;
    uint8_t type = V(0xEE32 + rnd8(V(0xEE31)));             // $E17A
    x = V(0xE26C);
    V(0xEE0D + x) = type;
    uint16_t rec = (uint16_t)(0xE068 + 8 * type);
    V(0xEE27 + x) = V(rec);
    V(0xEE27 + x) = (uint8_t)(V(rec) + rnd8((uint8_t)(V(rec + 1) + 1)));
    for (;;) {                                              // $E1BD: side, drift, row
        uint8_t r = rnd8(100);
        x = V(0xE26C);
        if (r & 1) {                                        // from the left
            uint8_t w = (uint8_t)(-(int)V(rec + 6));
            V(0xEE0F + x) = w; Z(0x86) = w;
            V(0xEE11 + x) = 0xFF; Z(0x87) = 0xFF;
            V(0xEE1F + x) = 4;
            V(0xEE21 + x) = 0;
            V(0xEE2B + x) = 0;
        } else {                                            // from the right
            V(0xEE0F + x) = 0x18; Z(0x86) = 0x18;
            V(0xEE11 + x) = 0x01; Z(0x87) = 0x01;
            V(0xEE1F + x) = 0xFC;
            V(0xEE21 + x) = 0xFF;
            V(0xEE2B + x) = 0x80;
        }
        int v = V(rec + 6) - 1 + V(0xEE0F + x);             // $E20A
        V(0xEE17 + x) = (uint8_t)v; Z(0x8A) = (uint8_t)v;
        V(0xEE19 + x) = (uint8_t)(V(0xEE11 + x) + (v >> 8)); Z(0x8B) = V(0xEE19 + x);
        new_dy(x);
        // $E239: LDA #$C0 : INY : SBC ($EC),Y meant the height (byte 7), but Y
        // is 0 after a drift of 0 or more and $FF after a negative one, so the
        // row range subtracts the pace range (byte 1) or the pace base (byte 0).
        uint8_t yi = V(0xEE25 + x) == 0xFF ? 0 : 1;
        uint8_t y = rnd8((uint8_t)(0xC0 - V(rec + yi)));
        x = V(0xE26C);
        V(0xEE13 + x) = y; Z(0x88) = y;
        v = y - 1 + V(rec + 7);
        V(0xEE1B + x) = (uint8_t)v; Z(0x8C) = (uint8_t)v;
        V(0xEE15 + x) = 0; V(0xEE1D + x) = 0;
        Z(0x89) = 0; Z(0x8D) = 0;
        uint8_t who;
        if (!hits_but(x, &who)) return;
    }
}

// $E7A0: an animal's timer ran out: it takes a step.
static void move(uint8_t x) {
    V(0xE9B6) = x;
    V(0xEE2D + x) = V(0xEE27 + x);
    V(0xE9B7) = 3;
    V(0xE9B8) = V(0xEE1F + x);
    V(0xE9B9) = V(0xEE21 + x);
    if (++V(0xEE29 + x) == 3) V(0xEE29 + x) = 0;
    for (;;) {
        x = V(0xE9B6);                                      // $E7CC: the new box
        int v;
        v = V(0xEE0F + x) + V(0xEE1F + x); Z(0x86) = (uint8_t)v;
        Z(0x87) = (uint8_t)(V(0xEE11 + x) + V(0xEE21 + x) + (v >> 8));
        v = V(0xEE13 + x) + V(0xEE23 + x); Z(0x88) = (uint8_t)v;
        Z(0x89) = (uint8_t)(V(0xEE15 + x) + V(0xEE25 + x) + (v >> 8));
        v = V(0xEE17 + x) + V(0xEE1F + x); Z(0x8A) = (uint8_t)v;
        Z(0x8B) = (uint8_t)(V(0xEE19 + x) + V(0xEE21 + x) + (v >> 8));
        v = V(0xEE1B + x) + V(0xEE23 + x); Z(0x8C) = (uint8_t)v;
        Z(0x8D) = (uint8_t)(V(0xEE1D + x) + V(0xEE25 + x) + (v >> 8));
        uint8_t who;
        if (!hits_but(x, &who)) break;
        if (--V(0xE9B7) == 0) {                             // $E81D: third try, turn back
            x = V(0xE9B6);
            V(0xEE2B + x) ^= 0x80;
            V(0xEE1F + x) = (uint8_t)(~V(0xE9B8) + 1);       // the carry is lost
            V(0xEE21 + x) = (uint8_t)~V(0xE9B9);
            V(0xEE2B + x) = V(0xEE21 + x);
            if (!(V(0xEE21 + x) | V(0xEE1F + x))) {
                if (V(0xEE2B + x) & 0x80) { V(0xEE1F + x) = 0xFC; V(0xEE21 + x) = 0xFF; }
                else V(0xEE1F + x) = 4;
            }
            new_dy(V(0xE9B6));
            draw_animal(V(0xE9B6));
            return;
        }
        // $E873: steer by the smaller overlap with the obstacle ($93-$9A)
        uint8_t a;
        a = (uint8_t)(Z(0x93) - Z(0x8A)); Z(0x93) = a; Z(0x94) = (a & 0x80) ? (uint8_t)-a : a;
        a = (uint8_t)(Z(0x95) - Z(0x8C)); Z(0x95) = a; Z(0x96) = (a & 0x80) ? (uint8_t)-a : a;
        a = (uint8_t)(Z(0x97) - Z(0x86)); Z(0x97) = a; Z(0x98) = (a & 0x80) ? (uint8_t)-a : a;
        a = (uint8_t)(Z(0x99) - Z(0x88)); Z(0x99) = a; Z(0x9A) = (a & 0x80) ? (uint8_t)-a : a;
        if (Z(0x94) >= Z(0x98)) Z(0x93) = Z(0x97);
        if (Z(0x96) >= Z(0x9A)) Z(0x95) = Z(0x99);
        x = V(0xE9B6);
        uint8_t ext = (Z(0x93) & 0x80) ? 0xFF : 0;          // $E8C7: dx
        int sum = Z(0x93) + V(0xEE1F + x);
        if (((uint8_t)sum ^ V(0xEE1F + x)) & 0x80) V(0xEE2B + x) ^= 0x80;
        V(0xEE1F + x) = (uint8_t)sum;
        V(0xEE21 + x) = (uint8_t)(ext + V(0xEE21 + x) + (sum >> 8));
        uint8_t d = V(0xEE1F + x);
        if (!(d & 0x80)) { if (d >= 5) V(0xEE1F + x) = 4; }
        else if (!(0xFC < d)) V(0xEE1F + x) = 0xFC;
        ext = (Z(0x95) & 0x80) ? 0xFF : 0;                  // $E908: dy
        sum = Z(0x95) + V(0xEE23 + x);
        V(0xEE23 + x) = (uint8_t)sum;
        V(0xEE25 + x) = (uint8_t)(ext + V(0xEE25 + x) + (sum >> 8));
        if (!(V(0xEE25 + x) & 0x80)) { if (V(0xEE23 + x) >= 8) V(0xEE23 + x) = 7; }
        else if (!(0xF9 >= V(0xEE23 + x))) V(0xEE23 + x) = 0xF9;   // inverted, as shipped
    }
    // $E93A: commit the move
    for (int i = 7; i >= 0; i--) Z(0x93 + i) = Z(0x86 + i);
    uint8_t x2 = V(0xE9B6);
    Z(0xEC) = V(0xEE0F + x2); Z(0xED) = V(0xEE11 + x2);
    Z(0xEE) = V(0xEE13 + x2); Z(0xEF) = V(0xEE15 + x2);
    Z(0xFA) = V(0xEE17 + x2); Z(0xFB) = V(0xEE19 + x2);
    Z(0xFC) = V(0xEE1B + x2); Z(0xFD) = V(0xEE1D + x2);
    Z(0x3C) = V(0xEE1F + x2);
    Z(0x3E) = V(0xEE23 + x2);
    erase_trail();
    if (gone()) {                                           // left the field
        V(0xEE0D + V(0xE9B6)) = (uint8_t)(0x80 | V(0xEE0D + V(0xE9B6)) >> 1);
        return;
    }
    x2 = V(0xE9B6);
    V(0xEE0F + x2) = Z(0x93); V(0xEE11 + x2) = Z(0x94);
    V(0xEE13 + x2) = Z(0x95); V(0xEE15 + x2) = Z(0x96);
    V(0xEE17 + x2) = Z(0x97); V(0xEE19 + x2) = Z(0x98);
    V(0xEE1B + x2) = Z(0x99); V(0xEE1D + x2) = Z(0x9A);
    draw_animal(x2);
}

// $EA73: animal x is shot: a carcass, and its meat.
static void kill(uint8_t x) {
    V(0xEB30) = x;
    uint8_t y = V(0xEDFA);
    V(0xEE3D + y) = V(0xEE0F + x); V(0xEEC6) = V(0xEE0F + x);
    V(0xEE43 + y) = V(0xEE11 + x); V(0xEEC7) = V(0xEE11 + x);
    V(0xEE49 + y) = V(0xEE13 + x); V(0xEEC8) = V(0xEE13 + x);
    V(0xEE4F + y) = V(0xEE15 + x); V(0xEEC9) = V(0xEE15 + x);
    V(0xEE55 + y) = V(0xEE17 + x);
    V(0xEE5B + y) = V(0xEE19 + x);
    V(0xEE61 + y) = V(0xEE1B + x);
    V(0xEE67 + y) = V(0xEE1D + x);
    uint8_t type = V(0xEE0D + x);
    uint16_t rec = (uint16_t)(0xE068 + 8 * type);
    if (type < 6) {             // a shot that met no box picks an empty slot; see REQ-192f
        uint16_t range = (uint16_t)((V(rec + 4) | V(rec + 5) << 8) + 1);
        uint16_t lb = (uint16_t)(rnd16(range) + (V(rec + 2) | V(rec + 3) << 8));
        uint16_t tot = (uint16_t)((V(0xEDF5) | V(0xEDF6) << 8) + lb);
        V(0xEDF5) = (uint8_t)tot; V(0xEDF6) = (uint8_t)(tot >> 8);
    }
    x = V(0xEB30);
    if (y < 6) { H.dead_type[y] = V(0xEE0D + x); H.dead_flip[y] = V(0xEE2B + x) & 0x80; }
    uint8_t id = (uint8_t)(V(0xEE0D + x) + 0x22);
    V(0xEEC5) = id;
    if ((V(0xEE9D + id) ^ V(0xEE2B + x)) & 0x80) flip(id);
    draw();
    V(0xEE2D + V(0xEB30)) = (uint8_t)(rnd8(4) | 1);
    x = V(0xEB30);
    V(0xEE0D + x) = (uint8_t)(0x80 | V(0xEE0D + x) >> 1);
    V(0xEDFA)++;
}

// ---------------------------------------------------------------------------
// The shot
// ---------------------------------------------------------------------------
// $F605: the two pixels at (x, y) and (x+1, y): 0 when both are dark.
static uint8_t pixels(uint8_t xlo, uint8_t xhi, uint8_t y) {
    uint16_t row = row_at(y);
    Z(0x93) = (uint8_t)row; Z(0x94) = (uint8_t)(row >> 8);
    Z(0x95) = xlo; Z(0x96) = xhi;
    uint8_t rem;
    uint8_t col = div7(xlo, xhi, &rem);
    uint8_t a = *ram((uint16_t)(row + col));
    if (rem == 6)                                           // pixel 6, and pixel 0 of the next byte
        a = (uint8_t)(((a >> 6) & 1) | (*ram((uint16_t)(row + (uint8_t)(col + 1))) & 1) << 1);
    else a >>= rem;
    cost(80);
    return COLOUR[a & 3];
}

// $E9BA: every tick while a shot is in flight.
static void shot(void) {
    bullet_toggle();
    uint8_t a = pixels(V(0xEE38), V(0xEE39), V(0xEE3A));
    a |= pixels(V(0xEE38), V(0xEE39), (uint8_t)(V(0xEE3A) + 1));
    V(0xEA72) = a;
    if (!a) {                                               // move on, twice a step
        uint8_t d = V(0xEE3C);
        int v = (uint8_t)(V(0xE006 + d) << 1) + V(0xEE38);
        V(0xEE38) = (uint8_t)v; Z(0x86) = (uint8_t)v;
        V(0xEE39) = (uint8_t)(V(0xEE39) + V(0xE00E + d) + (v >> 8)); Z(0x87) = V(0xEE39);
        v = (uint8_t)(V(0xE016 + d) << 1) + V(0xEE3A);
        V(0xEE3A) = (uint8_t)v; Z(0x88) = (uint8_t)v;
        V(0xEE3B) = (uint8_t)(V(0xEE3B) + V(0xE01E + d) + (v >> 8)); Z(0x89) = V(0xEE3B);
        uint8_t xl = (uint8_t)(Z(0x86) + 1), xh = (uint8_t)(Z(0x87) + (xl == 0));
        Z(0x8A) = xl; Z(0x8B) = xh;
        Z(0x8C) = (uint8_t)(Z(0x88) + 1); Z(0x8D) = Z(0x89);
        if (outside()) { V(0xEDF7) >>= 1; return; }         // off the field
        V(0xEE38) = Z(0x86); V(0xEE39) = Z(0x87); V(0xEE3A) = Z(0x88);
        bullet_xor(Z(0x86), Z(0x87), Z(0x88));
        return;
    }
    V(0xEDF7) >>= 1;                                        // $EA44: it hit something
    Z(0x86) = V(0xEE38); Z(0x87) = V(0xEE39);
    uint8_t xl = (uint8_t)(Z(0x86) + 1);
    Z(0x8A) = xl; Z(0x8B) = (uint8_t)(Z(0x87) + (xl == 0));
    Z(0x88) = V(0xEE3A); Z(0x89) = V(0xEE3B);
    Z(0x8C) = (uint8_t)(Z(0x88) + 1); Z(0x8D) = Z(0x89);
    uint8_t who;
    hits(&who);
    // A shot that met no box leaves whatever index the loops last held
    // (REQ-192f): a live animal is still killed, as in the original, but a
    // dead or empty slot, or a carcass's index past the two animal slots,
    // which drew garbage in the original, is skipped.
    if (who < 2 && !(V(0xEE0D + who) & 0x80)) kill(who);
}

// ---------------------------------------------------------------------------
// Setting up
// ---------------------------------------------------------------------------
// $F15A: shift tables for shifts 1-6 at base..base+$5FF: for each 7-bit
// value v, v << s (low seven bits) at +0 and what spills over at +$80.
static void shift_tables(uint16_t base) {
    for (int s = 1; s <= 6; s++)
        for (int v = 0; v < 128; v++) {
            H.m->ram[(uint16_t)(base + (s - 1) * 256 + v)] = (uint8_t)((v << s) & 0x7F);
            H.m->ram[(uint16_t)(base + (s - 1) * 256 + 0x80 + v)] = (uint8_t)(v >> (7 - s));
        }
    cost(359694);                                           // measured
}

// $E2A7: 4-7 pieces of the zone's scenery, placed clear of everything.
static void scenery(void) {
    uint8_t zone = V(0xEE6D);
    V(0xE381) = (uint8_t)(zone * 6);
    V(0xE37C) = (uint8_t)(rnd8(4) + 4);
    V(0xEDF9) = 0;
    Z(0x89) = 0; Z(0x8D) = 0;
    V(0xEEC9) = 0;
    do {
        uint8_t id = (uint8_t)(V(0xE04A + (uint8_t)(rnd8(6) + V(0xE381))) + 0x28);
        uint8_t h;
        V(0xE37F) = hunt_size(id, &h);
        V(0xE380) = h;
        V(0xEEC5) = id;
        uint8_t r;
        V(0xE37D) = (uint8_t)(div7((uint8_t)((uint8_t)(0x18 - V(0xE37F)) >> 1), 0, &r) + 1);
        V(0xE37E) = (uint8_t)(0xC0 - V(0xE380));
        uint8_t who;
        do {                                                // $E30C
            uint16_t p = mul8(rnd8(V(0xE37D)), 0x0E);
            Z(0x86) = (uint8_t)p; Z(0x87) = 0;              // $EEE1 leaves X = 0
            int v = Z(0x86) + V(0xE37F);
            Z(0x8A) = (uint8_t)v; Z(0x8B) = (uint8_t)(v >> 8);
            if (Z(0x8A) == 0) Z(0x8B)--;
            Z(0x8A)--;
            Z(0x88) = rnd8(V(0xE37E));
            Z(0x8C) = (uint8_t)(Z(0x88) + V(0xE380) - 1);
        } while (hits(&who));
        uint8_t x = V(0xEDF9);
        if (x < 8) H.scen_id[x] = id;
        V(0xEE6E + x) = Z(0x86); V(0xEEC6) = Z(0x86);
        V(0xEE76 + x) = Z(0x87); V(0xEEC7) = Z(0x87);
        V(0xEE7E + x) = Z(0x88); V(0xEEC8) = Z(0x88);
        V(0xEE86 + x) = Z(0x8A);
        V(0xEE8E + x) = Z(0x8B);
        V(0xEE96 + x) = Z(0x8C);
        draw();
        V(0xEDF9)++;
    } while (--V(0xE37C));
}

// $E56C: the whole start of a hunt.
static void setup(int bullets, const uint8_t flags[4], int zone) {
    Mach *m = H.m;
    V(0xEDF1) = (uint8_t)bullets; V(0xEDF2) = (uint8_t)(bullets >> 8);
    V(0xEDFB) = 4; V(0xEDFC) = 0;
    if (flags[3]) { V(0xEDFB) = 2; }                        // LSR $EDFC : ROR $EDFB
    uint8_t x = 0;
    for (V(0xE6F2) = 0; V(0xE6F2) < 3; V(0xE6F2)++)         // the regions' animals
        if (flags[V(0xE6F2)]) V(0xEE35 + x++) = V(0xE6F2);
    V(0xEE31) = (uint8_t)(x + 3);
    V(0xEE6D) = (uint8_t)zone;
    // $E5BB: seed the generator from the keyboard wait counter and Applesoft's RND seed
    uint8_t seed[6];
    seed[0] = (uint8_t)((m->ram[0x4E] ^ m->ram[0x4F]) | 1);
    for (int i = 4; i >= 0; i--) seed[1 + i] = m->ram[0xC9 + i] ^ m->ram[0x4E];
    memcpy(&V(0xF5CF), seed, 6);                            // $F552
    V(0xF5CF) |= 1;
    V(0xF5DB) = 1;
    V(0xEDF9) = 0; V(0xEDFA) = 0; V(0xEDF7) = 0; V(0xEE08) = 0; V(0xEE0C) = 0;
    V(0xEDF5) = 0; V(0xEDF6) = 0; V(0xEE00) = 0; V(0xEE04) = 0; V(0xEDF8) = 0;
    m->ram[0x32A] = 0x00; m->ram[0x32B] = 0x3A;             // & DFT slot 13: shift tables
    shift_tables(0x3A00);
    Z(0x86) = 0; Z(0x88) = 0; Z(0x87) = 0; Z(0x89) = 0;     // clear the field
    Z(0x8A) = 0x17; Z(0x8B) = 0x01; Z(0x8C) = 0xBF; Z(0x8D) = 0;
    erase();
    V(0xEE05) = rnd8(8);                                    // the hunter, somewhere
    uint8_t h;
    V(0xEE01) = hunt_size(1, &h);
    V(0xEE03) = h;
    uint16_t range = (uint16_t)(0x118 - V(0xEE01));
    uint16_t hx = rnd16(range);
    V(0xEDFD) = (uint8_t)hx; V(0xEDFE) = (uint8_t)(hx >> 8);
    uint16_t x2 = (uint16_t)(hx + V(0xEE01) - 1);
    V(0xEE01) = (uint8_t)x2; V(0xEE02) = (uint8_t)(x2 >> 8);
    V(0xEDFF) = rnd8((uint8_t)(0xC0 - V(0xEE03)));
    V(0xEE03) = (uint8_t)(V(0xEDFF) + V(0xEE03) - 1);
    for (V(0xE6F2) = 0; V(0xE6F2) < 6; V(0xE6F2)++) {       // $E68A: the animals' sizes
        uint16_t rec = (uint16_t)(0xE06E + 8 * V(0xE6F2));
        V(rec) = hunt_size((uint8_t)(V(0xE6F2) * 3 + 0x10), &h);
        V(rec + 1) = h;
    }
    scenery();
    draw_hunter();
    for (x = 0; x < 2; x++) {                               // both slots empty, timed
        V(0xEE2D + x) = (uint8_t)(rnd8(7) | 1);
        V(0xEE0D + x) = 0x80;
    }
    V(0xEE0A) = 3;
    V(0xEE0B) = 8;
    V(0xEDF3) = 0xC4; V(0xEDF4) = 0x09;                     // 2500 ticks
    mach_key_clear(m);
}

int hunt_play(Mach *m, int bullets, int b1, int b2, int b3, int b4, int zone,
              int *pounds, int *bullets_left) {
    H.m = m;
    H.cyc = 0;
    if (!H.ready && !load_sets(m)) {                        // no sprites: nothing to hunt
        *pounds = 0; *bullets_left = bullets;
        return 0;
    }
    uint8_t flags[4] = { (uint8_t)b1, (uint8_t)b2, (uint8_t)b3, (uint8_t)b4 };
    setup(bullets, flags, zone);
    H.running = 1;
    pace();
    for (;;) {                                              // $E0B3
        inc_4e();
        cost(62);
        if (--V(0xEE0A) == 0 && aim() < 0) { pace(); H.running = 0; return -1; }
        if (--V(0xEE0B) == 0) walk();
        if (V(0xEDF7) & 0x80) shot();
        for (V(0xE14E) = 0; V(0xE14E) < 2; V(0xE14E)++) {
            uint8_t x = V(0xE14E);
            cost(28);
            if (--V(0xEE2D + x)) { cost(3852); continue; }  // $E0EF: the idle delay
            if (V(0xEE0D + x) & 0x80) spawn(x);
            else move(x);
        }
        pace();
        uint16_t t = (uint16_t)((V(0xEDF3) | V(0xEDF4) << 8) - 1);
        V(0xEDF3) = (uint8_t)t; V(0xEDF4) = (uint8_t)(t >> 8);
        if (!t) break;
    }
    bullet_toggle();                                        // $E116
    *bullets_left = V(0xEDF1) | V(0xEDF2) << 8;
    *pounds = V(0xEDF5) | V(0xEDF6) << 8;
    pace();
    H.running = 0;
    return 0;
}

// ---------------------------------------------------------------------------
// The field, for other presenters
// ---------------------------------------------------------------------------
void hunt_set_frame_hook(void (*fn)(void *ctx), void *ctx) { H.hook = fn; H.hook_ctx = ctx; }

int hunt_running(void) { return H.running; }

void hunt_view(HuntView *v) {
    memset(v, 0, sizeof *v);
    v->zone = V(0xEE6D);
    v->bullets = V(0xEDF1) | V(0xEDF2) << 8;
    for (int i = 0; i < V(0xEDF9) && i < 8 && v->n < 24; i++) {          // scenery
        HuntSprite *p = &v->s[v->n++];
        p->kind = HV_SCENERY; p->img = H.scen_id[i] - 0x28 + 1;          // HUNT_SCENERY 7.. -> 1..
        p->x1 = s16(V(0xEE6E + i), V(0xEE76 + i)); p->y1 = V(0xEE7E + i);
        p->x2 = s16(V(0xEE86 + i), V(0xEE8E + i)); p->y2 = V(0xEE96 + i);
    }
    for (int i = 0; i < V(0xEDFA) && i < 6 && v->n < 24; i++) {          // the shot animals
        HuntSprite *p = &v->s[v->n++];
        p->kind = HV_DEAD; p->img = H.dead_type[i]; p->mirrored = !!H.dead_flip[i];
        p->x1 = s16(V(0xEE3D + i), V(0xEE43 + i)); p->y1 = s16(V(0xEE49 + i), V(0xEE4F + i));
        p->x2 = s16(V(0xEE55 + i), V(0xEE5B + i)); p->y2 = s16(V(0xEE61 + i), V(0xEE67 + i));
    }
    for (int x = 0; x < 2 && v->n < 24; x++) {                          // the animals
        if (V(0xEE0D + x) & 0x80) continue;
        HuntSprite *p = &v->s[v->n++];
        p->kind = HV_ANIMAL; p->img = V(0xEE0D + x); p->frame = V(0xEE29 + x);
        p->mirrored = !!(V(0xEE2B + x) & 0x80);
        p->x1 = s16(V(0xEE0F + x), V(0xEE11 + x)); p->y1 = s16(V(0xEE13 + x), V(0xEE15 + x));
        p->x2 = s16(V(0xEE17 + x), V(0xEE19 + x)); p->y2 = s16(V(0xEE1B + x), V(0xEE1D + x));
    }
    if (v->n < 24) {                                                    // the hunter
        HuntSprite *p = &v->s[v->n++];
        uint8_t pose = V(0xE03E + V(0xEE05));
        p->kind = HV_HUNTER; p->img = pose & 0x7F; p->frame = V(0xEE0C) & 3; p->mirrored = !!(pose & 0x80);
        p->x1 = s16(V(0xEDFD), V(0xEDFE)); p->y1 = V(0xEDFF);
        p->x2 = s16(V(0xEE01), V(0xEE02)); p->y2 = V(0xEE03);
    }
    if (V(0xEDF7) & 0x80) { v->shot = 1; v->sx = s16(V(0xEE38), V(0xEE39)); v->sy = V(0xEE3A); }
}
