// The engine's io() callback: every Req runs the matching display code.
#include "screen.h"
#include "hunt.h"
#include <stdio.h>
#include <string.h>

// menu.c / buy.c / trailscr.c / endscr.c
int  menu_occupation(Shell *sh);
void menu_names(Shell *sh, char n[5][OT_NAME_MAX]);
void menu_going_back(Shell *sh);
int  buy_month(Shell *sh);
void buy_intro(Shell *sh, long my_cents);
int  buy_menu(Shell *sh, const Matt *mt, int change);
int  buy_qty(Shell *sh, int item, int part);
void buy_msg(Shell *sh, int msg, int part);
void buy_depart(Shell *sh);
void trail_gosub(Shell *sh, int line);
void trail_day_end(Shell *sh, int travel);
int  trail_look_around(Shell *sh);
int  trail_split(Shell *sh);
void trail_leg(Shell *sh);
void trail_event_show(Shell *sh, const Req *r);
int  trail_event_end(Shell *sh, const Req *r);
int  trail_menu(Shell *sh, int first);
void trail_must_trade(Shell *sh, int b);
void trail_supplies(Shell *sh);
void trail_map(Shell *sh);
int  trail_pace(Shell *sh);
int  trail_rations(Shell *sh);
int  trail_rest_ask(Shell *sh);
void trail_rest_date(Shell *sh, int row);
void trail_talk(Shell *sh, int speaker);
int  trail_store_menu(Shell *sh, int first);
int  trail_store_qty(Shell *sh, int l);
void trail_store_msg(Shell *sh, int msg, int l);
void trail_trade_show(Shell *sh, int refresh);
void trail_trade_nobody(Shell *sh);
void trail_trade_lack(Shell *sh, const char *want);
int  trail_trade_offer(Shell *sh, const char *want, const char *offer, int she);
void trail_hunt_intro(Shell *sh);
void trail_hunt_result(Shell *sh, int flags, int l, int shot);
int  trail_grave_ask(Shell *sh);
void trail_grave_show(Shell *sh, const char *name, const char *epitaph);
int  trail_part_ask(Shell *sh, int part);
int  trail_part_msg(Shell *sh, int kind, const char *text);
void trail_no_oxen(Shell *sh);
void trail_tomb_final(Shell *sh, const char *leader, char *epitaph, int max);
void trail_all_dead(Shell *sh);
void trail_flip(Shell *sh);
void river_intro(Shell *sh);
int  river_menu(Shell *sh, int extra);
void river_info(Shell *sh);
void river_camp(Shell *sh);
void river_camp_date(Shell *sh);
void river_msg(Shell *sh, int msg);
int  river_ferry_offer(Shell *sh, int days);
int  river_guide_offer(Shell *sh, int sets, int has);
void river_guide_go(Shell *sh, int how);
void river_cross(Shell *sh, int method, int losses);
int  end_dalles(Shell *sh, int first);
int  end_barlow_ask(Shell *sh, double toll);
void end_barlow_no_cash(Shell *sh);
void float_intro(Shell *sh);
int  float_frame(Shell *sh);
void float_hit(Shell *sh, const Req *r);
void float_land(Shell *sh);
void win_arrive(Shell *sh);
void win_topten_show(Shell *sh, int qualified);
void win_topten_name(Shell *sh, int row, const TopTen *hi, char *out, int max);
void persist_save(const Persist *p, int which);                 // main.c

// COMMON.LIB:32120, then 31000.
static void show_error(Shell *sh, const Req *r) {
    Mach *m = sh->m;
    m->invflg = 0xFF;
    amp_wind(m, ARG_NONE);
    sh_print(sh, CL CR);
    amp_co(m, ARG_NONE, 80);
    sh_printf(sh, "Error %d at line #%d in %s." CR CR "Please report this error to MECC." CR, r->a, r->b, r->text);
    sh_space(sh);
}

int screens_io(void *ctx, Game *g, Req *r) {
    Shell *sh = ctx;
    sh->g = g;
    switch (r->kind) {
    case REQ_GOSUB: trail_gosub(sh, r->a); return 0;

    // MENU / BUY SUPPLIES
    case REQ_OCCUPATION: return menu_occupation(sh);
    case REQ_NAMES: menu_names(sh, r->names); menu_going_back(sh); return 0;
    case REQ_MONTH: sh->common = 0; return buy_month(sh);
    case REQ_MATT_INTRO: buy_intro(sh, (long)(r->x * 100 + .5)); return 0;
    case REQ_MATT_MENU: return buy_menu(sh, &g->matt, r->a);
    case REQ_MATT_QTY: return buy_qty(sh, r->a, r->b);
    case REQ_MATT_MSG: buy_msg(sh, r->a, r->b); return 0;
    case REQ_MATT_DEPART: buy_depart(sh); return 0;

    // OREGON TRAIL
    case REQ_LOOK_AROUND: return trail_look_around(sh);
    case REQ_ACTION_MENU: return trail_menu(sh, r->a);
    case REQ_MUST_TRADE: trail_must_trade(sh, r->a); return 0;
    case REQ_SUPPLIES: trail_supplies(sh); return 0;
    case REQ_MAP: trail_map(sh); return 0;
    case REQ_PACE: return trail_pace(sh);
    case REQ_RATIONS: return trail_rations(sh);
    case REQ_REST_ASK: return trail_rest_ask(sh);
    case REQ_REST_DATE: trail_rest_date(sh, r->a); return 0;
    case REQ_TALK: trail_talk(sh, r->a); return 0;
    case REQ_STORE_MENU: return trail_store_menu(sh, r->a);
    case REQ_STORE_QTY: return trail_store_qty(sh, r->a);
    case REQ_STORE_MSG: trail_store_msg(sh, r->a, r->b); return 0;
    case REQ_TRADE_SHOW: trail_trade_show(sh, r->a); return 0;
    case REQ_TRADE_NOBODY: trail_trade_nobody(sh); return 0;
    case REQ_TRADE_LACK: trail_trade_lack(sh, r->text); return 0;
    case REQ_TRADE_OFFER: return trail_trade_offer(sh, r->text, r->text2, r->a);
    case REQ_HUNT: {                                            // HUNT.LIB:50000-50011
        trail_hunt_intro(sh);
        int lb, left;
        if (hunt_play(sh->m, r->a, r->b & 1, (r->b >> 1) & 1, (r->b >> 2) & 1, 1, r->c, &lb, &left) < 0)
            return -1;                                          // Esc Esc: GOTO 31000
        r->a = lb;
        r->b = left;
        return 0;
    }
    case REQ_HUNT_RESULT: trail_hunt_result(sh, r->a, r->b, r->c); return 0;
    case REQ_SPLIT: return trail_split(sh);
    case REQ_LEG: trail_leg(sh); return 0;
    case REQ_POLL: return usr(sh->m, 3) == 141;
    case REQ_DAY_END: trail_day_end(sh, r->a); return 0;
    case REQ_EVENT_SHOW: trail_event_show(sh, r); return 0;
    case REQ_EVENT_END: return trail_event_end(sh, r);
    case REQ_GRAVE_ASK: return trail_grave_ask(sh);
    case REQ_GRAVE_SHOW: trail_grave_show(sh, r->text, r->text2); return 0;
    case REQ_PART_ASK: return trail_part_ask(sh, r->a);
    case REQ_PART_MSG: return trail_part_msg(sh, r->a, r->text);
    case REQ_NO_OXEN: trail_no_oxen(sh); return 0;
    case REQ_TOMB_FINAL: trail_tomb_final(sh, r->text, r->out, r->outlen); return 0;
    case REQ_ALL_DEAD: trail_all_dead(sh); return 0;
    case REQ_FLIP: trail_flip(sh); return 0;
    case REQ_SAVE: persist_save(sh->persist, r->a); return 0;
    case REQ_ERROR: show_error(sh, r); return 0;

    // RIVER.LIB / CROSS.LIB
    case REQ_RIVER_INTRO: river_intro(sh); return 0;
    case REQ_RIVER_MENU: return river_menu(sh, r->a);
    case REQ_RIVER_INFO: river_info(sh); return 0;
    case REQ_RIVER_CAMP: river_camp(sh); return 0;
    case REQ_RIVER_CAMP_DATE: river_camp_date(sh); return 0;
    case REQ_RIVER_MSG: river_msg(sh, r->a); return 0;
    case REQ_FERRY_OFFER: return river_ferry_offer(sh, r->a);
    case REQ_GUIDE_OFFER: return river_guide_offer(sh, r->a, r->b);
    case REQ_GUIDE_GO: river_guide_go(sh, r->a); return 0;
    case REQ_CROSS: river_cross(sh, r->a, r->b); return 0;

    // END.LIB / FLOAT / WIN
    case REQ_DALLES: return end_dalles(sh, r->a);
    case REQ_BARLOW_ASK: return end_barlow_ask(sh, r->x);
    case REQ_BARLOW_PAID: sh_print(sh, CL CR); return 0;
    case REQ_BARLOW_NO_CASH: end_barlow_no_cash(sh); return 0;
    case REQ_RAFT_INTRO: float_intro(sh); return 0;
    case REQ_RAFT_FRAME: return float_frame(sh);
    case REQ_RAFT_HIT: float_hit(sh, r); return 0;
    case REQ_RAFT_LAND: float_land(sh); return 0;
    case REQ_ARRIVE: win_arrive(sh); return 0;
    case REQ_TOPTEN_SHOW: win_topten_show(sh, r->a); return 0;
    case REQ_TOPTEN_NAME: win_topten_name(sh, r->a, r->data, r->out, r->outlen); return 1;
    }
    return 0;
}
