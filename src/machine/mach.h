// The Apple II the shell draws on: RAM for the hi-res pages, save-under
// buffers and loaded image sets, plus the state of the game's machine-language
// extension. Every & command the BASIC used has a C function here with the
// same arguments, so the ported display code reads like the original:
//   & BOX,X1,160,X2,180,3      ->  amp_box(m, x1, 160, x2, 180, 3)
// Omitted arguments are ARG_NONE. Each draws what the original machine code
// drew, pixel for pixel; drawing takes no time (only the game's own delays do).
//
// The machine never waits on its own: blocking commands (& SPACE, & INP, key
// reads) call m->idle() until a key arrives. m->cycles is the game's clock:
// the deliberate delays (music, animations, the hunt and the raft) charge the
// original's cycle counts, so the host paces them at the original's speed.
#ifndef OT_MACH_H
#define OT_MACH_H

#include <stdint.h>
#include <limits.h>

#define ARG_NONE INT_MIN
#define KEYQ 64
#define SPKQ 8192                   // speaker clicks not yet turned into sound
#define A2_HZ 1020484               // average 6502 cycles per second (spec REQ-279f)
#define FRAME_CYCLES (A2_HZ / 60)

#ifndef OT_MACH_TYPEDEF                 // C99 allows one typedef of a name
#define OT_MACH_TYPEDEF
typedef struct Mach Mach;
#endif

#include "text.h"       // TextState: fonts, windows, cursor, input (text.c)
#include "gfx.h"        // GfxState: image sets, shift tables, HCOLOR (gfx.c)
#include "tune.h"       // TuneState: & PT / & PN (tune.c)

struct Mach {
    uint8_t ram[65536];         // hi-res page 1 $2000, page 2 $4000, buffers, image sets
    uint8_t hpag;               // $E6: page drawn on ($20 or $40)
    uint8_t invflg;             // $32: $FF normal, $3F inverse (Applesoft INVERSE / NORMAL)
    int page2;                  // page shown ($C055 / $C054)
    int text_mode;              // text screen shown instead of graphics (TEXT, errors)
    int sound;                  // PEEK(975): tunes and Ctrl-S
    const char *version;        // "v1.0.N", shown on the opening screen's bottom right (NULL: none)

    // keyboard: the host pushes Apple key codes (bit 7 set) as they arrive
    uint8_t keyq[KEYQ];
    int kq_head, kq_tail;
    uint8_t kbd;                // $C000 latch (last key, bit 7 = strobe)

    unsigned long long cycles;  // 6502 cycles the original would have spent (the emulated clock)
    unsigned long long frame_at;// cycle count at the last presented frame
    void (*idle)(void *ctx);    // present a frame, pump input, pace to real time (host)
    int waiting;                // inside mach_key_wait: the game is blocked on a key
    void *idle_ctx;

    // speaker: the cycle time of every click ($C030), consumed by the host's audio
    unsigned long long spk[SPKQ];
    int spk_head, spk_tail;
    unsigned long clicks;       // total clicks (tests)
    // file loader for & RFL / & DUN / & UIM sources (host: bundled assets)
    int (*load)(void *ctx, const char *name, uint8_t *buf, int max);
    void *load_ctx;

    TextState text;
    GfxState gfx;
    TuneState tune;
};

void mach_init(Mach *m);
uint16_t hires_row(int y);                  // offset of row y within a page ($0000-$1FFF)

// keyboard ($C000 / $C010)
void mach_key_push(Mach *m, uint8_t key);   // host: a key was typed (code | $80)
int  mach_key_peek(Mach *m);                // latched key with bit 7 if unread
void mach_key_clear(Mach *m);               // BIT $C010
int  mach_key_wait(Mach *m);                // block (idling) until a key; returns it, strobe cleared

// the emulated clock: spend cycles, presenting a frame each 1/60 s of them
void mach_advance(Mach *m, unsigned long long cycles);
void mach_click(Mach *m);                   // toggle the speaker now

// The BASIC's hi-res graphics (gfx.c)
void hgr2(Mach *m);                         // HGR2: clear page 2, show it
void hcolor(Mach *m, int c);                // HCOLOR=
void hplot(Mach *m, int x, int y);          // HPLOT x,y
void hplot_to(Mach *m, int x, int y);       // HPLOT TO x,y (from the last point)

// USR(n): the extension's keyboard helpers
int usr(Mach *m, int n);

// gfx.c: the address registered by & DFT n (the original keeps it at $0310 + 2n)
uint16_t gfx_set_addr(const Mach *m, int n);

// & PT [n [,abort]] and & PN [n] (tune.c); tune buffer = image set 15
void tune_note(Mach *m, int pitch, int dur);
void amp_pt(Mach *m, int n, int abort_on_key);
void amp_pn(Mach *m, int n);

#endif
