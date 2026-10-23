// COMMON.LIB: the helpers every program appends (side 1 and side 2 copies).
#include "screen.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

void sh_print(Shell *sh, const char *s) { amp_print(sh->m, s); }

void sh_printf(Shell *sh, const char *fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    amp_print(sh->m, buf);
}

// 30000: & SPACE : & QFH
void sh_space(Shell *sh) { amp_space(sh->m, ARG_NONE); }

// 30100-30199: & GCP,ZX,ZY: PRINT ... & CO,ZX,ZY: & CEL: & INP,3,"YESNOyesno",1,Z$
// until the first letter is Y or N (either case). Side 2's copy has no PRINT.
int sh_yesno(Shell *sh) {
    int zx, zy;
    amp_gcp(sh->m, &zx, &zy);
    if (!sh->common) sh_print(sh, CR);
    for (;;) {
        char z[8];
        amp_co(sh->m, zx, zy);
        amp_cel(sh->m);
        sh_inp(sh, 3, "YESNOyesno", 1, z, sizeof z);
        int c = (unsigned char)z[0];
        if (c > 96) c -= 32;
        if (c == 'Y' || c == 'N') return c;
    }
}

// 41000-41005: a box with a triple border, its background saved to PA, and
// window 7 inside it. Side 2's copy neither selects the full window nor sets
// NORMAL first, and makes the window 5 pixels narrower.
void sh_box(Shell *sh, int x1, int y1, int x2, int y2) {
    Mach *m = sh->m;
    if (!sh->common) {
        amp_wind(m, ARG_NONE);
        m->invflg = 0xFF;                               // NORMAL
    }
    hcolor(m, 3);
    x1 = x1 / 7 * 7;                                    // FN X
    x2 = x2 / 7 * 7 + 6;
    sh->x1 = x1; sh->y1 = y1; sh->x2 = x2; sh->y2 = y2;
    amp_take(m, x1, y1, x2, y2, sh->pa);
    amp_box(m, x1, y1, x2, y2, ARG_NONE);
    for (int l = 1; l <= 3; l++) {
        hplot(m, x1 + l, y1 + 4 - l);
        hplot_to(m, x2 - l, y1 + 4 - l);
        hplot_to(m, x2 - l, y2 - 4 + l);
        hplot_to(m, x1 + l, y2 - 4 + l);
        hplot_to(m, x1 + l, y1 + 4 - l);
    }
    int narrow = sh->common ? 16 : 11;
    amp_dfw(m, 7, x1 + 8, y1 + 6, x2 - narrow - x1, y2 - 6 - y1);
    amp_wind(m, 7);
}

// 41100: & WIND: & BOX,0,182,279,191: GOSUB 30000
void sh_box_done(Shell *sh) {
    amp_wind(sh->m, ARG_NONE);
    amp_box(sh->m, 0, 182, 279, 191, ARG_NONE);
    sh_space(sh);
}

void sh_restore(Shell *sh) { amp_put(sh->m, sh->x1, sh->y1, sh->pa); }

// OREGON TRAIL / WIN 950: "Press SPACE BAR to continue" with the tune playing
// over and over; Ctrl-S toggles the sound. OREGON TRAIL's has & CO,,182,
// WIN's & CO,0,182 and no & BOX first.
void sh_space_tune(Shell *sh) {
    Mach *m = sh->m;
    amp_wind(m, ARG_NONE);
    if (!sh->common) amp_box(m, 0, 180, 279, 191, ARG_NONE);  // WIN's 950 has no box
    amp_co(m, sh->common ? 0 : ARG_NONE, 182);
    sh_print(sh, "Press SPACE BAR to continue" CC CR);
    usr(m, 1);
    for (;;) {
        m->waiting = 1;                                 // blocked on a key (the harness's wait)
        if (m->sound) amp_pt(m, 1, 1);                  // 955
        int z = usr(m, 2);                              // 956
        m->waiting = 0;
        if (z < 128) continue;
        if (z == 147) { m->sound = !m->sound; usr(m, 1); continue; }   // 957: Ctrl-S
        break;
    }
    amp_box(m, 0, 180, 279, 191, ARG_NONE);             // 958
}

// A message the original showed while it loaded from disk ("Going back to
// 1848...", "One moment please..."): loading is instant here, so the message
// stays up long enough to read.
void sh_loading(Shell *sh) { mach_advance(sh->m, A2_HZ * 3 / 2); }

int sh_inp(Shell *sh, int len, const char *ranges, int flag, char *out, int outlen) {
    return amp_inp(sh->m, len, ranges, flag, out, outlen);
}

// Power-on: the machine with the extension loaded and initialised.
void sh_power_on(Mach *m) {
    mach_init(m);
    gfx_init(m);
    text_init(m);
}
