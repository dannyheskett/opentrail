// The game's screens: the display half of the original BASIC programs, ported
// line by line onto the emulated machine (mach.h). The engine (game.h) runs
// the rules and asks for each screen through its io() callback; screens.c
// answers every Req by running the matching BASIC display code here.
//
// Comments cite program:line, as in the engine. BASIC variables that only the
// display code used live in Shell under their BASIC names.
#ifndef OT_SCREEN_H
#define OT_SCREEN_H

#include "game.h"
#include "mach.h"
#include "text.h"
#include "gfx.h"
#include "data.h"
#include "a2str.h"

#define PEEK(g, a) ((g)->mem[a])     // the handoff area (spec section 30)

// Control characters the programs PRINT (COMMON.LIB:40005).
#define CC "\x03"       // CC$: centre the line
#define CE "\x05"       // CE$
#define CF "\x06"       // CF$
#define CL "\x0C"       // CL$: clear the window
#define CR "\r"

typedef struct {
    Mach *m;
    Game *g;
    Persist *persist;
    int common;                 // which COMMON.LIB the running program appended: 0 side 1, 1 side 2 (FLOAT, WIN)
    int pa, sa, st;             // PA save-under buffer, SA sprite set, ST shift tables
    int x1, y1, x2, y2;         // the dialog box of COMMON.LIB:41000
    int goff;                   // G: extra row offset for event boxes (700)
    int y5;                     // Y5: travel-screen strip row (111)
    int wg, ix, fx, ni;         // travel strip: wagon frame, icon position, strip end, icon (3000-3246)
    int ia[5];                  // IA(): unpacked wagon frames (312)
    int sr_shown;               // status fields drawn
    int tune_loaded;            // a landmark tune is in the buffer at 51944
    int jf;                     // JF: at the main menu prompt
    int cc;                     // CC: the ground colour 310 last used
    int raft_landed;            // WIN follows FLOAT (the picture is already up)
    char td[24];                // TD$, as GOSUB 250 last built it (CROSS.LIB prints it stale)
    RndFn rnd; void *rnd_state; // the engine's RND for a new game (NULL: seeded from the clock)
} Shell;

// ---- COMMON.LIB ----
void sh_power_on(Mach *m);                                  // mach_init + the extension's own init
void sh_print(Shell *sh, const char *s);                    // PRINT s (s carries its own CR)
void sh_printf(Shell *sh, const char *fmt, ...);
void sh_space(Shell *sh);                                   // 30000: & SPACE : & QFH
int  sh_yesno(Shell *sh);                                   // 30100-30199: 'Y' or 'N'
void sh_box(Shell *sh, int x1, int y1, int x2, int y2);     // 41000: framed dialog, window 7
void sh_box_done(Shell *sh);                                // 41100: clear the bottom line, 30000
void sh_restore(Shell *sh);                                 // & PUT,X1,Y1,PA after a box
void sh_space_tune(Shell *sh);                              // OREGON TRAIL / WIN 950
void sh_loading(Shell *sh);                                 // hold a "loading" message
int  sh_inp(Shell *sh, int len, const char *ranges, int flag, char *out, int outlen);   // & INP

// ---- programs ----
void menu_program(Shell *sh);                               // MENU (side 1), never returns
int  screens_io(void *ctx, Game *g, Req *r);                // the engine's io() callback

#endif
