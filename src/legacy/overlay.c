// The quit dialog (Esc in legacy mode). Not the original's: the 1985 game had
// no way out but the power switch. It is drawn as COMMON.LIB:41000 draws a
// message box, with the original's font, on a scratch copy of the machine so
// the game's memory, windows and clock are untouched.
#include "overlay.h"
#include <stddef.h>

static Mach s_m;

// 41000-41005: a box with a triple border and window 7 inside it.
static void box(Mach *m, int x1, int y1, int x2, int y2) {
    hcolor(m, 3);
    x1 = x1 / 7 * 7;                                    // FN X
    x2 = x2 / 7 * 7 + 6;
    amp_box(m, x1, y1, x2, y2, ARG_NONE);
    for (int l = 1; l <= 3; l++) {
        hplot(m, x1 + l, y1 + 4 - l);
        hplot_to(m, x2 - l, y1 + 4 - l);
        hplot_to(m, x2 - l, y2 - 4 + l);
        hplot_to(m, x1 + l, y2 - 4 + l);
        hplot_to(m, x1 + l, y1 + 4 - l);
    }
    amp_dfw(m, 7, x1 + 8, y1 + 6, x2 - 11 - x1, y2 - 6 - y1);
    amp_wind(m, 7);
}

const uint8_t *overlay_quit(const Mach *m) {
    Mach *s = &s_m;
    *s = *m;
    s->idle = NULL;                                     // no frames, no input
    s->text.on_escape = NULL;
    if (s->load) s->load(s->load_ctx, "FONT", s->ram + 0x6BEB, 0x7024 - 0x6BEB);   // as MENU sets it
    s->hpag = m->page2 ? 0x40 : 0x20;                   // draw on the page shown
    s->text.noparse = 0;
    s->text.bn = 0;                                     // whatever the game had buffered stays unprinted
    amp_hook(s);
    amp_hsp(s, 2, 0);
    s->invflg = 0xFF;                                   // NORMAL
    box(s, 56, 76, 223, 104);
    amp_print(s, "Do you want to quit\r");
    amp_print(s, "the game? (Y/N)");
    text_flush(s);
    return s->ram + (s->hpag << 8);
}
