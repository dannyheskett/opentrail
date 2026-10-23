// BUY SUPPLIES: the departure month and Matt's General Store (display side).
// The engine keeps the quantities and the bill (game.c: buy_supplies).
#include "screen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const M[6] = { "February", "March", "April", "May", "June", "July" };
static const char *const SP[3] = { "wagon wheel", "wagon axle", "wagon tongue" };
static const char *const ITEMS[6] = { "Covered wagon", "Oxen", "Food", "Clothing",
                                      "Ammunition", "Spare parts" };

static char s_tb[24];           // TB$: "Bill so far"
static int s_parts_intro;       // 800's text is up for the part prompts

// 200: V -> "12.34"
static const char *money(long cents, char *buf) { return a2_money(cents, buf); }

// 910-950: the cost of item L in cents.
static long item_cost(const Matt *mt, int l) {
    switch (l) {
    case 1: return 4000L * mt->yokes;
    case 2: return 20L * mt->food;
    case 3: return 1000L * mt->clothing;
    case 4: return 200L * mt->boxes;
    case 5: return 1000L * (mt->parts[0] + mt->parts[1] + mt->parts[2]);
    }
    return 0;
}

// 6000-6110
int buy_month(Shell *sh) {
    Mach *m = sh->m;
    for (;;) {
        amp_dft(m, 7, 49152);
        sh_print(sh, CL CR);
        amp_image(m, 3, 10, 0);
        amp_image(m, 3, 10, 177);
        amp_dfw(m, 1, 15, 19, 260, 172);
        amp_wind(m, 1);
        sh_printf(sh, CR "It is 18%d.  Your jumping off place for Oregon is Independence, Missouri.  "
                  "You must decide which month to leave Independence." CR CR CR, PEEK(sh->g, 901));
        for (int l = 1; l <= 5; l++) sh_printf(sh, "   %d. %s" CR, l, M[l]);
        sh_print(sh, "   6. Ask for advice" CR CR CR "What is your choice? ");
        char z[4];
        sh_inp(sh, 1, "-16", 1, z, sizeof z);
        int c = atoi(z);
        if (c != 6) return c;
        amp_wind(m, 0);                                          // 6100
        amp_box(m, 0, 19, 279, 191, ARG_NONE);
        amp_image(m, 3, 10, 164);
        amp_dfw(m, 1, 15, 25, 255, 172);
        amp_wind(m, 1);
        sh_print(sh, "You attend a public meeting held for \"folks with the California - Oregon "
                     "fever.\"  You're told:" CR CR "If you leave too early, there won't be any "
                     "grass for your oxen to eat.  ");
        sh_print(sh, "If you leave too late, you may not get to Oregon before winter comes.  If "
                     "you leave at just the right time, there will be green grass and the weather "
                     "will still be cool." CR);
        sh_space(sh);
    }
}

// 1005 + 2000-2030
void buy_intro(Shell *sh, long my_cents) {
    Mach *m = sh->m;
    char k[24];
    amp_dun(m, "L0.PCK", sh->pa, 16);
    amp_cst(m, sh->st);
    sh_box(sh, 21, 84, 256, 140);
    sh_printf(sh, "Before leaving Independence, you should buy equipment and supplies.  You have "
              "$%s in cash, but you don't have to spend it all now." CR, money(my_cents, k));
    sh_box_done(sh);
    sh_restore(sh);
    sh_box(sh, 14, 112, sh->x2, sh->y2);       // 2000: Y1 = 112: X1 = 14, X2 and Y2 kept
    sh_print(sh, "You can buy whatever you need at Matt's General Store." CR);
    sh_box_done(sh);
    amp_wind(m, 0);                                              // 2010
    sh_print(sh, CL CR CR "Hello, I'm Matt.  So you're going to Oregon!  I can fix you up with what you need:" CR);
    amp_image(m, 4, 0, 50);                                      // 2020
    amp_dfw(m, 1, 70, 60, 200, 131);
    amp_wind(m, 1);
    amp_in(m, -16);
    sh_print(sh, CR CR "- a team of oxen to pull your wagon" CR CR "- clothing for both summer and winter" CR);
    sh_space(sh);
    amp_box(m, ARG_NONE, ARG_NONE, ARG_NONE, ARG_NONE, ARG_NONE);
    amp_co(m, ARG_NONE, ARG_NONE);                               // 2030
    sh_print(sh, "- plenty of food for the trip" CR CR "- ammunition for your rifles" CR CR
                 "- spare parts for your wagon" CR);
    amp_in(m, ARG_NONE);
    sh_space(sh);
    amp_dft(m, 7, 37380);                                        // 1005
    amp_wind(m, ARG_NONE);
    amp_box(m, 0, 0, 279, 45, ARG_NONE);
    amp_box(m, 50, 0, 279, 191, ARG_NONE);
}

// "Bill so far" box (1010, 510, 811)
static void bill_so_far(Shell *sh, int x, int y) {
    amp_co(sh->m, x, y);
    sh_printf(sh, "Bill so far: %s" CR, s_tb);
}

// 3000-3030 and the 1010 display for the chosen item.
int buy_menu(Shell *sh, const Matt *mt, int change) {
    Mach *m = sh->m;
    Game *g = sh->g;
    char z[24], k[24];
    s_parts_intro = 0;
    snprintf(s_tb, sizeof s_tb, "$%s", money(mt->bill_cents, z));    // 4000: TB$
    amp_wind(m, ARG_NONE);                                       // 3000
    amp_box(m, 50, 0, 279, 191, ARG_NONE);
    amp_dfw(m, 1, 60, 10, 219, 181);
    amp_box(m, 50, 5, 270, 7, 1);
    amp_box(m, 50, 47, 270, 49, 1);
    amp_box(m, 50, 100, 270, 102, 1);
    amp_wind(m, 1);
    sh_print(sh, "  Matt's General Store" CR "  Independence, Missouri" CR CR);   // 3010
    int zx, zy;
    amp_gcp(m, &zx, &zy);
    char date[32];
    snprintf(date, sizeof date, "%s %d, 18%d", M[PEEK(g, 902) - 2], PEEK(g, 903), PEEK(g, 901));
    amp_co(m, 199 - (int)strlen(date) * 8, zy);
    sh_printf(sh, "%s" CR CR, date);
    amp_gcp(m, &zx, &zy);                                        // 3015
    amp_co(m, zx, zy - 2);
    for (int l = 1; l <= 5; l++) {
        char v[24], line[24];
        snprintf(v, sizeof v, "   $%s", money(item_cost(mt, l), z));
        snprintf(line, sizeof line, "%s", a2_right(v, 8));    // RIGHT$("   $" + Z$, 8)
        sh_printf(sh, "%d. %s", l, ITEMS[l]);
        amp_gcp(m, &zx, &zy);
        sh_print(sh, CR);
        amp_co(m, 135, zy);
        sh_printf(sh, "%s" CR, line);
    }
    amp_wind(m, ARG_NONE);                                       // 3020
    amp_co(m, 111, 106);
    {
        char t[32];
        snprintf(t, sizeof t, "   %s", s_tb);
        sh_printf(sh, "Total bill: %s" CR, a2_right(t, 8));
        snprintf(t, sizeof t, "    $%s", money(g->my_cents, z));
        snprintf(k, sizeof k, "%s", a2_right(t, 8));
    }
    amp_co(m, 61, 135);
    sh_printf(sh, "Amount you have: %s" CR, k);
    amp_dfw(m, 1, 70, 150, 180, 41);                             // 3030
    amp_wind(m, 1);
    amp_co(m, 0, 23);
    sh_print(sh, "Press SPACE BAR to leave store" CR);
    usr(m, 1);
    amp_co(m, ARG_NONE, ARG_NONE);
    sh_printf(sh, "Which item would you like to %s? ", change ? "change" : "buy");
    // 250-255: a one-key field: 1-5 then Return, or the space bar at once
    amp_gcp(m, &zx, &zy);
    int key;
    for (;;) {
        amp_co(m, zx, zy);
        amp_cel(m);
        sh_print(sh, "\x7F" CR);
        key = usr(m, 5) - 128;
        if (key == 32) break;
        if (key < 49 || key > 53) continue;
        amp_co(m, zx, zy);
        sh_printf(sh, "%c\x7F" CR, key);
        int k2;
        do k2 = usr(m, 5) - 128; while (k2 != 127 && k2 != 8 && k2 != 13);
        if (k2 == 13) break;
    }
    sh_print(sh, CR);
    int c = key == 32 ? 0 : key - '0';
    if (c) {                                                     // 1010
        amp_wind(m, ARG_NONE);
        long v = mt->bill_cents - item_cost(mt, c);
        snprintf(s_tb, sizeof s_tb, "$%s", money(v, z));
        amp_box(m, 50, 35, 279, 191, ARG_NONE);
        amp_box(m, 50, 29, 270, 31, 1);
        amp_dfw(m, 1, 60, 40, 219, 151);
        amp_wind(m, 1);
        bill_so_far(sh, 0, 142);
        amp_co(m, ARG_NONE, ARG_NONE);
    }
    return c;
}

// 400-815: the item's prompt; returns the quantity.
int buy_qty(Shell *sh, int item, int part) {
    Mach *m = sh->m;
    char z[8];
    switch (item) {
    case 1:
        amp_image(m, 8, 150, 140);
        sh_print(sh, CR CR "There are 2 oxen in a yoke; I recommend at least 3 yoke.  I charge $40 a yoke." CR CR
                     "How many yoke do you" CR "want? ");
        sh_inp(sh, 1, "-19", 1, z, sizeof z);
        return atoi(z);
    case 2:
        amp_image(m, 2, 150, 140);
        sh_print(sh, "I recommend you take at least 200 pounds of food for each person in your family.  "
                     "I see that you have 5 people in all.  You'll need flour, sugar, bacon, and coffee.  "
                     "My price is 20 cents a pound." CR CR);
        sh_print(sh, "How many pounds of food do you want? ");
        sh_inp(sh, 4, "-09", 1, z, sizeof z);
        return atoi(z);
    case 3:
        amp_image(m, 5, 146, 140);
        sh_print(sh, CR "You'll need warm clothing in the mountains.  I recommend taking at least 2 sets of "
                     "clothes per person.  Each set is $10.00." CR);
        sh_print(sh, CR "How many sets of clothes do you want? ");
        sh_inp(sh, 2, "-09", 1, z, sizeof z);
        return atoi(z);
    case 4:
        sh_print(sh, CR CR CR);
        amp_image(m, 6, 149, 140);
        sh_print(sh, "I sell ammunition in boxes of 20 bullets.  Each box costs $2.00." CR CR
                     "How many boxes do" CR "you want? ");
        sh_inp(sh, 2, "-09", 1, z, sizeof z);
        return atoi(z);
    default:
        if (!s_parts_intro) {                                    // 800
            s_parts_intro = 1;
            amp_image(m, 3, 150, 133);
            sh_print(sh, "It's a good idea to have a few spare parts for your wagon.  Here are the prices:" CR);
            amp_in(m, 8);
            sh_print(sh, CR);
            for (int l = 0; l <= 2; l++) {
                int zx, zy;
                sh_printf(sh, " %s", SP[l]);
                amp_gcp(m, &zx, &zy);
                sh_print(sh, CR);
                amp_co(m, 109, zy);
                sh_print(sh, "- $10 each" CR);
            }
            amp_in(m, ARG_NONE);
            amp_dfw(m, 1, 60, 114, 219, 18);                     // 810
            amp_wind(m, 1);
        }
        amp_box(m, ARG_NONE, ARG_NONE, ARG_NONE, ARG_NONE, ARG_NONE);
        amp_co(m, ARG_NONE, ARG_NONE);
        sh_printf(sh, "How many %ss? ", SP[part]);
        sh_inp(sh, 1, "-09", 1, z, sizeof z);
        return atoi(z);
    }
}

void buy_msg(Shell *sh, int msg, int part) {
    Mach *m = sh->m;
    char a[24], b[24];
    switch (msg) {
    case MATT_MSG_FOOD_LIMIT:                                    // 510
        amp_box(m, ARG_NONE, ARG_NONE, ARG_NONE, ARG_NONE, ARG_NONE);
        amp_co(m, ARG_NONE, ARG_NONE);
        sh_print(sh, CR CR "Your wagon may only carry 2000 pounds of food." CR);
        sh_space(sh);
        amp_box(m, ARG_NONE, ARG_NONE, ARG_NONE, ARG_NONE, ARG_NONE);
        bill_so_far(sh, 0, 142);
        amp_co(m, ARG_NONE, ARG_NONE);                           // & CO: GOTO 500
        break;
    case MATT_MSG_PART_LIMIT:                                    // 811
        amp_co(m, ARG_NONE, ARG_NONE);
        sh_print(sh, CF);
        sh_printf(sh, "Your wagon may only carry 3 %ss." CR, SP[part]);
        amp_wind(m, 0);
        sh_box_done(sh);
        bill_so_far(sh, 60, 184);
        amp_wind(m, 1);
        amp_box(m, ARG_NONE, ARG_NONE, ARG_NONE, ARG_NONE, ARG_NONE);
        break;
    case MATT_MSG_OVER_BUDGET:                                   // 5000
        amp_dfw(m, 1, 60, 130, 219, 61);
        amp_wind(m, 1);
        sh_print(sh, CF);
        snprintf(b, sizeof b, "$%s", money(sh->g->matt.bill_cents, a));
        sh_printf(sh, "Okay, that comes to a total of %s.  But I see that you only have $%s.  "
                  "We'd better go over the list again." CR, b, money(sh->g->my_cents, a));
        sh_box_done(sh);
        amp_wind(m, 1);
        amp_box(m, ARG_NONE, ARG_NONE, ARG_NONE, ARG_NONE, ARG_NONE);
        break;
    case MATT_MSG_NO_OXEN:                                       // 5006
        amp_dfw(m, 1, 60, 130, 219, 61);
        amp_wind(m, 1);
        sh_print(sh, CF);
        sh_print(sh, CF "Don't forget, you'll need oxen to pull your wagon." CR);
        sh_box_done(sh);
        amp_wind(m, 1);
        amp_box(m, ARG_NONE, ARG_NONE, ARG_NONE, ARG_NONE, ARG_NONE);
        break;
    }
}

// 5010-5020
void buy_depart(Shell *sh) {
    Mach *m = sh->m;
    amp_wind(m, 0);
    amp_box(m, 50, 0, 279, 191, ARG_NONE);
    amp_dfw(m, 1, 60, 60, 190, 131);
    amp_wind(m, 1);
    sh_print(sh, "Well then, you're ready to start.  Good luck!  You have a long and difficult "
                 "journey ahead of you." CR);
    sh_space(sh);
    amp_dun(m, "L0.PCK", sh->pa, 16);
    amp_cst(m, sh->st);
    sh_box(sh, 42, 160, 231, 180);
    sh_print(sh, "Now loading the wagon..." CR);
    sh_loading(sh);
    amp_rfl(m, "EVENTS.IMA", 52200);
    amp_rfl(m, "MS0.BIN", 51944);
    amp_rfl(m, "PRAIRIE.IMA", 49152);
    // & RNH,"OREGON TRAIL": RUN clears its variables (the leg's icon NI is
    // 0 until 3000 sets it, so the first scene unpacks image 0), then its
    // windows (29000, 29004)
    sh->ni = sh->wg = sh->ix = sh->fx = 0;
    memset(sh->ia, 0, sizeof sh->ia);
    sh->cc = sh->goff = 0;
    sh->x1 = sh->y1 = sh->x2 = sh->y2 = 0;
    sh->td[0] = 0;
    amp_dfw(m, 4, 80, 30, 199, 161);
    amp_dfw(m, 3, 28, 100, 210, 37);
    amp_dfw(m, 2, 0, 25, 279, 45);
    amp_dfw(m, 1, 0, 73, 279, 118);
    amp_dfw(m, 5, 135, 125, 144, 57);
}
