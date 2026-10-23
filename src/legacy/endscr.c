// RIVER.LIB / CROSS.LIB, END.LIB, FLOAT and WIN: the display side.
#include "screen.h"
#include "dalles.h"
#include "floatdraw.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const RIVER_OPT[6] = {                        // RIVER.LIB:51000
    "attempt to ford the river", "caulk wagon and float it across",
    "wait to see if conditions improve", "get more information",
    "hire an Indian to help", "take a ferry across" };

// OREGON TRAIL:250, TD$
static void td(Shell *sh) {
    const Game *g = sh->g;
    snprintf(sh->td, sizeof sh->td, "%s %d, %d", MONTH[g->am - 1], g->ad, g->ay);
}
static const char *lm_name(const Game *g) {
    const char *n = LM_NAME[g->lm];
    return strncmp(n, "the ", 4) ? n : n + 4;
}
// BASIC's empty FOR loops, timed on the original (about 1.2 ms a pass).
static void basic_delay(Shell *sh, int passes) { mach_advance(sh->m, (unsigned long long)passes * 1225); }

// ---------------------------------------------------------------------------
// RIVER.LIB
// ---------------------------------------------------------------------------
// OREGON TRAIL:3500 and RIVER.LIB:50000-50005
void river_intro(Shell *sh) {
    Mach *m = sh->m;
    Game *g = sh->g;
    char rw[16], rd[16];
    amp_rfl(m, "OREGON.IMA", sh->sa);
    amp_dft(m, 7, sh->sa);
    sh_print(sh, CL CR);
    amp_image(m, 10, 10, 160);
    amp_image(m, 10, 10, 30);
    td(sh);                                                      // 50000: GOSUB 250
    amp_wind(m, ARG_NONE);                                       // 50001
    sh_printf(sh, CR "%s" CC CR "%s" CC CR, lm_name(g), sh->td);
    amp_dfw(m, 7, 20, 60, 220, 119);
    amp_dfw(m, 6, 30, 70, 230, 89);
    amp_wind(m, 7);
    sh_printf(sh, CR "You must cross the river in order to continue.  The river at this point is "
              "currently %s feet across, and %s feet deep in the middle." CR,
              a2_str(g->rw, rw), a2_str(g->rd, rd));
    sh_space(sh);
}

// 50010-50015
int river_menu(Shell *sh, int extra) {
    Mach *m = sh->m;
    Game *g = sh->g;
    char a[16], b[16];
    amp_dfw(m, 7, 0, 48, 279, 111);
    amp_wind(m, 7);
    amp_box(m, ARG_NONE, ARG_NONE, ARG_NONE, ARG_NONE, ARG_NONE);
    amp_co(m, ARG_NONE, ARG_NONE);
    sh_printf(sh, "Weather: %s" CR "River width: %s feet" CR "River depth: %s feet" CR,
              WEATHER[g->w], a2_str(g->rw, a), a2_str(g->rd, b));
    int y;
    amp_gcp(m, NULL, &y);                                        // 50011
    amp_co(m, ARG_NONE, y + 4);
    sh_print(sh, "You may:" CR);
    amp_gcp(m, NULL, &y);
    amp_co(m, ARG_NONE, y + 4);
    int z = 0;
    for (int l = 0; l <= 1; l++) sh_printf(sh, "%d. %s" CR, ++z, RIVER_OPT[l]);
    if (extra) sh_printf(sh, "%d. %s" CR, ++z, RIVER_OPT[extra + 3]);
    for (int l = 2; l <= 3; l++) sh_printf(sh, "%d. %s" CR, ++z, RIVER_OPT[l]);   // 50015
    sh_print(sh, CR "What is your choice? ");
    char range[8], c[4];
    snprintf(range, sizeof range, "-1%d", z);
    sh_inp(sh, 1, range, 1, c, sizeof c);
    return atoi(c);
}

// 50020 / 50160-50170
void river_info(Shell *sh) {
    Mach *m = sh->m;
    amp_box(m, ARG_NONE, ARG_NONE, ARG_NONE, ARG_NONE, ARG_NONE);
    amp_wind(m, 6);
    amp_co(m, ARG_NONE, ARG_NONE);
    sh_print(sh, "To ford a river means to pull your wagon across a shallow part of the river, "
                 "with the oxen still attached." CR);
    sh_space(sh);
    amp_box(m, ARG_NONE, ARG_NONE, ARG_NONE, ARG_NONE, ARG_NONE);
    amp_co(m, ARG_NONE, ARG_NONE);
    sh_print(sh, "To caulk the wagon means to seal it so that no water can get in.  The wagon "
                 "can then be floated across like a boat." CR);
    sh_space(sh);
    amp_box(m, ARG_NONE, ARG_NONE, ARG_NONE, ARG_NONE, ARG_NONE);
    amp_co(m, ARG_NONE, ARG_NONE);
    sh_print(sh, "To use a ferry means to put your wagon on top of a flat boat that belongs to "
                 "someone else.  The owner of the ferry will take your wagon across the river." CR);
    sh_space(sh);
}

void river_camp(Shell *sh) {                                     // 50025
    Mach *m = sh->m;
    amp_box(m, ARG_NONE, ARG_NONE, ARG_NONE, ARG_NONE, ARG_NONE);
    amp_co(m, ARG_NONE, 50);
    sh_print(sh, "You camp near the river for a day." CC CR);
    sh_space(sh);
}

void river_camp_date(Shell *sh) {
    td(sh);                                                      // 50025: GOSUB 250
    amp_wind(sh->m, ARG_NONE);
    amp_co(sh->m, ARG_NONE, 18);
    amp_cel(sh->m);
    sh_printf(sh, "%s" CC CR, sh->td);
}

void river_msg(Shell *sh, int msg) {
    Mach *m = sh->m;
    switch (msg) {
    case RIVER_MSG_FLOAT_SHALLOW:                                // 50080
        amp_box(m, ARG_NONE, ARG_NONE, ARG_NONE, ARG_NONE, ARG_NONE);
        amp_co(m, 0, 50);
        sh_print(sh, "The river is too shallow" CC CR "to float across." CC CR);
        break;
    case RIVER_MSG_FERRY_SHALLOW:                                // 50100
        amp_box(m, ARG_NONE, ARG_NONE, ARG_NONE, ARG_NONE, ARG_NONE);
        amp_co(m, 0, 50);
        sh_print(sh, "The ferry is not operating today because the river is to shallow." CR);
        break;
    case RIVER_MSG_FERRY_NO_MONEY:                               // 50105
        amp_co(m, ARG_NONE, ARG_NONE);
        amp_cew(m);
        sh_print(sh, "You do not have enough money to pay for the ferry." CR);
        break;
    }
    sh_space(sh);
}

int river_ferry_offer(Shell *sh, int days) {                     // 50101
    Mach *m = sh->m;
    char d[8];
    amp_box(m, ARG_NONE, ARG_NONE, ARG_NONE, ARG_NONE, ARG_NONE);
    amp_wind(m, 6);
    sh_printf(sh, "The ferry operator says that he will charge you $5.00 and that you will have to "
              "wait %s days.  Are you willing to do this? ", a2_str(days, d));
    return sh_yesno(sh) == 'Y';
}

int river_guide_offer(Shell *sh, int sets, int has) {            // 50130-50136
    Mach *m = sh->m;
    amp_box(m, ARG_NONE, ARG_NONE, ARG_NONE, ARG_NONE, ARG_NONE);
    amp_wind(m, 6);
    sh_printf(sh, "A Shoshoni guide says that he will take your wagon across the river in exchange "
              "for %d sets of clothing." CR CR, sets);
    if (!has) {
        sh_printf(sh, "You don't have %d sets of clothing." CR, sets);
        sh_space(sh);
        return 0;
    }
    sh_print(sh, "Will you accept this " CR "offer? ");
    return sh_yesno(sh) == 'Y';
}

void river_guide_go(Shell *sh, int how) {                        // 50140-50145
    Mach *m = sh->m;
    amp_box(m, ARG_NONE, ARG_NONE, ARG_NONE, ARG_NONE, ARG_NONE);
    amp_wind(m, 6);
    sh_printf(sh, "The Shoshoni guide will help you %s" CR,
              how == 1 ? "ford the river." : "float your wagon across.");
    sh_space(sh);
}

// CROSS.LIB: the crossing drawn over the river bank (after 190: & CST).
void river_cross(Shell *sh, int method, int losses) {
    Mach *m = sh->m;
    Game *g = sh->g;
    amp_cst(m, 14848);
    amp_wind(m, ARG_NONE);                                       // 50000
    amp_rfl(m, "RIVER.IMA", sh->sa);
    sh_printf(sh, CF "%s" CC CR "%s" CC CR, lm_name(g), sh->td); // TD$ as last built: a day lost
                                                                 // floating (GOSUB 500) is not shown yet
    int x1 = 54, x2 = 222, y1 = 20, y2 = 140;
    hcolor(m, 7);                                                // 50110
    amp_box(m, x1, y1, x2, y2, 0);
    for (int l1 = 1; l1 <= 3; l1++) {
        hplot(m, x1 + l1, y1 + 4 - l1);
        hplot_to(m, x2 - l1, y1 + 4 - l1);
        hplot_to(m, x2 - l1, y2 - 4 + l1);
        hplot_to(m, x1 + l1, y2 - 4 + l1);
        hplot_to(m, x1 + l1, y1 + 4 - l1);
    }
    amp_image(m, 5, 58, 24);
    amp_image(m, method, 102, 63);
    basic_delay(sh, 301);
    for (int l = 0; l <= 52; l++) {                              // 50010: the water
        hcolor(m, 6);
        hplot(m, 114 + 2 * l, 136);
        hplot_to(m, 218, 77 + l);
    }
    if (!losses) {                                               // 50030
        for (int l = 0; l <= 60; l++) {
            hcolor(m, 4);
            hplot(m, 58, 24 + l);
            hplot_to(m, 58 + 2 * l, 24);
        }
    } else if (method == 2) {                                    // 50060: the ford goes wrong
        for (int l = 0; l <= 21; l++) {
            hcolor(m, 6);
            hplot(m, 80 + 2 * l, 71);
            hplot_to(m, 180, 100 - l);
        }
    } else {
        amp_image(m, 4, 102, 63);                                // 50050: tipped over
    }
    amp_cst(m, 14848);                                           // 3504: GOSUB 190
}

// ---------------------------------------------------------------------------
// END.LIB
// ---------------------------------------------------------------------------
int end_dalles(Shell *sh, int first) {
    Mach *m = sh->m;
    if (first) {                                                 // 50000
        amp_rfl(m, "OREGON.IMA", sh->sa);
        amp_dft(m, 7, sh->sa);
        sh_print(sh, CL CR);
        amp_image(m, 10, 10, 20);
        amp_image(m, 10, 10, 160);
        amp_dfw(m, 7, 10, 50, 269, 100);
    } else amp_box(m, ARG_NONE, ARG_NONE, ARG_NONE, ARG_NONE, ARG_NONE);   // 50040
    amp_wind(m, 7);                                              // 50010
    sh_print(sh, "The trail divides here." CR "You may:" CR CR CR "1. float down the Columbia River" CR
                 CR "2. take the Barlow Toll Road" CR CR CR "What is your choice? ");
    char z[4];
    sh_inp(sh, 1, "-12", 1, z, sizeof z);
    return atoi(z);
}

int end_barlow_ask(Shell *sh, double toll) {                     // 50020
    Mach *m = sh->m;
    char k[24];
    amp_box(m, ARG_NONE, ARG_NONE, ARG_NONE, ARG_NONE, ARG_NONE);
    amp_co(m, ARG_NONE, ARG_NONE);
    sh_printf(sh, CR CR "You must pay $%s to travel the Barlow road.  Are you willing to do this? ",
              a2_money((long)floor(toll * 100 + .5), k));
    return sh_yesno(sh) == 'Y';
}

void end_barlow_no_cash(Shell *sh) {                             // 50030
    sh_print(sh, CR "You do not have enough cash." CR);
    sh_space(sh);
}

// ---------------------------------------------------------------------------
// FLOAT, with the sprites of DALLESUTIL.OBJ (dalles.c)
// ---------------------------------------------------------------------------
// The engine asks REQ_RAFT_FRAME before each pass of FLOAT:1070-1180 and runs
// the pass's rules in raft_tick(); floatdraw.c draws what the pass drew
// (1140-1165) when the engine next asks for anything.
static FloatDrawn fl_drawn;

// One pass of the BASIC outside the & commands, as measured on the original:
// 266,000 cycles a pass with no rocks, and 94,000 more for each rock on the
// river (1075-1085, 1120/1130, 1160/1165, and 300 when it spawns). With the
// sprites, a pass takes about a quarter to half a second: the raft's speed.
#define FLOAT_PASS_CYCLES 266000
#define FLOAT_ROCK_CYCLES 94000

static void float_delay(void *ctx, int passes) { basic_delay(ctx, passes); }

void float_intro(Shell *sh) {
    Mach *m = sh->m;
    sh->common = 1; sh->pa = 7710; sh->sa = 52760;
    amp_dun(m, "L16.PCK", 7710, 16);                             // 1000
    amp_hsp(m, 2, ARG_NONE);                                     // 1010
    amp_rfl(m, "ORSPRITE.IMA", 49152);
    dalles_install(m);                                           // & RNH,"DALLESUTIL.OBJ"
    amp_wind(m, ARG_NONE);
    sh_box(sh, 7, 62, 252, 109);
    sh_print(sh, "Use the arrow keys to guide your raft through the rushing waters of the Columbia River." CR);
    sh_space(sh);
    amp_wind(m, ARG_NONE);                                       // 1015
    sh_box(sh, 7, 62, 252, 109);
    sh_print(sh, "After passing the third direction sign, land your raft at the trail to the Willamette Valley." CR);
    sh_space(sh);
    amp_dun(m, "RIVER.PCK", sh->pa, 16);                         // 1020
    amp_dft(m, 7, 49152);
    dalles_cst(m, 50800);
    amp_dpb(m, 0, 50200);
    amp_dpb(m, 1, 50400);
    amp_dpb(m, 2, 50600);
    float_draw_start(&fl_drawn);                                 // 1030
    // 110-115: the collision chirp, a one-tune buffer at 1153 (set 15): pitches
    // 21 down to 1, one unit each
    uint8_t *t = m->ram + 1153;
    t[0] = 1; t[1] = 0; t[2] = 4; t[3] = 0;
    int a = 4;
    for (int l = 21; l >= 1; l--) { t[a++] = (uint8_t)l; t[a++] = 1; }
    t[a] = 128;
    amp_dft(m, 15, 1153);
    sh->raft_landed = 0;
}

int float_frame(Shell *sh) {
    float_draw_pass(sh->m, &sh->g->raft, &fl_drawn);
    const Raft *r = &sh->g->raft;
    mach_advance(sh->m, FLOAT_PASS_CYCLES + FLOAT_ROCK_CYCLES * (unsigned)(r->fl[0] + r->fl[1]));
    return usr(sh->m, 3);                                        // 1090
}

void float_hit(Shell *sh, const Req *r) {                        // 800-810
    Mach *m = sh->m;
    const Game *g = sh->g;
    float_draw_pass(m, &g->raft, &fl_drawn);                     // the pass drew before 1170
    // 740: a collision chirps four times (not the landing or "everyone died" boxes)
    if (m->sound && (!strncmp(r->text, "The raft has hit", 16) || !strncmp(r->text, "The raft is destroyed", 21)))
        for (int s1 = 0; s1 <= 3; s1++) amp_pt(m, 1, ARG_NONE);
    amp_wind(m, ARG_NONE);
    sh_box(sh, 28, 40, 28 + 203, 40 + 30 + 9 * r->nlines);
    sh_printf(sh, "%s" CR, r->text);
    for (int l = 0; l < r->nlines; l++) sh_printf(sh, "   %s" CR, r->lines[l]);
    if (r->nlines) amp_take(m, 0, 180, 279, 191, sh->sa);
    sh_box_done(sh);
    amp_put(m, 0, 180, sh->sa);
    sh_restore(sh);
    // 770: a rock that was hit is parked off the river, if anyone survived (760)
    if (!strncmp(r->text, "The raft has hit a rock.", 24) && g->raft.rock >= 0) {
        int alive = 0;
        for (int l = 0; l < g->np; l++) alive += g->h1[l] != -2;
        if (alive) float_draw_hit_rock(m, &g->raft);
    }
}

void float_land(Shell *sh) {
    Mach *m = sh->m;
    float_draw_pass(m, &sh->g->raft, &fl_drawn);
    float_draw_landing(m, &sh->g->raft, float_delay, sh);        // 960, 900
    m->hpag = 0x40;                                              // & & : POKE 230,64
    amp_hook(m);
    sh_print(sh, CR);
    amp_dun(m, "L17.PCK", 12200, 16);
    amp_wind(m, ARG_NONE);
    amp_co(m, 0, 170);
    sh_print(sh, "One moment please..." CC CR);
    sh_loading(sh);
    amp_rfl(m, "MS16.BIN", 1153);
    sh->raft_landed = 1;
}

// ---------------------------------------------------------------------------
// WIN
// ---------------------------------------------------------------------------
static const char *const PL[4] = { "", "s", "s", "en" };        // PL$
static const char *const OCC[2][2] = { { "carpenter", "doubled" }, { "farmer", "tripled" } };

static void pad(char *out, long v, int n) {
    char t[32], s[24];
    a2_str((double)v, s);
    snprintf(t, sizeof t, "%*s%s", n, "", s);
    strcpy(out, t + strlen(t) - n);
}

// 610-622: the points table from row ZZ; pts are the displayed points.
static void win_table(Shell *sh, const Score *s, const long *pts, int zz) {
    Mach *m = sh->m;
    char a[16], b[16], k[24];
    amp_co(m, ARG_NONE, zz);                                     // 610
    sh_printf(sh, "   %s%s in %s health", a2_str((double)s->n[0], a), s->n[0] > 1 ? " people" : " person",
              HEALTH[s->band]);
    amp_co(m, 245, ARG_NONE); pad(b, pts[0], 4); sh_printf(sh, "%s" CR, b);
    sh_print(sh, "   1 wagon"); amp_co(m, 245, ARG_NONE); pad(b, pts[1], 4); sh_printf(sh, "%s" CR, b);
    pad(a, s->n[2], 4);                                          // 615
    sh_printf(sh, "%s ox%s", a, PL[(s->n[2] != 1) * 3]);
    amp_co(m, 245, ARG_NONE); pad(b, pts[2], 4); sh_printf(sh, "%s" CR, b);
    sh_printf(sh, "   %s spare wagon part%s", a2_str((double)s->n[3], a), PL[s->n[3] != 1]);   // 616
    amp_co(m, 245, ARG_NONE); pad(b, pts[3], 4); sh_printf(sh, "%s" CR, b);
    pad(a, s->n[4], 4);
    sh_printf(sh, "%s set%s of clothing", a, PL[s->n[4] != 1]);
    amp_co(m, 245, ARG_NONE); pad(b, pts[4], 4); sh_printf(sh, "%s" CR, b);
    pad(a, s->n[5], 4);                                          // 620
    sh_printf(sh, "%s bullet%s", a, PL[2 * (s->n[5] != 1)]);
    amp_co(m, 245, ARG_NONE); pad(b, pts[5], 4); sh_printf(sh, "%s" CR, b);
    pad(a, s->n[6], 4);                                          // 621
    sh_printf(sh, "%s pounds of food", a);
    amp_co(m, 245, ARG_NONE); pad(b, pts[6], 4); sh_printf(sh, "%s" CR, b);
    sh_printf(sh, "     $%s cash", a2_money((long)floor(s->cash * 100 + .5), k));
    amp_co(m, 245, ARG_NONE); pad(b, pts[7], 4); sh_printf(sh, "%s" CR, b);
    long sc = 0;
    for (int l = 0; l <= 7; l++) sc += pts[l];
    sh_print(sh, CR);                                            // 622
    amp_co(m, 185, ARG_NONE); sh_print(sh, "Total:");
    amp_co(m, 237, ARG_NONE); pad(b, sc, 5); sh_printf(sh, "%s" CR CR CR, b);
}

void win_arrive(Shell *sh) {
    Mach *m = sh->m;
    Game *g = sh->g;
    const Score *s = &g->score;
    // END.LIB:50080 unpacks the arrival picture when WIN doesn't follow FLOAT
    if (!sh->raft_landed) amp_dun(m, "L17.PCK", 49152, 16);
    sh->raft_landed = 0;
    sh->common = 1; sh->pa = 12200; sh->sa = 15900;
    amp_dft(m, 15, 51944);                                       // 100
    amp_rfl(m, "MS17.BIN", 51944);
    amp_box(m, 0, 170, 279, 191, ARG_NONE);                      // 1005
    amp_wind(m, ARG_NONE);                                       // 601
    amp_box(m, 0, 160, 279, 180, 3);
    m->invflg = 0x3F;
    amp_co(m, ARG_NONE, 162);
    sh_print(sh, "The Willamette Valley, Oregon" CC CR);
    sh_printf(sh, "%s %d, 18%d" CC CR, MONTH[PEEK(g, 902) - 1], PEEK(g, 903), PEEK(g, 901));
    m->invflg = 0xFF;
    sh_space_tune(sh);
    amp_wind(m, ARG_NONE);                                       // 605
    sh_box(sh, 14, 60, 253, 108);
    sh_print(sh, "Congratulations!  You have made it to Oregon!  Let's see how many points you have received." CR);
    sh_space(sh);
    sh_print(sh, CL CR);                                            // 606
    amp_wind(m, ARG_NONE);
    const char *z = "Points for arriving in Oregon";
    int zw = (int)((strlen(z) / 2.0 + .5) * 8);
    amp_box(m, 139 - zw, 0, 139 + zw, 11, 3);
    m->invflg = 0x3F;
    amp_co(m, ARG_NONE, 2);
    sh_printf(sh, "%s" CC CR, z);
    m->invflg = 0xFF;
    sh_print(sh, CR CR);
    int zz, zy;
    amp_gcp(m, NULL, &zz);
    win_table(sh, s, s->pts, zz);
    amp_gcp(m, NULL, &zy);                                       // 625
    amp_dfw(m, 7, 20, zy, 259, 191 - zy);
    amp_wind(m, 7);
    if (s->occupation > 1) {
        sh_printf(sh, "For going as a %s, your points are %s." CR, OCC[s->occupation - 2][0], OCC[s->occupation - 2][1]);
        sh_space(sh);
        long pts[8];
        for (int l = 0; l <= 7; l++) pts[l] = s->pts[l] * s->occupation;
        amp_wind(m, ARG_NONE);
        win_table(sh, s, pts, zz);
        amp_gcp(m, NULL, &zy);
        amp_dfw(m, 7, 20, zy, 259, 191 - zy);
        amp_wind(m, 7);
    }
    sh_space(sh);                                                // 630
}

// 200 + 2000-2020
void win_topten_show(Shell *sh, int qualified) {
    Mach *m = sh->m;
    TopTen hi;
    sh_print(sh, CL CR);
    amp_wind(m, ARG_NONE);
    amp_co(m, 0, 89);
    sh_print(sh, "One moment please..." CR);
    sh_loading(sh);
    amp_rfl(m, "FIRST.IMA", 8192);
    amp_dft(m, 7, 8192);
    amp_cst(m, 14848);
    topten_read(sh->persist, hi);
    sh_print(sh, CL CR);                                            // 2000
    amp_wind(m, ARG_NONE);
    amp_image(m, 5, 55, 0);
    amp_co(m, 10, 20);  sh_print(sh, "Name" CR);
    amp_co(m, 130, 20); sh_print(sh, "Points" CR);
    amp_co(m, 210, 20); sh_print(sh, "Rating" CR);
    sh_print(sh, CR);
    amp_gcp(m, NULL, &sh->y5);                                   // ZZ
    amp_dfw(m, 7, 20, 140, 230, 51);                             // 2015
    if (!qualified) {
        char sc[16];
        amp_co(m, ARG_NONE, sh->y5);                             // 700
        for (int l = 0; l <= 9; l++) {
            int zx, zy;
            amp_gcp(m, &zx, &zy);
            for (int l1 = 0; l1 <= 2; l1++) {
                char z[OT_NAME_MAX + 8];
                if (l1 == 1) { char t[OT_NAME_MAX + 4]; snprintf(t, sizeof t, "  %s", hi[l][1]); snprintf(z, sizeof z, "%s", a2_right(t, 4)); }
                else snprintf(z, sizeof z, "%.*s", OT_NAME_MAX, hi[l][l1]);
                amp_co(m, 139 * (l1 == 1) + 189 * (l1 == 2), zy);
                sh_printf(sh, "%s" CR, z);
            }
        }
        amp_wind(m, 7);
        sh_printf(sh, "You have accumulated %s points.  This is not enough to qualify for the Oregon Top Ten." CR,
                  a2_str((double)sh->g->score.total, sc));
        sh_space(sh);
        return;
    }
    amp_wind(m, 7);                                              // 2020
    sh_print(sh, "Congratulations!  Type your name as you would like to see it on the Oregon Top Ten list." CR);
}

// 520-530
void win_topten_name(Shell *sh, int g_row, const TopTen *hip, char *out, int max) {
    Mach *m = sh->m;
    const char (*hi)[3][OT_NAME_MAX] = *hip;
    int zz = sh->y5;
    amp_wind(m, ARG_NONE);
    amp_co(m, ARG_NONE, zz);                                     // 700 with the new row blank
    for (int l = 0; l <= 9; l++) {
        int zx, zy;
        amp_gcp(m, &zx, &zy);
        for (int l1 = 0; l1 <= 2; l1++) {
            char z[OT_NAME_MAX + 8];
            const char *v = l == g_row ? "" : hi[l][l1];
            if (l1 == 1) { char t[OT_NAME_MAX + 4]; snprintf(t, sizeof t, "  %s", v); snprintf(z, sizeof z, "%s", a2_right(t, 4)); }
            else snprintf(z, sizeof z, "%s", v);
            amp_co(m, 139 * (l1 == 1) + 189 * (l1 == 2), zy);
            sh_printf(sh, "%s" CR, z);
        }
    }
    for (;;) {                                                   // 530
        amp_wind(m, ARG_NONE);
        amp_co(m, ARG_NONE, zz + g_row * 9);
        amp_cel(m);
        sh_inp(sh, 15, "-AZ-az .'-", 1, out, max);
        char t[OT_NAME_MAX + 4];
        amp_co(m, 139, zz + g_row * 9);
        snprintf(t, sizeof t, " %s", hi[g_row][1]);
        sh_print(sh, a2_right(t, 4));
        amp_co(m, 189, ARG_NONE);
        sh_printf(sh, "%s" CR, hi[g_row][2]);
        amp_wind(m, 7);
        amp_box(m, ARG_NONE, ARG_NONE, ARG_NONE, ARG_NONE, ARG_NONE);
        sh_print(sh, "Would you like to make any changes? ");
        if (sh_yesno(sh) != 'Y') return;
        out[0] = 0;
    }
}
