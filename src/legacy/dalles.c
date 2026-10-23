// DALLESUTIL.OBJ rebuilt. See dalles.h. Addresses in comments are the
// original's ($3000-$3BF4).
#include "dalles.h"
#include "mach.h"

#define RAM(a) m->ram[(uint16_t)(a)]
#define REC(n) ((uint16_t)(0x3A85 + 16 * (n)))     // $39EA: sprite records
#define RECT(k) ((uint16_t)(0x3AB5 + 4 * (k)))     // $3AB5: col1, y1, col2, y2
#define FLAG(k) RAM(0x3AC1 + (k))                   // $3AC1: taken off / put back
#define RES(k) RAM(0x333D + (k))                    // $333D: overlap results

// Cycle costs, calibrated on the original: each
// command's statement including Applesoft's argument parsing, plus its work.
#define DALLES_CMD_CYCLES 14000         // DPB, KILP, RMOVP
#define DALLES_PUTP_CYCLES 39000
#define DALLES_CST_CYCLES 77000
#define DALLES_IMAGE_CYCLES 27500
#define DALLES_IMAGE_PLAIN 25
#define DALLES_IMAGE_SHIFTED 63
#define DALLES_DRAW_CYCLES 400
#define DALLES_DRAW_BYTE 65
#define DALLES_RESTORE_CYCLES 300
#define DALLES_RESTORE_BYTE 20

void dalles_install(Mach *m) {
    for (int a = 0x3A85; a <= 0x3AC3; a++) RAM(a) = 0;
    for (int a = 0x333D; a <= 0x3345; a++) RAM(a) = 0;
    mach_advance(m, 60000);                         // the & RNH load, roughly
}

// $3783: x -> byte column and shift, rounding down for negative x
static void d_xsplit(int x, uint8_t *col, int *shift) {
    x = (int16_t)x;
    int q = x >= 0 ? x / 7 : -((-x + 6) / 7);
    *col = (uint8_t)q;
    *shift = x - q * 7;
}

// $349E: image n of the set in slot 7 (the index is assumed at offset 0)
static uint16_t d_lookup(const Mach *m, int n, int *w, int *h) {
    uint16_t set = m->gfx.set[7];
    int y = ((n * 2) & 0xFF) + 1;
    uint8_t hi = m->ram[(uint16_t)(set + (y & 0xFF))], lo = m->ram[(uint16_t)(set + ((y - 1) & 0xFF))];
    uint16_t p = (uint16_t)(set + (lo | (hi & 0x3F) << 8));
    *w = m->ram[p];
    *h = m->ram[(uint16_t)(p + 1)];
    return (uint16_t)(p + 2);
}

// shift tables: shift s at base + 256 s, lo[v] = v << s (7 bits), hi[v] = v >> (7 - s)
static uint8_t d_lo(const Mach *m, int s, int v) { return m->ram[(uint16_t)(m->gfx.shift_base + s * 256 + v)]; }
static uint8_t d_hi(const Mach *m, int s, int v) { return m->ram[(uint16_t)(m->gfx.shift_base + s * 256 + 128 + v)]; }

static uint16_t d_row(const Mach *m, int y) { return (uint16_t)(m->hpag * 256 + hires_row(y)); }   // $34FB/$35BB

void dalles_cst(Mach *m, int addr) {                                // $3AD8, $33F9
    uint16_t base = (uint16_t)addr;
    m->gfx.shift_base = base;
    for (int s = 0; s <= 6; s++)
        for (int v = 0; v < 128; v++) {
            RAM(base + s * 256 + v) = (uint8_t)((v << s) & 0x7F);
            RAM(base + s * 256 + 128 + v) = (uint8_t)(v >> (7 - s));
        }
    mach_advance(m, DALLES_CST_CYCLES);
}

// $3AEF, $36CC: a raw image; every screen byte it covers takes its pixels and
// (except the spill byte past the right edge) the image byte's high bit.
void dalles_image(Mach *m, int n, int x, int y) {
    int w, h, s;
    uint8_t col;
    uint16_t src = d_lookup(m, n & 0xFF, &w, &h);
    d_xsplit(x, &col, &s);
    uint8_t row = (uint8_t)y;
    static const uint8_t MASK[7] = { 0x00, 0x01, 0x03, 0x07, 0x0F, 0x1F, 0x3F };   // $377C
    long bytes = 0;
    uint8_t rows = (uint8_t)h;
    do {                                                            // $3702, DEC $94
        if (row < 192) {
            uint16_t a = d_row(m, row);
            uint8_t yy = col;
            uint8_t cnt = (uint8_t)w;
            if (s == 0) {                                           // $367B
                int i = 0;
                do {
                    if (yy < 40) RAM(a + yy) = RAM(src + i);
                    i++; yy++; bytes++;
                } while (--cnt);
            } else {
                uint8_t carry = RAM(a + yy) & MASK[s];              // $371D
                int v = 0, i = 0;
                do {
                    uint8_t d = RAM(src + i);
                    v = d >> 7;
                    if (yy < 40) RAM(a + yy) = (uint8_t)(d_lo(m, s, d & 0x7F) | carry | (v ? 0x80 : 0));
                    carry = d_hi(m, s, d & 0x7F);
                    i++; yy++; bytes++;
                } while (--cnt);
                if (yy < 40) {                                      // $3752: the spill keeps its high bit
                    uint8_t c = (uint8_t)(carry | (v ? 0x80 : 0));
                    RAM(a + yy) = (uint8_t)(((c ^ RAM(a + yy)) & MASK[s]) ^ RAM(a + yy));
                }
            }
        }
        row++;
        src = (uint16_t)(src + w);
    } while (--rows);
    mach_advance(m, DALLES_IMAGE_CYCLES + (unsigned long long)bytes * (s ? DALLES_IMAGE_SHIFTED : DALLES_IMAGE_PLAIN));
}

// ---------------------------------------------------------------------------
// Sprites
// ---------------------------------------------------------------------------
// $30CB: draw sprite k at its position, saving the bytes it covers.
static void d_draw(Mach *m, int k) {
    uint16_t r = REC(k);
    RAM(r) = 0x80;                                                  // $3A19
    int s = RAM(r + 1);
    uint8_t col = RAM(r + 2), w = RAM(r + 3), y = RAM(r + 4), h = RAM(r + 5);
    uint16_t mask = (uint16_t)(RAM(r + 6) | RAM(r + 7) << 8);
    uint16_t pic = (uint16_t)(RAM(r + 8) | RAM(r + 9) << 8);
    uint16_t save = (uint16_t)(RAM(r + 14) | RAM(r + 15) << 8);
    uint8_t first = d_hi(m, s, 0x7F);                               // $30E9: pixels left of x kept
    uint8_t beyond = first ^ 0x7F;
    uint8_t x = 0;                                                  // the X register
    long bytes = 0;
    uint8_t rows = h;
    do {
        if (y >= 192) {                                             // $3100
            save++;
            x += w;
        } else {
            uint16_t a = d_row(m, y);
            uint8_t yy = col, mc = first, pc = 0, andv = 0, eorv = 0, hb = 0;
            int left = w;
            for (;;) {
                if (left > 0) {                                     // $3134
                    uint8_t mv = RAM(mask + x) & 0x7F;
                    andv = d_lo(m, s, mv) | mc;
                    mc = d_hi(m, s, mv);
                    uint8_t p = RAM(pic + x);
                    hb = p & 0x80;
                    eorv = (uint8_t)(d_lo(m, s, p & 0x7F) | hb | pc);
                    pc = d_hi(m, s, p & 0x7F);
                } else if (left == 0) {                             // $317D: the spill byte
                    save++;
                    x--;
                    if (yy < 40) { andv = mc | beyond; eorv = pc | hb; }
                }
                if (yy < 40) {                                      // $3162
                    uint8_t v = RAM(a + yy);
                    RAM(save + x) = v;
                    RAM(a + yy) = (uint8_t)((v & andv) ^ eorv);
                    bytes++;
                }
                x++; yy++;
                if (--left < 0) break;
            }
        }
        y++;
    } while (--rows);
    mach_advance(m, DALLES_DRAW_CYCLES + (unsigned long long)bytes * DALLES_DRAW_BYTE);
}

// $31AD: put back what sprite k's last drawing covered.
static void d_restore(Mach *m, int k) {
    uint16_t r = REC(k);
    if (!(RAM(r) & 0x80)) return;                                   // $3A62
    uint8_t col = RAM(r + 10), w = RAM(r + 11), y = RAM(r + 12), h = RAM(r + 13);
    uint16_t save = (uint16_t)(RAM(r + 14) | RAM(r + 15) << 8);
    uint8_t end = (uint8_t)(col + w), yend = (uint8_t)(y + h);
    uint8_t x = 0;
    long bytes = 0;
    for (uint8_t row = y; ; ) {
        if (row >= 192) x += w;
        else {
            uint16_t a = d_row(m, row);
            uint8_t yy = col;
            do {
                if (yy < 40) { RAM(a + yy) = RAM(save + x); bytes++; }
                x++; yy++;
            } while (yy != end);
        }
        row++;
        if (row == yend) break;
    }
    mach_advance(m, DALLES_RESTORE_CYCLES + (unsigned long long)bytes * DALLES_RESTORE_BYTE);
}

// $32F6: inclusive interval test on values offset by imm and saturated at 255
static int d_overlap(int a1, int a2, int b1, int b2, int imm) {
    #define SAT(v) ((v) + imm > 255 ? 255 : (v) + imm)
    int A1 = SAT(a1), A2 = SAT(a2), B1 = SAT(b1), B2 = SAT(b2);
    #undef SAT
    if (A1 == B2) return 1;
    if (A1 > B2) return 0;
    if (A1 >= B1) return 1;
    if (A2 >= B2) return 1;
    return A2 >= B1;
}

// $328A + $39C9: test rect k against rects 0..k-1, mark the overlapping ones;
// returns 1 if any of 0..k-1 is now marked (already marked ones count).
static int d_test(Mach *m, int k) {
    uint16_t t = RECT(k);
    int any = 0;
    for (int i = 0; i < k; i++) {
        uint16_t u = RECT(i);
        int xo = d_overlap(RAM(t), RAM(t + 2), RAM(u), RAM(u + 2), 0xD7);
        int yo = d_overlap(RAM(t + 1), RAM(t + 3), RAM(u + 1), RAM(u + 3), 0x3F);
        RES(i) = (uint8_t)((xo && yo) ? 0x80 : 0);
    }
    for (int i = k - 1; i >= 0; i--) {
        FLAG(i) |= RES(i) & 0x80;
        if (FLAG(i) & 0x80) any = 1;
    }
    mach_advance(m, 700 + 250ULL * (unsigned)k);
    return any;
}

// $38BA: which sprites must come off and go back on around slot's change.
static void d_closure(Mach *m, int slot) {
    for (int k = slot; k >= 0; k--) FLAG(k) = 0;
    FLAG(slot) = 0x80;
    if (slot == 0) return;
    for (int k = slot; k >= 0; k--) {                               // $3969: last positions
        uint16_t r = REC(k), t = RECT(k);
        RAM(t) = RAM(r + 10);
        RAM(t + 2) = (uint8_t)(RAM(r + 10) + RAM(r + 11) - 1);
        RAM(t + 1) = RAM(r + 12);
        RAM(t + 3) = (uint8_t)(RAM(r + 12) + RAM(r + 13) - 1);
    }
    d_test(m, slot);
    uint16_t r = REC(slot), t = RECT(slot);                        // $38E5: the new position
    RAM(t) = RAM(r + 2);
    RAM(t + 2) = (uint8_t)(RAM(r + 2) + RAM(r + 3));
    RAM(t + 1) = RAM(r + 4);
    RAM(t + 3) = (uint8_t)(RAM(r + 4) + RAM(r + 5) - 1);
    int s = slot;
    for (;;) {                                                      // $390E
        if (!d_test(m, s)) return;
        if (--s == 0) return;
        int x = s;
        while (x >= 0 && !(FLAG(x) & 0x80)) x--;                   // $3925
        if (x < 0) return;
    }
}

static void d_restore_upto(Mach *m, int slot) {                    // $394B
    for (int k = 0; k <= slot; k++) if (FLAG(k) & 0x80) d_restore(m, k);
}

static void d_draw_from(Mach *m, int slot) {                       // $3932
    for (int k = slot; k >= 0; k--)
        if ((FLAG(k) & 0x80) && (RAM(REC(k)) & 0x80)) d_draw(m, k);
}

static void d_clear(Mach *m, int n) { for (int i = 0; i < 16; i++) RAM(REC(n) + i) = 0; }   // $39FF

void amp_dpb(Mach *m, int n, int addr) {                            // $3B55, $37AC
    d_clear(m, n);
    RAM(REC(n) + 14) = (uint8_t)addr;
    RAM(REC(n) + 15) = (uint8_t)(addr >> 8);
    mach_advance(m, DALLES_CMD_CYCLES);
}

void amp_kilp(Mach *m, int n) {                                     // $3B72, $37C6
    for (int i = 15; i >= 0; i--) RAM(REC(2) + i) = RAM(REC(n) + i);
    d_closure(m, 2);
    d_restore_upto(m, 1);
    d_clear(m, n);
    d_draw_from(m, 2);
    mach_advance(m, DALLES_CMD_CYCLES);
}

void amp_putp(Mach *m, int slot, int image, int x, int y) {         // $3B7F, $37FA
    uint16_t r = REC(slot);
    int w, h, w2, h2;
    uint16_t mp = d_lookup(m, image & 0xFF, &w, &h);
    RAM(r + 3) = (uint8_t)w;
    RAM(r + 5) = (uint8_t)h;
    RAM(r + 6) = (uint8_t)mp; RAM(r + 7) = (uint8_t)(mp >> 8);
    uint16_t pp = d_lookup(m, (image + 1) & 0xFF, &w2, &h2);
    RAM(r + 8) = (uint8_t)pp; RAM(r + 9) = (uint8_t)(pp >> 8);
    uint8_t col; int s;
    d_xsplit(x, &col, &s);
    RAM(r + 2) = col;
    RAM(r + 1) = (uint8_t)s;
    RAM(r + 4) = (uint8_t)y;
    d_closure(m, slot);
    d_restore_upto(m, slot);
    RAM(r) = 0x80;
    d_draw_from(m, slot);
    for (int i = 0; i < 4; i++) RAM(r + 10 + i) = RAM(r + 2 + i);  // $3871: remember the position
    RAM(r + 11)++;                                                  // one byte wider: the spill
    mach_advance(m, DALLES_PUTP_CYCLES);
}

void amp_rmovp(Mach *m, int n) {                                    // $3BBC, $3894
    d_closure(m, n);
    d_restore_upto(m, n);
    FLAG(n) = 0;
    d_draw_from(m, n);
    d_clear(m, n);
    mach_advance(m, DALLES_CMD_CYCLES);
}
