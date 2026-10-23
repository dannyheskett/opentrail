// OREGON TRAIL's display code and the overlays' screens (PACE, RATION, MAP,
// TALK, BUY, TRADE, HUNT, PART, TOMB, LF). Line numbers are OREGON TRAIL's
// unless a file is named.
#include "screen.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define Y5 111
#define SEQ_MAX 16384

// 22000: DATA for 400 and the status labels.
static const char *const STATUS_LABEL[6] = { "Date", "Weather", "Health", "Food",
                                             "Next landmark", "Miles traveled" };

// ---------------------------------------------------------------------------
// Small helpers
// ---------------------------------------------------------------------------
static void inverse(Shell *sh) { sh->m->invflg = 0x3F; }
static void normal(Shell *sh) { sh->m->invflg = 0xFF; }
static void cst(Shell *sh) { amp_cst(sh->m, 14848); }            // 190: & CST,14848

// 250: TD$
static const char *td(Shell *sh) {
    const Game *g = sh->g;
    snprintf(sh->td, sizeof sh->td, "%s %d, %d", MONTH[g->am - 1], g->ad, g->ay);
    return sh->td;
}

// 260: the landmark name without a leading "the "
static const char *lm_name(const Game *g) {
    const char *n = LM_NAME[g->lm];
    return strncmp(n, "the ", 4) ? n : n + 4;
}

static int ground_colour(const Game *g) {                        // 310: CC
    return 3 * (g->as >= 1) + (g->as < 1) * (5 * (g->ar <= .2) + (g->ar > .2));
}

// 200: K$
static const char *kmoney(long cents, char *buf) { return a2_money(cents, buf); }

// Right-justify s in n characters (RIGHT$("   " + s, n)).
static const char *rjust(const char *s, int n, char *buf) {
    char t[64];
    snprintf(t, sizeof t, "%*s%s", n, "", s);
    snprintf(buf, 64, "%s", t + strlen(t) - n);
    return buf;
}

// ---------------------------------------------------------------------------
// 300-400: the travel and landmark screens
// ---------------------------------------------------------------------------
static int s_art_i = 1;         // I: 8 after events that redraw the whole strip (310)

static void hint400(Shell *sh) {
    Mach *m = sh->m;
    Game *g = sh->g;
    amp_wind(m, ARG_NONE);
    if (g->lm != g->nm) {
        amp_box(m, 0, Y5, 279, Y5 + 9, ARG_NONE);
        amp_hsp(m, 1, ARG_NONE);
        amp_co(m, ARG_NONE, Y5 + 1);
        sh_print(sh, "Press RETURN to size up the situation" CC CR);
        amp_hsp(m, ARG_NONE, ARG_NONE);
    }
}

static void scene300(Shell *sh) {
    Mach *m = sh->m;
    Game *g = sh->g;
    char buf[64];
    g->f9 = 0;
    amp_wind(m, ARG_NONE);
    td(sh);                                                      // GOSUB 250
    sh->x1 = 0; sh->x2 = 278; sh->y2 = 180;
    if (!g->ll) {                                                // 300-305: the landmark picture
        if (g->lm) {
            char v[16];
            snprintf(v, sizeof v, "L%d.PCK", g->lm);
            amp_dun(m, v, sh->pa, 16);
            cst(sh);
        }
        amp_box(m, sh->x1, 160, sh->x2, 180, 3);
        inverse(sh);
        amp_co(m, ARG_NONE, 162);
        sh_printf(sh, "%s" CC CR, lm_name(g));
        sh_printf(sh, "%s" CC CR, sh->td);
        normal(sh);
        return;
    }
    // 310: the travel strip
    int sr = g->sr;
    amp_box(m, 0, sr * 31, 279 - sr * (s_art_i != 8) * 94, 191 - sr * 132, 4);
    g->sr = 1;
    int cc = ground_colour(g);
    sh->cc = cc;
    sh->y1 = Y5;
    amp_dft(m, 7, 49152);
    amp_image(m, 4, 0, 13);
    amp_box(m, 0, 60, 278, Y5 - 1, cc);
    s_art_i = 1;
    // 312: the leg's landmark icon and the three wagon frames, unpacked
    int ni = sh->ni, z;                                          // NI, set at leg start (3000)
    sh->ia[0] = sh->sa;
    amp_uim(m, ni, sh->ia[0], &z);
    sh->ia[1] = sh->ia[0] + z;
    for (int l = 1; l <= 3; l++) { amp_uim(m, l, sh->ia[l], &z); sh->ia[l + 1] = sh->ia[l] + z; }
    cst(sh);
    sh->wg = 1;
    amp_image(m, sh->wg, 186, 32);
    amp_dpc(m, 278, 170, &z);
    if (!z) amp_box(m, sh->x1, sh->y1, sh->x2, sh->y2, 3);
    hint400(sh);                                                 // 315
    inverse(sh);
    char t[6][80];
    snprintf(t[0], 80, "%s", sh->td);                      // TD$ from 300's GOSUB 250
    snprintf(t[1], 80, "%s", WEATHER[g->w]);
    snprintf(t[2], 80, "%s", HEALTH[(int)(g->h / 35)]);
    a2_str(g->pf, buf); snprintf(t[3], 80, "%s pounds", buf);
    a2_str(floor(g->d), buf); snprintf(t[4], 80, "%s miles", buf);
    a2_str(floor(g->m), buf); snprintf(t[5], 80, "%s miles", buf);
    amp_co(m, ARG_NONE, 125);                                    // 320
    for (int l = 0; l <= 5; l++) { amp_co(m, STATUS_COL[l], ARG_NONE); sh_printf(sh, "%s:" CR, STATUS_LABEL[l]); }
    amp_wind(m, 5);
    for (int l = 0; l <= 5; l++) sh_printf(sh, CE "%s" CR, t[l]);
}

// 3000: the leg's strip geometry (FX, IX) before the scene is drawn.
static void leg_geometry(Shell *sh) {
    Game *g = sh->g;
    sh->fx = 180 - SCENERY[g->nm - 1][1];
    sh->ix = (int)(sh->fx - g->d * 2);
    sh->ni = SCENERY[g->nm - 1][0];
}

void trail_gosub(Shell *sh, int line) {
    Mach *m = sh->m;
    switch (line) {
    case 300: scene300(sh); break;
    // 350: every caller has just done & WIND (1000, 3170, 400 before 570); CC is
    // the ground colour 310 last computed, not today's
    case 350: amp_wind(m, ARG_NONE); amp_box(m, 0, 60, 278, 80, sh->cc); break;
    case 355: amp_wind(m, ARG_NONE); amp_box(m, 0, Y5, 278, Y5 + 9, 3); break;
    case 400: hint400(sh); break;
    case 950: sh_space_tune(sh); break;
    case 500: case 3181: case 3235: inverse(sh); break;
    case 3180: amp_wind(m, ARG_NONE); normal(sh); amp_dft(m, 7, 52200); break;
    case 3190: cst(sh); amp_dft(m, 7, 49152); amp_wind(m, ARG_NONE); break;
    case 3499: normal(sh); break;
    case 3000: usr(m, 1); break;                                 // X = USR(1): clear the strobe
    }
}

// The pace of the days. Drawing is instant, so it is set here: a travel day
// about as long as the original's (some 2 s, of which each wagon step was the
// 0.1 s its picture took to draw), a day of rest shorter.
#define DAY_PAUSE  (A2_HZ * 11 / 10)
#define WAGON_STEP (A2_HZ / 10)
#define REST_DAY   (A2_HZ * 3 / 10)

// 3246-3265: the wagon moves on, the status panel updates.
void trail_day_end(Shell *sh, int travel) {
    Mach *m = sh->m;
    Game *g = sh->g;
    char buf[40];
    mach_advance(m, travel ? DAY_PAUSE : REST_DAY);
    if (travel) {                                                // 3246
        int vp = (int)floor((sh->fx - g->d * 2 - sh->ix) / 4.0 + .5);
        for (int l4 = 1; l4 <= vp; l4++) {
            sh->wg = sh->wg * (sh->wg < 3) + 1;
            sh->ix += 4;
            amp_dft(m, 7, sh->ia[0]);
            amp_image(m, 1, sh->ix, 31);
            amp_dft(m, 7, sh->ia[sh->wg]);
            amp_image(m, 1, 186, 32);
            mach_advance(m, WAGON_STEP);
        }
    }
    if (g->sr) {                                                 // 3260
        amp_wind(m, 5);
        sh_printf(sh, CE "%s %d, %d" CR, MONTH[g->am - 1], g->ad, g->ay);
        sh_printf(sh, CE "%s" CR, WEATHER[g->w]);
        sh_printf(sh, CE "%s" CR, HEALTH[(int)(g->h / 35)]);
        sh_printf(sh, CE "%s pounds" CR, a2_str(g->pf, buf));
    }
    if (travel) {                                                // 3265
        sh_printf(sh, CE "%s miles" CR, a2_str(floor(g->d + .5), buf));
        sh_printf(sh, CE "%s miles" CR, a2_str(floor(g->m + .5), buf));
    }
}

// ---------------------------------------------------------------------------
// Arrival, splits, legs
// ---------------------------------------------------------------------------
// 1005-1006
int trail_look_around(Shell *sh) {
    Mach *m = sh->m;
    Game *g = sh->g;
    char v[16];
    amp_wind(m, ARG_NONE);                                       // 1000
    g->f9 = 0;
    snprintf(v, sizeof v, "MS%d.BIN", g->lm);
    amp_rfl(m, v, 51944);
    trail_gosub(sh, 350);
    trail_gosub(sh, 355);
    sh_box(sh, 21, 60, 252, 98);
    sh_printf(sh, "You are now at %s.  Would you like to look around? ", LM_NAME[g->lm]);
    int yes = sh_yesno(sh) == 'Y';
    sh_restore(sh);
    return yes;
}

// 2110-2120
int trail_split(Shell *sh) {
    Mach *m = sh->m;
    Game *g = sh->g;
    sh_box(sh, 0, 60, 279, 154);
    sh_print(sh, "The trail divides here.  You may:" CR CR);
    amp_in(m, 10);
    int l = 1;
    for (; l <= 2; l++) {
        int leg = l == 1 ? LM_LEG[g->lm] : LM_ALT[g->lm];
        sh_printf(sh, "%d. head for %s" CR, l, LM_NAME[LEG_DEST[leg]]);
    }
    sh_printf(sh, "%d. see the map" CR CR, l);
    amp_in(m, ARG_NONE);
    sh_print(sh, CR "What is your choice? ");
    char z[4];
    sh_inp(sh, 1, "-13", 1, z, sizeof z);
    sh_restore(sh);
    return atoi(z);
}

// 2200 (after the engine chose the leg)
void trail_leg(Shell *sh) {
    Mach *m = sh->m;
    Game *g = sh->g;
    char buf[16];
    inverse(sh);
    amp_wind(m, 5);
    amp_co(m, ARG_NONE, 36);
    sh_printf(sh, CE "%s miles" CR, a2_str(g->d, buf));
    sh_box(sh, 28, 60, 250, 97 + 9 * (g->lm == 1));
    sh_printf(sh, "From %s it is %s miles to %s." CR, LM_NAME[g->lm], buf, LM_NAME[g->nm]);
    sh_space_tune(sh);
    leg_geometry(sh);                                            // 3000 follows at once
}

// ---------------------------------------------------------------------------
// Event messages (550, 700, 710, 900)
// ---------------------------------------------------------------------------
static void art_draw(Shell *sh, int art) {
    Mach *m = sh->m;
    switch (art) {
    case ART_SNOW:     amp_image(m, 8, 130, 47); break;          // 10000
    case ART_STORM:    amp_image(m, 1, 105, 0); break;           // 10605
    case ART_BLIZZARD: amp_image(m, 3, 105, 0); break;
    case ART_HAIL:     amp_image(m, 2, 118, 0); break;           // 10705
    case ART_FIRE:     amp_image(m, 5, 184, 32); break;          // 11210
    case ART_FRUIT:    amp_image(m, 6, 150, 32); break;          // 11100
    case ART_THIEF:    amp_image(m, 7, 263, 35); break;          // 11330
    }
}

static void art_clear(Shell *sh, int art) {
    Mach *m = sh->m;
    switch (art) {
    case ART_STORM: case ART_BLIZZARD: amp_box(m, 105, 0, 165, 15, ARG_NONE); break;
    case ART_HAIL: amp_wind(m, ARG_NONE); amp_box(m, 118, 0, 165, 15, ARG_NONE); break;
    case ART_THIEF: amp_box(m, 263, 35, 279, 59, ARG_NONE); break;
    case ART_FIRE: s_art_i = 8; break;                         // 11210: I = 8, then GOSUB 300
    }
}

// 700: a box sized to the message.
static void box700(Shell *sh, const char *v) {
    int y1 = 60 + sh->goff, y2 = y1 + 20;
    int x = 6 + (int)floor(strlen(v) / 2.0 * 8);
    int x1 = (139 - x) * (x < 140), x2 = (139 + x) * (x < 140) + 279 * (x >= 140);
    sh_box(sh, x1, y1, x2, y2);
    sh_printf(sh, "%s" CC CR, v);
}

// 900: text and the loss lines.
static void box900(Shell *sh, const char *a, const char (*lines)[OT_LINE_MAX], int n) {
    sh_box(sh, 28, 60, 238, 90 + 9 * n);
    sh_printf(sh, "%s" CR, a);
    for (int l = 0; l < n; l++) sh_printf(sh, "   %s" CR, lines[l]);
}

static int s_event_art;

void trail_event_show(Shell *sh, const Req *r) {
    Mach *m = sh->m;
    (void)m;
    s_event_art = r->b;
    art_draw(sh, r->b);                                          // (3180 selected EVENTS.IMA)
    switch (r->a) {
    case EV_BOX: sh->goff = r->c; box700(sh, r->text); break;
    case EV_TOAST: box700(sh, r->text); break;
    case EV_LOSE_DAYS: box700(sh, r->text); break;              // 550
    case EV_LIST: box900(sh, r->text, r->lines, r->nlines); break;
    }
}

int trail_event_end(Shell *sh, const Req *r) {
    Mach *m = sh->m;
    switch (r->a) {
    case EV_BOX:                                                 // 710
        sh_box_done(sh);
        sh->goff = 0;
        sh_restore(sh);
        break;
    case EV_LOSE_DAYS:                                           // 570: GOSUB 400 came first
        sh_space(sh);
        break;
    case EV_LIST:                                                // 900
        sh_box_done(sh);
        sh_restore(sh);
        break;
    }
    int ret = (m->kbd & 0x7F) == 13;                             // PEEK(-16384) = 13
    art_clear(sh, s_event_art);
    s_event_art = ART_NONE;
    return ret;
}

// ---------------------------------------------------------------------------
// The action menu and its choices (4000-4900)
// ---------------------------------------------------------------------------
int trail_menu(Shell *sh, int first) {
    Mach *m = sh->m;
    Game *g = sh->g;
    if (first) {                                                 // 4000
        amp_rfl(m, "OREGON.IMA", sh->sa);
        amp_dft(m, 7, sh->sa);
        normal(sh);
    }
    amp_wind(m, ARG_NONE);                                       // 4005
    sh_print(sh, CL CR);
    amp_co(m, ARG_NONE, ARG_NONE);
    sh_printf(sh, "%s" CC CR, g->ll ? "" : lm_name(g));          // 4010
    sh_printf(sh, "%s" CC CR, td(sh));
    inverse(sh);
    amp_wind(m, 2);
    amp_box(m, ARG_NONE, ARG_NONE, ARG_NONE, ARG_NONE, ARG_NONE);
    amp_co(m, ARG_NONE, 5);
    amp_in(m, 10);
    double h = g->h > 139 ? 139 : g->h;
    sh_printf(sh, "Weather: %s" CR, WEATHER[g->w]);              // 4020
    sh_printf(sh, "Health: %s" CR, HEALTH[(int)(h / 35)]);
    sh_printf(sh, "Pace: %s" CR, PACE[g->p - 1]);
    sh_printf(sh, "Rations: %s" CR, RATION[g->r - 1]);
    normal(sh);
    amp_in(m, ARG_NONE);
    amp_wind(m, 1);                                              // 4030
    sh_print(sh, CF "You may:" CR);
    sh_print(sh, CR);                                            // 4040
    amp_in(m, 30);
    int l = 0;
    for (; l <= 6; l++) sh_printf(sh, "%d. %s" CR, l + 1, ACTION[l]);
    l = l + 1;
    if (!g->ll) {
        sh_printf(sh, "%d. %s" CR, l, ACTION[7]);
        l++;
        if (LM_TYPE[g->lm] == 1) { sh_printf(sh, "%d. %s" CR, l, ACTION[8]); l++; }
    }
    if (g->ll) { sh_printf(sh, "%d. %s" CR, l, ACTION[9]); l++; }    // 4050
    char range[16], z[4];
    snprintf(range, sizeof range, "-1%d", l - 1);                // 4060
    amp_in(m, ARG_NONE);
    amp_co(m, ARG_NONE, 110);
    sh_print(sh, "What is your choice? ");
    sh_inp(sh, 1, range, 1, z, sizeof z);
    return atoi(z);
}

// 4070
void trail_must_trade(Shell *sh, int b) {
    Mach *m = sh->m;
    amp_wind(m, ARG_NONE);
    amp_box(m, 0, 73, 279, 191, ARG_NONE);
    amp_wind(m, 3);
    sh_printf(sh, "You must trade for %s%s to be able to continue." CR, b == 2 ? "an " : "a ",
              STORE_UNIT[b - 2]);
    sh_box_done(sh);
}

// 4100-4110
void trail_supplies(Shell *sh) {
    Mach *m = sh->m;
    Game *g = sh->g;
    char k[24], buf[64], num[32];
    amp_wind(m, ARG_NONE);
    sh_print(sh, CL CR "  Your Supplies" CC CR);
    kmoney(g->my_cents, k);
    amp_image(m, 2, 0, 30); amp_image(m, 3, 20, 80); amp_image(m, 8, 10, 140);
    amp_image(m, 5, 82, 120); amp_image(m, 6, 169, 130);
    amp_wind(m, 4);
    int zy;
    for (int l = 2; l <= 8; l++) {
        sh_print(sh, ITEM[l]);
        amp_gcp(m, NULL, &zy);
        sh_print(sh, CR);
        amp_co(m, 130, zy);
        sh_printf(sh, "%s" CR, rjust(a2_str(floor(g->inv[l] + .51), num), 4, buf));
    }
    amp_gcp(m, NULL, &zy);
    sh_print(sh, "money left" CR);
    amp_co(m, 120, zy);
    snprintf(num, sizeof num, "$%s", k);
    sh_printf(sh, "%s" CR, rjust(num, 8, buf));
    sh_box_done(sh);
}

// 4200 + MAP.LIB
void trail_map(Shell *sh) {
    Mach *m = sh->m;
    Game *g = sh->g;
    amp_dun(m, "M0.PCK", sh->pa, 16);
    hcolor(m, 4);
    if (g->q1 - 1)
        for (int l = 0; l <= g->q1 - 2; l++) {
            hplot(m, MAP_XY[g->q[l]][0], MAP_XY[g->q[l]][1]);
            hplot_to(m, MAP_XY[g->q[l + 1]][0], MAP_XY[g->q[l + 1]][1]);
        }
    if (g->lm != g->nm) {                                        // 50010
        double x1 = MAP_XY[g->nm][0], x2 = MAP_XY[g->lm][0];
        double y1 = MAP_XY[g->nm][1], y2 = MAP_XY[g->lm][1];
        double sl = (y2 - y1) / (x2 - x1);
        double z = g->d * (x2 - x1) / g->dd;
        double x = z + x1, y = y2 - sl * (x2 - x);
        hplot(m, (int)x2, (int)y2);
        hplot_to(m, (int)x, (int)y);
    }
    sh_box_done(sh);                                             // 50015
    sh_print(sh, CL CR);
    cst(sh);
}

// PACE.LIB
static void underline(Shell *sh, int z) {                        // 50100
    int zx, zy;
    amp_gcp(sh->m, &zx, &zy);
    zy += 8;
    hcolor(sh->m, 6);
    hplot(sh->m, z, zy);
    hplot_to(sh->m, zx - 3, zy);
}

int trail_pace(Shell *sh) {
    Mach *m = sh->m;
    Game *g = sh->g;
    for (;;) {
        amp_wind(m, ARG_NONE);                                   // 50000
        sh_print(sh, CL CR);
        amp_image(m, 10, 10, 10);
        amp_image(m, 10, 10, 170);
        amp_co(m, 0, 35);
        sh_print(sh, "Change pace" CC CR);
        sh_printf(sh, "(currently \"%s\")" CC CR CR, PACE[g->p - 1]);
        amp_in(m, 25);
        sh_print(sh, "The pace at which you travel" CR "can change.  Your choices are:" CR);   // 50005
        int zx, zy, x, y;
        amp_gcp(m, NULL, &zy);
        amp_dfw(m, 7, 40, zy, 239, 191 - zy);
        amp_wind(m, 7);
        sh_print(sh, CR);                                        // 50010
        amp_in(m, -20);
        for (int l = 1; l <= 3; l++) sh_printf(sh, "%d. a %s pace" CR, l, PACE[l - 1]);
        sh_print(sh, "4. find out what these different paces mean" CR CR);
        amp_gcp(m, NULL, &y);
        amp_wind(m, ARG_NONE);                                   // 50020
        amp_co(m, ARG_NONE, zy + y);
        amp_in(m, 25);
        sh_print(sh, "What is your choice? ");
        amp_in(m, ARG_NONE);
        amp_gcp(m, &x, &y);
        char a[4];
        sh_inp(sh, 1, "-14", 0, a, sizeof a);
        if (!strcmp(a, "4")) {                                   // 50040-50070
            amp_hsp(m, 1, ARG_NONE);
            amp_wind(m, 0);
            sh_print(sh, CL CR);
            sh_print(sh, PACE[0]); underline(sh, 0);
            sh_print(sh, " - You travel about 8 hours a day, taking frequent rests.  You take care "
                         "not to get too tired." CR CR);
            sh_print(sh, PACE[1]); underline(sh, 0);
            sh_print(sh, " - You travel about 12 hours a day, starting just after sunrise and stopping "
                         "shortly before sunset.  You stop to rest only when necessary.  You finish "
                         "each day feeling very tired." CR CR);
            sh_print(sh, PACE[2]); underline(sh, 0);
            sh_print(sh, " - You travel about 16 hours a day, starting before sunrise and continuing "
                         "until dark.  You almost never stop to rest.  You do not get enough sleep "
                         "at night.  ");
            sh_print(sh, "You finish each day feeling absolutely exhausted, and your health suffers." CR);
            amp_hsp(m, ARG_NONE, ARG_NONE);
            sh_space(sh);
            continue;
        }
        (void)zx;
        int p = a[0] ? atoi(a) : g->p;                           // 50030
        amp_co(m, x, y);
        sh_printf(sh, "%d" CR, p);
        return a[0] ? p : 0;
    }
}

// RATION.LIB
int trail_rations(Shell *sh) {
    Mach *m = sh->m;
    Game *g = sh->g;
    amp_wind(m, ARG_NONE);
    sh_print(sh, CL CR);
    amp_image(m, 10, 10, 0);
    amp_image(m, 10, 10, 180);
    amp_co(m, ARG_NONE, 20);
    sh_print(sh, "Change food rations" CC CR);
    sh_printf(sh, "(currently \"%s\")" CC CR CR, RATION[g->r - 1]);
    int zx, zy, y;
    amp_gcp(m, &zx, &zy);
    amp_dfw(m, 7, 10, zy, 269, 191 - zy);                        // 50005
    amp_wind(m, 7);
    sh_print(sh, "The amount of food the people in your party eat each day can change.  These amounts are:" CR);
    amp_gcp(m, NULL, &y);
    zy += y;                                                     // 50010
    amp_dfw(m, 6, 30, zy, 249, 191 - zy);
    amp_wind(m, 6);
    sh_print(sh, CR);
    amp_in(m, -20);
    sh_printf(sh, "1. %s - meals are large and generous." CR, RATION[0]);
    sh_printf(sh, CR "2. %s - meals are small, but adequate." CR CR, RATION[1]);
    sh_printf(sh, "3. %s - meals are very small; everyone stays hungry." CR, RATION[2]);
    amp_wind(m, 7);
    amp_in(m, ARG_NONE);
    amp_co(m, ARG_NONE, 164);
    sh_print(sh, "What is your choice? ");                       // 50040
    int x;
    amp_gcp(m, &x, &y);
    char z[4];
    sh_inp(sh, 1, "-13", 0, z, sizeof z);
    int r = z[0] ? atoi(z) : g->r;
    amp_co(m, x, y);
    sh_printf(sh, "%d" CR, r);
    return z[0] ? r : 0;
}

// 4500
int trail_rest_ask(Shell *sh) {
    sh_box(sh, 40, 60, 240, 88);
    sh_print(sh, "How many days would you like to rest? ");
    char z[4];
    sh_inp(sh, 1, "-09", 1, z, sizeof z);
    return atoi(z);
}

// 4505 (and the ferry wait, X5 = 18)
void trail_rest_date(Shell *sh, int row) {
    amp_wind(sh->m, ARG_NONE);
    amp_co(sh->m, ARG_NONE, row);
    sh_printf(sh, CE "%s" CC CR, td(sh));
}

// TALK.LIB: the speaker's record in OREGON{1,2}.SEQ.
void trail_talk(Shell *sh, int speaker) {
    Mach *m = sh->m;
    Game *g = sh->g;
    static uint8_t seq[SEQ_MAX * 2];
    char name[24], a[256] = "", z[256] = "", v[256] = "";
    snprintf(name, sizeof name, "OREGON%d.SEQ", g->s + 1);
    int n = m->load ? m->load(m->load_ctx, name, seq, sizeof seq) : -1;
    if (n > 0) {
        int pos = (g->lm - g->s * 5) * 768 + speaker * 256;
        seq_read(seq, n, &pos, a, sizeof a);
        int found = seq_read(seq, n, &pos, z, sizeof z);
        if (!found) seq_read(seq, n, &pos, v, sizeof v);
    }
    sh_print(sh, CL CR);
    amp_image(m, 10, 10, 10);
    amp_image(m, 10, 10, 160);
    amp_dfw(m, 7, 20, 40, 230, 151);
    amp_wind(m, 7);
    sh_printf(sh, "%s tells you:" CR CR "\"%s%s\"" CR, a, z, v);
    sh_box_done(sh);
    cst(sh);
}

// ---------------------------------------------------------------------------
// BUY.LIB: fort stores
// ---------------------------------------------------------------------------
static int s_store_zy, s_store_x, s_store_y;

int trail_store_menu(Shell *sh, int first) {
    Mach *m = sh->m;
    Game *g = sh->g;
    char k[24];
    if (first) {                                                 // 50000-50005
        amp_wind(m, ARG_NONE);
        sh_print(sh, CL CR);
        int z = (int)strlen(LM_NAME[g->lm]), x = (int)strlen(sh->td);   // TD$ from 4010
        z = z * (z >= x) + x * (z < x);
        amp_box(m, 130 - (int)floor(z / 2.0 + .5) * 8, 1, 148 + (int)floor(z / 2.0 + .5) * 8, 22, 3);
        inverse(sh);
        amp_co(m, ARG_NONE, 4);
        sh_printf(sh, "%s" CC CR "%s" CC CR, LM_NAME[g->lm], sh->td);
        normal(sh);
        int y;
        amp_gcp(m, NULL, &y);                                    // 50003
        amp_co(m, ARG_NONE, y + 4);
        sh_print(sh, CR "You may buy:" CR CR);
        amp_gcp(m, &s_store_x, &s_store_y);
        for (int l = 0; l <= 6; l++) sh_printf(sh, "  %d. %s" CR, l + 1, STORE_NAME[l]);
        sh_print(sh, "  8. Leave store" CR CR);
        int zx;
        amp_gcp(m, &zx, &s_store_zy);
        amp_dfw(m, 7, s_store_x + 150, s_store_y, 279 - (s_store_x + 150), 191 - s_store_y);
        amp_wind(m, 7);
        for (int l = 0; l <= 6; l++) {
            char p[24];
            snprintf(p, sizeof p, " %s", kmoney(store_price_cents(l + 1, g->lm), k));
            sh_printf(sh, "%s per %s" CR, a2_right(p, 5), STORE_UNIT[l]);   // RIGHT$(" " + K$, 5)
        }
    }
    amp_dfw(m, 7, 0, s_store_zy, 279, 191 - s_store_zy);        // 50010
    amp_wind(m, 7);
    amp_box(m, ARG_NONE, ARG_NONE, ARG_NONE, ARG_NONE, ARG_NONE);
    sh_printf(sh, "You have $%s to spend." CR "Which number? ", kmoney(g->my_cents, k));
    char z[4];
    sh_inp(sh, 1, "-18", 1, z, sizeof z);
    int l = atoi(z);
    if (l == 8) cst(sh);                                         // 4800: GOSUB 190
    return l;
}

int trail_store_qty(Shell *sh, int l) {
    sh_printf(sh, "How many %s%s? ", STORE_UNIT[l - 1], STORE_PLURAL[l - 1]);
    char z[8];
    sh_inp(sh, 3 + (l == 7), "-09", 1, z, sizeof z);
    return atoi(z);
}

void trail_store_msg(Shell *sh, int msg, int l) {
    Mach *m = sh->m;
    switch (msg) {
    case STORE_MSG_AFFORD: sh_print(sh, "You cannot afford that." CR); break;
    case STORE_MSG_AFFORD_MANY: sh_print(sh, "You cannot afford that many." CR); break;
    case STORE_MSG_OXEN_CAP: amp_co(m, 0, 18); sh_print(sh, CF "You may only take 20 oxen." CR); break;
    case STORE_MSG_FOOD_CAP: amp_co(m, 0, 18); sh_print(sh, CF "Your wagon may only carry 2000 pounds of food." CR); break;
    case STORE_MSG_PART_CAP: amp_co(m, 0, 18); sh_printf(sh, CF "Your wagon may only carry 3 %s." CR, ITEM[l + 1]); break;
    }
    sh_space(sh);
}

// ---------------------------------------------------------------------------
// TRADE.LIB
// ---------------------------------------------------------------------------
static void trade_supplies(Shell *sh) {                          // 50050
    Mach *m = sh->m;
    char buf[64], num[24];
    amp_wind(m, 4);
    for (int l = 2; l <= 8; l++) {
        sh_print(sh, ITEM[l]);
        amp_co(m, 130, ARG_NONE);
        sh_printf(sh, CE "%s" CR, rjust(a2_str(floor(sh->g->inv[l] + .5), num), 4, buf));
    }
}

void trail_trade_show(Shell *sh, int refresh) {
    Mach *m = sh->m;
    if (!refresh) {                                              // 50000
        amp_wind(m, ARG_NONE);
        sh_print(sh, CL CR "Your Supplies" CC CR);
        amp_image(m, 2, 0, 30); amp_image(m, 3, 20, 80); amp_image(m, 8, 71, 110); amp_image(m, 6, 169, 100);
    }
    trade_supplies(sh);
    if (!refresh) {
        amp_dfw(m, 7, 0, 150, 279, 41);                          // 50025
        amp_wind(m, 7);
    }
}

void trail_trade_nobody(Shell *sh) {
    sh_print(sh, "No one wants to" CC CR "trade with you today." CC CR);
    sh_space(sh);
    cst(sh);
}

void trail_trade_lack(Shell *sh, const char *want) {
    sh_printf(sh, "You meet another emigrant who wants %s.  You don't have this." CR, want);
    sh_space(sh);
    cst(sh);
}

int trail_trade_offer(Shell *sh, const char *want, const char *offer, int she) {
    Mach *m = sh->m;
    sh_printf(sh, "You meet another emigrant who wants %s.  ", want);
    sh_printf(sh, "%s will trade you %s." CR, she ? "She" : "He", offer);
    amp_co(m, ARG_NONE, 32);
    sh_print(sh, "Are you willing to trade? ");
    int yes = sh_yesno(sh) == 'Y';
    cst(sh);                                                     // 4900: GOSUB 190
    return yes;
}

// ---------------------------------------------------------------------------
// HUNT.LIB
// ---------------------------------------------------------------------------
void trail_hunt_intro(Shell *sh) {
    Mach *m = sh->m;
    amp_wind(m, ARG_NONE);
    sh_print(sh, CL CR "Hunting Instructions" CC CR);
    amp_hsp(m, 1, ARG_NONE);
    amp_dfw(m, 7, 20, 30, 120, 151);
    amp_wind(m, 7);
    sh_print(sh, "Return Key" CR CR CR "Arrow Keys" CR CR CR CR CR CR CR CR "Space Bar" CR);
    amp_dfw(m, 7, 110, 30, 169, 151);                            // 50005
    amp_wind(m, 7);
    sh_print(sh, "To start or stop walking" CR CR CR);
    sh_print(sh, "To point the rifle" CR "(novice hunters)" CR CR CR "To point the rifle" CR
                 "(expert hunters)" CR CR CR "To fire the rifle" CR);
    amp_dft(m, 7, sh->sa);
    amp_image(m, 6, 1, 145);
    amp_hsp(m, 2, ARG_NONE);                                     // 50010
    amp_image(m, 1, 30, 78);
    sh_space(sh);
}

void trail_hunt_result(Shell *sh, int flags, int l, int shot) {
    Mach *m = sh->m;
    Game *g = sh->g;
    amp_dfw(m, 7, 20, 40, 220, 151);                             // 50011
    sh_print(sh, CL CR);
    amp_wind(m, 7);
    (void)g;
    if (flags & HUNT_GOT)
        sh_printf(sh, "From the animals you shot, you got %d pound%s of meat.  ", shot, shot == 1 ? "" : "s");
    if (flags & HUNT_FULL) sh_print(sh, "However, your wagon is full." CR);
    if (flags & HUNT_ROOM) sh_printf(sh, "However, your wagon will only hold another %d pounds of food." CR, l);
    if (flags & HUNT_CARRY100) sh_print(sh, "However, you were only able to carry 100 pounds back to the wagon." CR);
    if (flags & HUNT_NONE) sh_print(sh, "You were unable to shoot any food." CR);
    sh_print(sh, CR);                                            // 50020
    amp_image(m, 6, 1, 145);
    sh_space(sh);
    cst(sh);
}

// ---------------------------------------------------------------------------
// Gravesites, broken parts, deaths
// ---------------------------------------------------------------------------
int trail_grave_ask(Shell *sh) {                                 // 10400
    sh_box(sh, 0, 60, 279, 90);
    sh_print(sh, "You pass a gravesite.  Would you like to look closer? ");
    int yes = sh_yesno(sh) == 'Y';
    sh_restore(sh);
    return yes;
}

void trail_grave_show(Shell *sh, const char *name, const char *epitaph) {   // TOMB.LIB:50100
    Mach *m = sh->m;
    amp_dun(m, "TS.PCK", ARG_NONE, ARG_NONE);
    amp_dfw(m, 7, 56, 62, 150, 45);
    inverse(sh);
    amp_wind(m, 7);
    sh_printf(sh, "Here lies" CC CR "%s" CC CR CR "%s" CR, name, epitaph);
    normal(sh);
    sh_box_done(sh);
    cst(sh);
}

int trail_part_ask(Shell *sh, int part) {                        // PART.LIB:42000
    Mach *m = sh->m;
    s_art_i = 8;                                                 // 10800: I = 8
    amp_wind(m, ARG_NONE);                                       // PART.LIB:42000
    amp_image(m, 4, 185, 29);
    amp_box(m, 0, Y5, 278, Y5 + 9, 3);
    amp_box(m, 7, 66, 272, 97, ARG_NONE);
    amp_dfw(m, 7, 15, 68, 250, 27);
    amp_wind(m, 7);
    sh_printf(sh, "Broken wagon %s.  Would you like to try to repair it? ", STORE_UNIT[part - 2]);
    int yes = sh_yesno(sh) == 'Y';
    amp_box(m, ARG_NONE, ARG_NONE, ARG_NONE, ARG_NONE, ARG_NONE);
    return yes;
}

int trail_part_msg(Shell *sh, int kind, const char *text) {
    Mach *m = sh->m;
    amp_co(m, ARG_NONE, ARG_NONE);
    sh_print(sh, CF);
    sh_printf(sh, "%s" CR, text);
    if (kind == PART_MSG_NO_SPARE || kind == PART_MSG_TRADE) {
        sh_space(sh);
        return 0;
    }
    hint400(sh);                                                 // 42100
    sh_space(sh);
    return (m->kbd & 0x7F) == 13;
}

void trail_no_oxen(Shell *sh) {                                  // 21000
    sh_box(sh, 21, 70, 245, 110);
    sh_print(sh, "You are unable to continue your journey.  You have no oxen to pull the wagon." CR);
    sh_box_done(sh);
    sh_restore(sh);
}

// TOMB.LIB:50010-50030: the last of the party.
void trail_tomb_final(Shell *sh, const char *leader, char *epitaph, int max) {
    Mach *m = sh->m;
    amp_dun(m, "TS.PCK", ARG_NONE, ARG_NONE);
    amp_rfl(m, "TS.BIN", 51944);
    amp_dfw(m, 6, 56, 62, 150, 45);
    amp_wind(m, 6);
    inverse(sh);
    sh_printf(sh, "Here lies" CC CR "%s" CC CR CR, leader);
    int x, y;
    amp_gcp(m, &x, &y);
    normal(sh);
    sh_space_tune(sh);
    amp_dfw(m, 7, 0, 163, 279, 28);
    amp_wind(m, 7);
    sh_print(sh, CF "Would you like to write" CR "an epitaph? ");
    epitaph[0] = 0;
    if (sh_yesno(sh) == 'N') return;
    for (;;) {
        amp_wind(m, 7);                                          // 50020
        sh_print(sh, CF "What would you like on the tombstone? " CR);
        sh_inp(sh, 29, "-09-AZ-az ,.'-", 0, epitaph, max);
        if (!epitaph[0]) return;
        inverse(sh);                                             // 50030
        amp_wind(m, 6);
        amp_co(m, ARG_NONE, y);
        sh_printf(sh, CF "%s" CR, epitaph);
        normal(sh);
        amp_wind(m, 7);
        sh_print(sh, CF "Would you like to make" CR "changes? ");
        if (sh_yesno(sh) != 'Y') return;
    }
}

void trail_all_dead(Shell *sh) {                                 // 50040
    amp_wind(sh->m, 7);
    sh_print(sh, CF "All of the people" CC CR "in your party have died." CC CR);
    sh_space_tune(sh);
}

void trail_flip(Shell *sh) {                                     // FLIP.LIB:50030
    amp_rfl(sh->m, "MOUNTAINS.IMA", 49152);
}
