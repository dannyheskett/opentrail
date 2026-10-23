// MENU (side 1) and MANAGEMENT: the main menu, the Top Ten, "Learn about the
// trail", the sound toggle, the management options, and the new-game screens
// the engine asks for (occupation and names, MENU:4000-6045).
#include "screen.h"
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const SB[2] = { "off", "on" };            // SB$

// MENU:29010, the points tables of "how points are earned".
static const char *const PTS_HEALTH[4][2] = {
    { "good", "500" }, { "fair", "400" }, { "poor", "300" }, { "very poor", "200" } };
static const char *const PTS_ITEMS[7][2] = {
    { "wagon", "50" }, { "ox", "4" }, { "spare wagon part", "2" }, { "set of clothing", "2" },
    { "bullets (each 50)", "1" }, { "food (each 25 pounds)", "1" }, { "cash (each 5 dollars)", "1" } };

// MENU:2005-2015 / WIN:700-705 / MANAGEMENT:900-915: the Top Ten table rows.
static void topten_rows(Shell *sh, TopTen hi) {
    Mach *m = sh->m;
    for (int l = 0; l <= 9; l++) {
        int zx, zy;
        amp_gcp(m, &zx, &zy);
        for (int l1 = 0; l1 <= 2; l1++) {
            char z[OT_NAME_MAX + 4];
            snprintf(z, sizeof z, "%s", hi[l][l1]);
            if (l1 == 1) {                                   // RIGHT$("  " + Z$, 4)
                char t[OT_NAME_MAX + 4];
                snprintf(t, sizeof t, "  %s", hi[l][l1]);
                int n = (int)strlen(t);
                snprintf(z, sizeof z, "%s", t + (n > 4 ? n - 4 : 0));
            }
            amp_co(m, 139 * (l1 == 1) + 189 * (l1 == 2), zy);
            sh_printf(sh, "%s" CR, z);
        }
    }
}

// MENU:2200-2235: how points are earned.
static void points_help(Shell *sh) {
    Mach *m = sh->m;
    sh_print(sh, CL CR);
    amp_wind(m, ARG_NONE);
    amp_image(m, 6, 45, 0);
    sh_print(sh, CR);
    int zy = 20;
    amp_dfw(m, 7, 0, zy, 279, 191 - zy);
    amp_wind(m, 7);
    sh_print(sh, "Your most important resource is the people you have with you.  You receive "
                 "points for each member of your party who arrives safely; you receive more "
                 "points if they arrive in good health!" CR CR);
    amp_in(m, 10);
    m->invflg = 0x3F;                                        // INVERSE
    amp_box(m, 6, 61, 81, 81, 3);
    amp_box(m, 146, 61, 229, 81, 3);
    sh_print(sh, "Health of");  amp_co(m, 140, ARG_NONE); sh_print(sh, "Points per" CR);
    sh_print(sh, "  Party");    amp_co(m, 140, ARG_NONE); sh_print(sh, "  Person" CR);
    m->invflg = 0xFF;
    sh_print(sh, CR);
    for (int l = 0; l <= 3; l++) {
        sh_print(sh, PTS_HEALTH[l][0]);
        amp_co(m, 166, ARG_NONE);
        sh_printf(sh, "%s" CR, PTS_HEALTH[l][1]);
    }
    amp_in(m, ARG_NONE);
    sh_space(sh);
    amp_box(m, ARG_NONE, ARG_NONE, ARG_NONE, ARG_NONE, ARG_NONE);
    amp_co(m, ARG_NONE, ARG_NONE);
    sh_print(sh, "The resources you arrive with will help you get started in the new land.  "
                 "You receive points for each item you bring safely to Oregon." CR CR);
    amp_in(m, 10);
    m->invflg = 0x3F;
    amp_box(m, 6, 43, 107, 63, 3);
    amp_box(m, 166, 43, 249, 63, 3);
    sh_print(sh, "Resources of"); amp_co(m, 160, ARG_NONE); sh_print(sh, "Points per" CR);
    sh_print(sh, "   Party");     amp_co(m, 160, ARG_NONE); sh_print(sh, "   Item" CR);
    m->invflg = 0xFF;
    sh_print(sh, CR);
    for (int l = 0; l <= 6; l++) {
        sh_print(sh, PTS_ITEMS[l][0]);
        amp_co(m, 190, ARG_NONE);
        sh_printf(sh, "%2s" CR, PTS_ITEMS[l][1]);           // RIGHT$(" " + Z$, 2)
    }
    amp_in(m, ARG_NONE);
    sh_space(sh);
    amp_box(m, ARG_NONE, ARG_NONE, ARG_NONE, ARG_NONE, ARG_NONE);
    amp_co(m, ARG_NONE, 50);
    amp_image(m, 3, 10, 30);
    amp_image(m, 3, 10, 160);
    amp_dfw(m, 2, 21, 60, 255, 191);
    amp_wind(m, 2);
    sh_print(sh, "You receive points for your occupation in the new land.  Because more farmers "
                 "and carpenters were needed than bankers, you receive double points upon "
                 "arriving in Oregon as a carpenter, and triple points for arriving as a farmer." CR);
    amp_wind(m, ARG_NONE);
    sh_space(sh);
}

// MENU:2000-2020: the Oregon Top Ten.
static void top_ten(Shell *sh) {
    Mach *m = sh->m;
    TopTen hi;
    topten_read(sh->persist, hi);
    sh_print(sh, CL CR);
    amp_wind(m, ARG_NONE);
    amp_image(m, 5, 55, 0);
    int zy = 20;
    m->invflg = 0x3F;
    amp_box(m, 30, 18, 69, 29, 3);   amp_co(m, 34, zy);  sh_print(sh, "Name" CR);
    amp_box(m, 126, 18, 177, 29, 3); amp_co(m, 130, zy); sh_print(sh, "Points" CR);
    amp_box(m, 204, 18, 255, 29, 3); amp_co(m, 208, zy); sh_print(sh, "Rating" CR);
    m->invflg = 0xFF;
    sh_print(sh, CR);
    topten_rows(sh, hi);
    sh_print(sh, CR);
    int zx;
    amp_gcp(m, &zx, &zy);
    sh_box(sh, 20, zy, 240, zy + 30);
    sh_print(sh, "Would you like to see how points are earned? ");
    int yes = sh_yesno(sh) == 'Y';
    amp_wind(m, ARG_NONE);
    if (yes) points_help(sh);
}

// MENU:2300: sound on/off.
static void toggle_sound(Shell *sh) {
    Mach *m = sh->m;
    amp_wind(m, ARG_NONE);
    amp_box(m, 0, 63, 279, 160, ARG_NONE);
    amp_dfw(m, 7, 20, 90, 240, 70);
    amp_wind(m, 7);
    m->sound = !m->sound;
    sh_printf(sh, "The sound is now turned %s." CR CR, SB[m->sound]);
    sh_print(sh, "You may turn the sound on or off during the program by pressing Control-S." CR);
    sh_space(sh);
}

// MENU:7000-7500: Learn about the trail.
static void learn(Shell *sh) {
    Mach *m = sh->m;
    amp_wind(m, ARG_NONE);
    amp_box(m, 0, 63, 279, 160, ARG_NONE);
    amp_dfw(m, 7, 20, 70, 240, 90);
    amp_wind(m, 7);
    static const char *const pages[] = {
        "Try taking a journey by covered wagon across 2000 miles of plains, rivers, and "
        "mountains.  Try!  On the plains, will you slosh your oxen through mud and "
        "water-filled ruts or will you plod through dust six inches deep?",
        "How will you cross the rivers?  If you have money, you might take a ferry (if there "
        "is a ferry).  Or, you can ford the river and hope you and your wagon aren't "
        "swallowed alive!",
        "What about supplies?  Well, if you're low on food you can hunt.  You might get a "
        "buffalo...you might.  And there are bear in the mountains.",
        "At the Dalles, you can try navigating the Columbia River, but if running the rapids "
        "with a makeshift raft makes you queasy, better take the Barlow Road.",
        "If for some reason you don't survive -- your wagon burns, or thieves steal your oxen, "
        "or you run out of provisions, or you die of cholera -- don't give up!  Try "
        "again...and again...until your name is up with the others on The Oregon Top Ten.",
    };
    for (int i = 0; i < 5; i++) {
        if (i) { amp_co(m, ARG_NONE, ARG_NONE); sh_print(sh, CF CR); }
        sh_printf(sh, "%s" CR, pages[i]);
        sh_space(sh);
    }
    amp_co(m, ARG_NONE, ARG_NONE);
    sh_print(sh, CF CR);
    // 7400-7410: the colour bars
    static const char *const br[4] = { "Orange", "Green", "Blue", "Violet" };
    sh_print(sh, "You may use these colors to" CC CR);
    sh_print(sh, "adjust your monitor or" CC CR);
    sh_print(sh, "television." CC CR CR);
    int zx, zy;
    amp_gcp(m, &zx, &zy);
    amp_wind(m, ARG_NONE);
    zx += 20; zy += 70;
    for (int l = 0; l <= 3; l++) {
        amp_wind(m, ARG_NONE);
        amp_box(m, zx + 60 * l, zy, zx + 50 + 60 * l, zy + 30,
                5 * (l == 0) + 1 * (l == 1) + 6 * (l == 2) + 2 * (l == 3));
        amp_dfw(m, 6, zx + 60 * l, zy + 32, 50, 10);
        amp_wind(m, 6);
        sh_printf(sh, "%s" CC CR, br[l]);
    }
    amp_wind(m, 7);
    sh_space(sh);
    amp_co(m, ARG_NONE, ARG_NONE);                               // 7500
    sh_print(sh, CF CR CR "Control-S key" CC CR CR);
    sh_print(sh, "You may turn the sound on or off during the program by pressing Control-S." CR);
    sh_space(sh);
    amp_box(m, ARG_NONE, ARG_NONE, ARG_NONE, ARG_NONE, ARG_NONE);
}

// ---------------------------------------------------------------------------
// MANAGEMENT
// ---------------------------------------------------------------------------
static const char *const MGMT[5] = { "See the current Top Ten list", "See the original Top Ten list",
    "Erase the current Top Ten list", "Erase the tombstone messages", "Return to main menu" };

static void mgmt_list(Shell *sh, int original) {                 // 900-915
    Mach *m = sh->m;
    TopTen hi;
    if (original) {
        Persist p;
        persist_reset_topten(&p);
        topten_read(&p, hi);
    } else topten_read(sh->persist, hi);
    sh_print(sh, CL CR);
    amp_wind(m, 0);
    sh_printf(sh, "%s Top Ten list" CC CR CR CR CR, original ? "Original" : "Current");
    int zx, zy;
    amp_gcp(m, &zx, &zy);
    amp_co(m, 10, zy);  sh_print(sh, "Name" CR);
    amp_co(m, 130, zy); sh_print(sh, "Points" CR);
    amp_co(m, 210, zy); sh_print(sh, "Rating" CR);
    sh_print(sh, CR);
    topten_rows(sh, hi);
    sh_space(sh);
}

static int mgmt_confirm(Shell *sh, const char *text) {
    sh_print(sh, CL CR);
    amp_wind(sh->m, 7);
    sh_printf(sh, "%s" CR CR "Do you want to do this? ", text);
    return sh_yesno(sh) == 'Y';
}

static void management(Shell *sh) {
    Mach *m = sh->m;
    for (;;) {                                                   // 1000-1015
        sh_print(sh, CL CR);
        amp_wind(m, 0);
        sh_print(sh, "The Oregon Trail" CC CR "Version 1.4" CC CR CR);
        hcolor(m, 6);
        hplot(m, 0, 20); hplot_to(m, 279, 20);
        sh_print(sh, "Management Options" CC CR);
        amp_dfw(m, 7, 0, 60, 279, 151);
        amp_wind(m, 7);
        sh_print(sh, "You may:" CR);
        amp_in(m, 20);
        sh_print(sh, CR);
        for (int l = 1; l <= 5; l++) sh_printf(sh, "%d. %s" CR, l, MGMT[l - 1]);
        sh_print(sh, CR);
        amp_in(m, ARG_NONE);
        sh_print(sh, "What is your choice? ");
        char z[4];
        sh_inp(sh, 1, "-15", 1, z, sizeof z);
        amp_wind(m, 0);
        int c = atoi(z);
        if (c == 1 || c == 2) mgmt_list(sh, c == 2);
        else if (c == 3) {
            if (mgmt_confirm(sh, "If you erase the current Top Ten list, the names and scores "
                                 "will be replaced by those on the original list.")) {
                amp_wind(m, ARG_NONE);
                sh_print(sh, CL CR);
                amp_co(m, 0, 89);
                sh_print(sh, "One moment please..." CR);
                sh_loading(sh);
                persist_reset_topten(sh->persist);
                screens_io(sh, sh->g, &(Req){ .kind = REQ_SAVE, .a = 1 });
            }
        } else if (c == 4) {
            if (mgmt_confirm(sh, "There may be a tombstone message on each side of the diskette.  "
                                 "If you erase these messages, they will not be replaced until "
                                 "team leaders die along the trail.")) {
                amp_wind(m, ARG_NONE);
                sh_print(sh, CL CR);
                for (int side = 0; side < 2; side++) {                   // 250
                    sh_print(sh, CL CR);
                    sh_print(sh, CL CR);
                    amp_co(m, 0, 89);
                    sh_printf(sh, "Removing tombstones from side %s..." CR, side ? "two" : "one");
                }
                persist_erase_tombs(sh->persist);
                screens_io(sh, sh->g, &(Req){ .kind = REQ_SAVE, .a = 0 });
                sh_print(sh, CL CR);
            }
        } else if (c == 5) {                                     // 31000
            amp_wind(m, ARG_NONE);
            sh_print(sh, CL CR);
            amp_co(m, ARG_NONE, 89);
            sh_print(sh, "Getting the Oregon Trail menu..." CR);
            sh_loading(sh);
            return;
        }
    }
}

// ---------------------------------------------------------------------------
// New game (asked by the engine)
// ---------------------------------------------------------------------------
// MENU:4000-4110
int menu_occupation(Shell *sh) {
    Mach *m = sh->m;
    sh_print(sh, CL CR);
    for (;;) {
        amp_image(m, 3, 10, 5);                                  // 4005
        amp_dfw(m, 7, 10, 35, 240, 124);
        amp_wind(m, 7);
        sh_print(sh, "Many kinds of people made the trip to Oregon." CR CR "You may:" CR CR);
        amp_in(m, 10);
        sh_print(sh, "1. Be a banker from Boston" CR);
        sh_print(sh, "2. Be a carpenter from Ohio" CR "3. Be a farmer from Illinois" CR
                     "4. Find out the differences" CR);
        amp_in(m, 30);
        sh_print(sh, "between these choices" CR CR);
        amp_image(m, 3, 10, 160);
        amp_in(m, ARG_NONE);
        sh_print(sh, "What is your choice? ");
        char z[4];
        sh_inp(sh, 1, "-14", 1, z, sizeof z);
        int c = atoi(z);
        if (c != 4) return c;
        amp_box(m, ARG_NONE, ARG_NONE, ARG_NONE, ARG_NONE, ARG_NONE);     // 4100
        amp_dfw(m, 7, 20, 35, 240, 124);
        amp_wind(m, 7);
        sh_print(sh, "Traveling to Oregon isn't easy!  But if you're a banker, you'll have more "
                     "money for supplies and services than a carpenter or a farmer." CR CR);
        sh_print(sh, "However, the harder you have to try, the more points you deserve!  "
                     "Therefore, the farmer earns the greatest number of points and the banker "
                     "earns the least." CR);
        sh_space(sh);
        amp_box(m, ARG_NONE, ARG_NONE, ARG_NONE, ARG_NONE, ARG_NONE);
    }
}

// MENU:500: & INP,ZN,"-AZ-az '.-",ZZ,Z$
static int name_inp(Shell *sh, int zz, char *out) {
    return sh_inp(sh, 9, "-AZ-az '.-", zz, out, OT_NAME_MAX);
}

// MENU:6015-6042: the leader, then four more (Return fills in the defaults).
void menu_names(Shell *sh, char n[5][OT_NAME_MAX]) {
    Mach *m = sh->m;
    int zpos[5][2];
    amp_dfw(m, 7, 0, 102, 279, 89);
    amp_wind(m, 7);
    sh_print(sh, CL CR);
    amp_image(m, 1, 7, 0);
    amp_co(m, ARG_NONE, ARG_NONE);
    sh_print(sh, "What is the first name of the wagon leader? ");
    name_inp(sh, 1, n[0]);
    amp_co(m, ARG_NONE, ARG_NONE);                               // 6020
    sh_print(sh, CF CR);
    amp_co(m, ARG_NONE, ARG_NONE);
    sh_print(sh, "What are the first names of the four other members in your party? " CR CR);
    int zx, y, zy;
    amp_gcp(m, &zx, &y);
    amp_co(m, ARG_NONE, 81);
    sh_print(sh, "(Enter names or press Return)" CC CR);
    amp_co(m, zx, y);                                            // 6021
    for (int l = 0; l <= 4; l++) {
        sh_printf(sh, "%d. ", l + 1);
        amp_gcp(m, &zx, &zy);
        sh_print(sh, CR);
        zpos[l][0] = zx; zpos[l][1] = zy;
    }
    sh_print(sh, CR);
    amp_co(m, zpos[0][0], zpos[0][1]);                           // 6030
    sh_printf(sh, "%s" CR, n[0]);
    for (int l = 1; l <= 4; l++) {
        char z[OT_NAME_MAX] = "";
        amp_co(m, zpos[l][0], zpos[l][1]);
        name_inp(sh, 0, z);
        if (!z[0]) {                                             // Return: defaults from here on
            for (; l <= 4; l++) { amp_co(m, zpos[l][0], zpos[l][1]); sh_printf(sh, "%s" CR, n[l]); }
            break;
        }
        snprintf(n[l], OT_NAME_MAX, "%s", z);                    // 6035
    }
    for (;;) {                                                   // 6040
        amp_co(m, ARG_NONE, 81);
        amp_cel(m);
        sh_print(sh, "Are these names correct? ");
        if (sh_yesno(sh) == 'Y') return;
        amp_co(m, ARG_NONE, 81);                                 // 6041
        amp_cel(m);
        sh_print(sh, "Change which name? ");
        char c[4];
        sh_inp(sh, 1, "-15", 1, c, sizeof c);
        int z = atoi(c) - 1;
        amp_co(m, zpos[z][0], zpos[z][1]);
        amp_cel(m);
        char nm[OT_NAME_MAX] = "";
        name_inp(sh, 0, nm);
        if (!nm[0]) {
            amp_co(m, zpos[z][0], zpos[z][1]);
            sh_printf(sh, "%s" CR, n[z]);
            snprintf(nm, OT_NAME_MAX, "%s", n[z]);
        }
        snprintf(n[z], OT_NAME_MAX, "%s", nm);                   // 6042
    }
}

// MENU:9000 after the names: "Going back to 1848..."
void menu_going_back(Shell *sh) {
    Mach *m = sh->m;
    amp_wind(m, ARG_NONE);
    amp_dun(m, "L0.PCK", sh->pa, 16);
    amp_cst(m, sh->st);
    sh_box(sh, 49, 160, 49 + 170, 180);
    sh_print(sh, "Going back to 1848..." CR);
    sh_loading(sh);
}

// ---------------------------------------------------------------------------
// Boot: the MECC title, unpacked to page 1 by the boot loader ($7400) while
// the rest of the disk loads.
// ---------------------------------------------------------------------------
// The title stays up for 2 s, or until a key (which is not passed on). The
// original showed it for 12 s while the rest of the game loaded from disk.
#define TITLE_FRAMES 120

static void boot_title(Shell *sh) {
    Mach *m = sh->m;
    if (m->load) m->load(m->load_ctx, "FONT", m->ram + 0x6BEB, 0x7024 - 0x6BEB);   // part of the extension
    static uint8_t rle[4096];
    int n = m->load ? m->load(m->load_ctx, "TITLE.RLE", rle, sizeof rle) : -1;
    int row = 0, col = 0;
    #define PUT(v) do { m->ram[0x2000 + hires_row(row) + col] = (v); if (++row == 192) { row = 0; col++; } } while (0)
    for (int p = 0; p < n && col < 40; ) {
        int c = rle[p++];
        if (c == 0xC0) break;
        int k = (c & 0x3F) + 1;
        if (c < 0x40) { for (int i = 0; i < k && p < n; i++) PUT(rle[p++]); }
        else if (c < 0x80) { int v = rle[p++]; for (int i = 0; i < k; i++) PUT(v); }
        else { int a = rle[p], b = rle[p + 1]; p += 2; for (int i = 0; i < k; i++) { PUT(a); PUT(b); } }
    }
    #undef PUT
    m->page2 = 0;
    if (m->version) {                            // the version in the title's bottom right corner, as the
        int hp = m->hpag, w = 7 * (int)strlen(m->version);   // family's games show it (6 in, 4 up)
        m->hpag = 0x20;                          // on the title's page, in the Apple's own text on its
        amp_dfw(m, 6, 0, 0, 280, 192);           // own black cells (white on the green band fringes into colour)
        amp_wind(m, 6);
        amp_box(m, (280 - 8 - w) / 7 * 7, 180, 279, 189, 4);   // in the green band (180-191), on whole bytes
        amp_dfw(m, 7, 280 - 6 - w, 192 - 4 - 8, w, 9);
        amp_wind(m, 7);
        amp_print(m, m->version);
        m->hpag = hp;
        amp_wind(m, 0);
    }
    for (int f = 0; f < TITLE_FRAMES && !(mach_key_peek(m) & 0x80); f++)
        mach_advance(m, FRAME_CYCLES);
    mach_key_clear(m);
    m->page2 = 1;
}

// ---------------------------------------------------------------------------
// MENU:100-1020
// ---------------------------------------------------------------------------
static void menu_start(Shell *sh) {
    Mach *m = sh->m;
    sh->common = 0; sh->pa = 12288; sh->st = 14848; sh->sa = 37380;
    amp_rfl(m, "FIRST.IMA", 49152);                              // 110
    amp_rfl(m, "OREGON.IMA", 37380);
    amp_cst(m, sh->st);
    if (m->sound == 255) m->sound = 1;                           // 130
    amp_hook(m);                                                 // 140
    sh_print(sh, CL CR);
    amp_hsp(m, 2, 0);
    amp_dft(m, 7, 49152);
    amp_dft(m, 15, 51944);
    amp_wind(m, ARG_NONE);
}

// Esc Esc (COMMON.LIB:31000): back to the menu. During a game that unwinds the
// game; at the main menu prompt it is MENU:5000, "Insert another diskette."
static Shell *s_sh;
static int s_in_game;
static jmp_buf s_reboot;

static void on_escape(Mach *m) {
    (void)m;
    if (s_in_game) game_abandon(s_sh->g);
    if (s_sh->jf) longjmp(s_reboot, 1);
}

void menu_program(Shell *sh) {
    Mach *m = sh->m;
    s_sh = sh;
    m->text.on_escape = on_escape;
    if (setjmp(s_reboot)) {                                      // MENU:5000
        amp_wind(m, ARG_NONE);
        sh_print(sh, CL CR);
        amp_co(m, 0, 89);
        sh_print(sh, "Insert another diskette." CC CR);
        sh_space(sh);
        sh_print(sh, CL CR);
    }
    boot_title(sh);
    menu_start(sh);
    for (;;) {
        amp_box(m, ARG_NONE, ARG_NONE, ARG_NONE, ARG_NONE, ARG_NONE);    // 1000
        amp_image(m, 2, 1, 10);
        amp_image(m, 3, 8, 40);
        sh->jf = 1;                                              // 1005
        amp_dfw(m, 7, 32, 63, 245, 188);
        amp_wind(m, 7);
        amp_vsp(m, 3, ARG_NONE);
        sh_print(sh, "You may:" CR CR);
        sh_print(sh, "  1. Travel the trail" CR "  2. Learn about the trail" CR
                     "  3. See the Oregon Top Ten" CR);
        sh_printf(sh, "  4. Turn sound %s" CR, SB[!m->sound]);
        sh_print(sh, CR);                                        // 1015
        amp_image(m, 3, 8, 164);
        sh_print(sh, "What is your choice? ");
        char z[4];
        sh_inp(sh, 1, "\x01-14", 1, z, sizeof z);
        sh->jf = 0;
        int a = (unsigned char)z[0];
        amp_vsp(m, ARG_NONE, ARG_NONE);
        srand((unsigned)m->cycles);                              // RND(-(PEEK(78)+PEEK(79)*256))
        if (a == 1) {
            sh_print(sh, CL CR);
            amp_wind(m, ARG_NONE);
            amp_co(m, 0, 89);
            sh_print(sh, "Getting Management Options..." CR);
            sh_loading(sh);
            management(sh);
            menu_start(sh);                                      // & RNH,"MENU"
            continue;
        }
        switch (a - 48) {
        case 1: {                                                // 9000: a new game
            Game *g = sh->g;
            game_init(g, screens_io, sh, sh->rnd, sh->rnd_state,
                      (unsigned long long)m->cycles * 2654435761u + 1, sh->persist);
            s_in_game = 1;
            Outcome o = game_play(g);
            s_in_game = 0;
            // Back to the menu. A party that dies on side 1 goes straight to
            // MENU; everything else passes through COMMON.LIB:31000 or side 2's
            // MENU:31000, which say so (FLOAT:760 first says "One moment please...").
            if (!(o == OUTCOME_DIED && g->s == 0)) {
                if (o == OUTCOME_DIED && g->lm == 16) {
                    amp_wind(m, ARG_NONE);
                    sh_print(sh, CL CR);
                    amp_co(m, 0, 89);
                    sh_print(sh, "One moment please..." CR);
                    sh_loading(sh);
                }
                m->invflg = 0xFF;
                amp_wind(m, ARG_NONE);
                sh_print(sh, CL CR);
                amp_co(m, ARG_NONE, 89);
                sh_print(sh, "Getting the Oregon Trail menu..." CR);
                sh_loading(sh);
            }
            menu_start(sh);                                      // & RNH,"MENU"
            break;
        }
        case 2: learn(sh); break;
        case 3: top_ten(sh); break;
        case 4: toggle_sound(sh); break;
        }
    }
}
