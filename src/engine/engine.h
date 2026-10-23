// Internal helpers shared by the engine's program files (not for the host).
#ifndef OT_ENGINE_H
#define OT_ENGINE_H

#include "game.h"
#include "data.h"
#include "a2num.h"
#include "a2str.h"

// Ask the host: ASK(g, .kind = REQ_X, .a = 1) returns the host's answer.
int ot_ask(Game *g, Req *r);
#define ASK(g, ...) ot_ask((g), &(Req){ __VA_ARGS__ })
#define GOSUB(g, line) ASK((g), .kind = REQ_GOSUB, .a = (line))

// Applesoft helpers.
#define RND(g) ot_rnd(g)
double a2_val(const char *s);                           // VAL
void poke(Game *g, int addr, double v, int line);       // POKE, ?ILLEGAL QUANTITY -> error 53
int  peek(const Game *g, int addr);
void a2_error(Game *g, int err, int line, const char *program);   // COMMON.LIB:32000 -> menu
void game_over(Game *g, Outcome o);                     // & RNH,"MENU" from any depth

// Tombstones (TOMB.LIB, FLIP.LIB, OREGON TRAIL:29004).
void tomb_read_segments(Game *g);                       // SN(), ML() from side S
void tomb_read(Game *g, int rec, char *name, char *epitaph);
void tomb_write(Game *g, const char *epitaph);

// OREGON TRAIL routines used across files.
void trail_run(Game *g);                                // OREGON TRAIL:110 onward
void rest500(Game *g);                                  // 500: run SD rest days (events)
void rest4505(Game *g);                                 // 4505: run SD rest days (menu, ferry)
void death(Game *g, int q);                             // TOMB.LIB:50000 (+ 650 via 8000)
void list_box(Game *g, const char *text);               // 900
void loss_items(Game *g, double v);       // 50205
void loss_oxen(Game *g, double v);                      // 50185
void loss_drown(Game *g, double v);                     // 50175

// Other programs.
void menu_new_game(Game *g);                            // MENU:9000
void buy_supplies(Game *g);                             // BUY SUPPLIES
void river_run(Game *g);                                // RIVER.LIB:50000
void end_lib(Game *g);                                  // END.LIB:50000
void end_handoff(Game *g);                              // END.LIB:50050-50080
void float_run(Game *g);                                // FLOAT
void win_run(Game *g);                                  // WIN

#endif
