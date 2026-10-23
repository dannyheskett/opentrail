// & PT and & PN: the extension's tune player ($8AF2, $8B3A) and its tone
// generator ($7F2B), timed to the cycle so the music plays at its original
// pitch and tempo (spec §29.6, REQ-279..279g).
#include "mach.h"

// $7FC8: two half-period counts (t0, t1) per pitch, pitch 0 = rest.
static const uint8_t HALF[51][2] = {
    {0,0},{246,246},{232,232},{219,219},{207,207},{195,195},{184,184},{174,174},
    {164,164},{155,155},{146,146},{138,138},{130,130},{123,123},{116,116},{108,110},
    {103,104},{97,98},{92,92},{87,87},{82,82},{77,78},{73,73},{69,69},{65,65},{61,62},
    {58,58},{54,55},{51,52},{48,49},{46,46},{43,44},{41,41},{38,39},{36,37},{34,35},
    {32,33},{30,31},{29,29},{27,28},{26,26},{24,25},{23,23},{21,22},{20,21},{19,20},
    {18,18},{17,17},{16,16},{15,16},{14,15},
};

#define PASS 23             // cycles per pass of every loop in $7F2B
// Measured against the original: $8B89 + $7F2B entry cost
// per note, and the & dispatcher's name search plus argument parsing per command.
#define NOTE_SETUP 258
#define PT_CALL 47560

// $7F2B: one note. The pass counter runs 256 x duration passes (65536 for a
// duration of 0); each carry of its low byte costs 2 more cycles, and each
// half period ends 2 cycles late, when the speaker is toggled again.
void tune_note(Mach *m, int pitch, int dur) {
    mach_advance(m, NOTE_SETUP);
    long passes = dur ? 256L * dur : 65536L;
    int t0 = pitch < 51 ? HALF[pitch][0] : 0, t1 = pitch < 51 ? HALF[pitch][1] : 0;
    int skew = t0 >> 5;
    int h1 = t0 - skew, h2 = t1 + skew;             // $3D, $3C
    long done = 0;                                  // passes counted so far
    if (!h1) {                                      // rest ($7FA7)
        mach_advance(m, (unsigned long long)(passes * PASS + 2 * (passes / 256)));
        return;
    }
    for (int half = 0; ; half ^= 1) {
        mach_click(m);                              // LDA $C030
        long n = half ? h2 : h1;
        if (!n) n = 256;                            // DEY from 0 wraps
        long run = n < passes - done ? n : passes - done;
        long carries = (done + run) / 256 - done / 256;
        mach_advance(m, (unsigned long long)(run * PASS + 2 * carries + 2));
        done += run;
        if (done >= passes) return;
    }
}

// $8B58: the address of tune n in the buffer, or 0 for an illegal tune number.
static int tune_start(const Mach *m, uint16_t buf, int n) {
    if (n <= 0 || n > m->ram[buf]) return -1;
    return (uint16_t)(buf + (m->ram[(uint16_t)(buf + 2 * n)] | m->ram[(uint16_t)(buf + 2 * n + 1)] << 8));
}

static TuneState *st(Mach *m) { return &m->tune; }

// $8B89: play the note at the pointer; at the end marker, rewind to the tune's
// start and report the end (carry set).
static int play_next(Mach *m) {
    TuneState *t = st(m);
    uint8_t pitch = m->ram[t->ptr];
    if (pitch & 0x80) {
        int s = tune_start(m, gfx_set_addr(m, 15), t->tune);
        if (s >= 0) t->ptr = (uint16_t)s;
        return 1;
    }
    tune_note(m, pitch, m->ram[(uint16_t)(t->ptr + 1)]);
    t->ptr = (uint16_t)(t->ptr + 2);
    return 0;
}

static int select_tune(Mach *m, int n) {
    int s = tune_start(m, gfx_set_addr(m, 15), n);
    if (s < 0) return 0;                            // ?ILLEGAL QUANTITY in the original
    m->tune.tune = n;
    m->tune.ptr = (uint16_t)s;
    return 1;
}

// & PT [n [,abort]]: play (the rest of) the tune; with abort non-zero, a key
// press stops it after the current note (the key stays in the latch).
void amp_pt(Mach *m, int n, int abort_on_key) {
    mach_advance(m, PT_CALL);
    if (n != ARG_NONE && !select_tune(m, n)) return;
    int abort = abort_on_key != ARG_NONE && abort_on_key != 0;
    for (;;) {
        if (play_next(m)) return;
        if (abort && (mach_key_peek(m) & 0x80)) return;
    }
}

// & PN [n]: play one note (selecting tune n first if given).
void amp_pn(Mach *m, int n) {
    mach_advance(m, PT_CALL);
    if (n != ARG_NONE && !select_tune(m, n)) return;
    play_next(m);
}
