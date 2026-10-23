// The Oregon Trail (MECC, Apple II, 1985) rules engine.
//
// A line-for-line port of the original Applesoft BASIC (docs/OREGON-TRAIL-SPEC.md
// cites every rule by program and line; the code comments cite the same lines).
// The engine owns the game loop, exactly as the BASIC did: whenever the original
// drew a screen or waited for the player, the engine calls the host's io()
// callback with a Req describing what is needed, and continues with the answer.
// The engine draws nothing and knows nothing about raylib, files, or time.
//
// State fields keep the BASIC variable names (in comments) so the port can be
// read side by side with legacy/extracted/*.bas.
#ifndef OT_GAME_H
#define OT_GAME_H

#include <stdbool.h>
#include <setjmp.h>

#define OT_NAME_MAX 16      // names are <= 9 chars (MENU:6015), Top Ten <= 15
#define OT_EPITAPH_MAX 32   // epitaph <= 29 chars (TOMB.LIB:50020)
#define OT_LINE_MAX 48      // one loss line, "Zeke (drowned)", "1700 pounds of food"
#define OT_LOSS_MAX 12      // T$(0..10), drowning stops at 11 lines

// Inventory slots, I(0..8) in the BASIC.
enum { INV_WAGON = 1, INV_OXEN = 2, INV_CLOTHES = 3, INV_BULLETS = 4,
       INV_WHEELS = 5, INV_AXLES = 6, INV_TONGUES = 7, INV_FOOD = 8, INV_COUNT = 9 };

// ---------------------------------------------------------------------------
// Persistent disk data: raw images of the TOMB.SEQ files (one per disk side)
// and HISCORE.SEQ, read and written field by field exactly as the game's
// & TREED / & TWRITE did, so record overflow (spec REQ-358) behaves the same.
// The host loads these before play and saves them when the engine sends REQ_SAVE.
// ---------------------------------------------------------------------------
#define SEQ_SIZE 1024
typedef struct {
    unsigned char tomb[2][SEQ_SIZE];
    unsigned char hiscore[SEQ_SIZE];
} Persist;

typedef char TopTen[10][3][OT_NAME_MAX];    // HI$(0..9, name/points/rating)

void persist_default(Persist *p);           // original Top Ten, tombstones erased
void persist_erase_tombs(Persist *p);       // MANAGEMENT:250
void persist_reset_topten(Persist *p);      // MANAGEMENT:705-720
void topten_read(const Persist *p, TopTen hi);
void topten_write(Persist *p, TopTen hi);   // WIN:535-540

// ---------------------------------------------------------------------------
// Requests from the engine to the host. Each names the BASIC lines it stands
// for; the host draws exactly that and returns the player's answer.
// In/out fields: a, b, c, x are inputs unless a kind says otherwise.
// ---------------------------------------------------------------------------
typedef enum {
    // Purely visual BASIC subroutines and display-state points, a = the line:
    // 300 travel/landmark scene, 350 clear message strip, 355 clear leg strip,
    // 400 "Press RETURN" hint line, 950 "Press SPACE BAR" + tune;
    // 500 INVERSE before a rest day, 3180 before an event (& WIND: NORMAL:
    // & DFT,7,52200), 3181 after it (INVERSE), 3190 after the day's events
    // (& CST: & DFT,7,49152: & WIND), 3235 INVERSE after a forced illness,
    // 3499 NORMAL at the end of GOSUB 3100; 3000 the leg's X = USR(1), which
    // clears a Return still latched from "Press SPACE BAR" before travel polls.
    REQ_GOSUB,

    // ---- MENU / BUY SUPPLIES (new game) ----
    REQ_OCCUPATION,         // MENU:4000-4030      -> 1..3
    REQ_NAMES,              // MENU:6015-6045      names[] hold defaults; host edits in place
    REQ_MONTH,              // BUY SUPPLIES:6000   -> choice 1..5 (March..July)
    REQ_MATT_INTRO,         // BUY SUPPLIES:2000-2030
    REQ_MATT_MENU,          // BUY SUPPLIES:3000   -> 0 leave (space bar), 1..5 item; a = 1 after a refused leave ("change")
    REQ_MATT_QTY,           // item a (1..5), part b (0..2 for item 5) -> quantity
    REQ_MATT_MSG,           // a = MATT_MSG_*
    REQ_MATT_DEPART,        // BUY SUPPLIES:5010-5020

    // ---- OREGON TRAIL main loop ----
    REQ_LOOK_AROUND,        // 1005-1006 arrival box + Y/N          -> 1 yes / 0 no
    REQ_ACTION_MENU,        // 4000-4060, a = 1 on entry (4000)     -> choice as typed (1..9)
    REQ_MUST_TRADE,         // 4070, a = B (2 no oxen, 5..7 part)
    REQ_SUPPLIES,           // 4100-4110
    REQ_MAP,                // 4200 + MAP.LIB
    REQ_PACE,               // PACE.LIB  -> 1..3, 0 = Return (keep)
    REQ_RATIONS,            // RATION.LIB -> 1..3, 0 = keep
    REQ_REST_ASK,           // 4500 -> 0..9
    REQ_REST_DATE,          // 4505 per rest day, a = text row (X5)
    REQ_TALK,               // TALK.LIB, a = speaker 0..2 (landmark LM, side S)
    REQ_STORE_MENU,         // BUY.LIB:50000-50010, a = 1 on entry -> 1..8 (8 leave)
    REQ_STORE_QTY,          // BUY.LIB:50020, a = item 1..7 -> quantity
    REQ_STORE_MSG,          // a = STORE_MSG_*, b = item (1..7)
    REQ_TRADE_SHOW,         // TRADE.LIB:50000 supplies panel (also 50050 refresh when a = 1)
    REQ_TRADE_NOBODY,       // TRADE.LIB:50027
    REQ_TRADE_LACK,         // 50030: text = wanted, e.g. "3 sets of clothing"
    REQ_TRADE_OFFER,        // 50030-50035: text = wanted, text2 = offered, a = 1 She -> 1 accept
    REQ_HUNT,               // HUNT.LIB:50000-50011, a = bullets, b = region flags
                            //   (bit0 LM>3 AND LM<13, bit1 LM>6, bit2 LM<7), c = ZO
                            //   out: a = pounds shot (raw L), b = bullets left
                            //   return -1: Esc Esc, abandon the game (GOTO 31000)
    REQ_HUNT_RESULT,        // 50012-50020, a = HUNT_* flags, b = pounds kept, c = pounds shot (halved)
    REQ_SPLIT,              // 2110-2120 -> 1, 2, 3 (map)
    REQ_LEG,                // 2200 "From A it is n miles to B."
    REQ_POLL,               // 800 poll the keyboard during travel -> 1 if Return
    REQ_DAY_END,            // 3246-3265 wagon step + status panel (a = 1 travel day)
    REQ_EVENT_SHOW,         // event message; see EventStyle in `a`, art in `b`
    REQ_EVENT_END,          // wait for the player -> 1 if Return was the last key
    REQ_GRAVE_ASK,          // 10400 -> 1 look closer
    REQ_GRAVE_SHOW,         // TOMB.LIB:50100, text = name, text2 = epitaph
    REQ_PART_ASK,           // PART.LIB:42000, a = part (5..7) -> 1 try to repair
    REQ_PART_MSG,           // PART.LIB, a = PART_MSG_*, text -> 1 if Return after
    REQ_NO_OXEN,            // 21000
    REQ_TOMB_FINAL,         // TOMB.LIB:50010-50030, text = leader; host fills out (epitaph)
    REQ_ALL_DEAD,           // TOMB.LIB:50040
    REQ_FLIP,               // FLIP.LIB side-2 prompt / Fort Laramie
    REQ_SAVE,               // persistent data changed; a = 0 tombs, 1 Top Ten
    REQ_ERROR,              // COMMON.LIB:32120 "Error a at line #b in text."

    // ---- RIVER.LIB / CROSS.LIB ----
    REQ_RIVER_INTRO,        // 50000-50005
    REQ_RIVER_MENU,         // 50010-50015, a = extra option (0, 1 guide, 2 ferry) -> choice
    REQ_RIVER_INFO,         // 50160-50170
    REQ_RIVER_CAMP,         // 50025 message; the rest day follows, then REQ_RIVER_CAMP_DATE
    REQ_RIVER_CAMP_DATE,
    REQ_RIVER_MSG,          // a = RIVER_MSG_*
    REQ_FERRY_OFFER,        // 50101, a = days, x = cost -> 1 accept
    REQ_GUIDE_OFFER,        // 50130-50136, a = sets, b = 1 if the party has them -> 1 accept
    REQ_GUIDE_GO,           // 50145, a = 1 ford, 2 float
    REQ_CROSS,              // CROSS.LIB, a = I (1 ferry, 2 ford, 3 float), b = losses Z

    // ---- END.LIB / FLOAT / WIN ----
    REQ_DALLES,             // END.LIB:50000-50010, a = 1 on entry -> 1 float, 2 Barlow
    REQ_BARLOW_ASK,         // 50020, x = toll -> 1 yes
    REQ_BARLOW_PAID,        // 50020: paid, PRINT CL$
    REQ_BARLOW_NO_CASH,     // 50030
    REQ_RAFT_INTRO,         // FLOAT:1010-1020
    REQ_RAFT_FRAME,         // one pass of FLOAT:1070-1180 drawn; -> key code (USR(3)), 0 none
    REQ_RAFT_HIT,           // FLOAT:800 message box (text + lines)
    REQ_RAFT_LAND,          // FLOAT:900/960 landing
    REQ_ARRIVE,             // WIN:600-630 arrival and points; score in g->score
    REQ_TOPTEN_NAME,        // WIN:510-530, a = row, data = the shifted TopTen; host fills out (name)
    REQ_TOPTEN_SHOW,        // WIN:2000-2016, a = 1 qualified
} ReqKind;
#define REQ_COUNT (REQ_TOPTEN_SHOW + 1)

typedef enum { MATT_MSG_FOOD_LIMIT, MATT_MSG_PART_LIMIT, MATT_MSG_OVER_BUDGET,
               MATT_MSG_NO_OXEN } MattMsg;
typedef enum { STORE_MSG_AFFORD, STORE_MSG_AFFORD_MANY, STORE_MSG_OXEN_CAP,
               STORE_MSG_FOOD_CAP, STORE_MSG_PART_CAP } StoreMsg;
typedef enum {
    PART_MSG_REPAIRED,      // 42010 "You were able to repair ..." then 42100
    PART_MSG_SPARE,         // 42030 "... You must replace it with a spare part." then 42100
    PART_MSG_NO_SPARE,      // 42030 same text, then a plain wait (42040)
    PART_MSG_TRADE,         // 42040 "Since you don't have a spare ..., you must trade for one."
} PartMsg;
typedef enum { RIVER_MSG_FLOAT_SHALLOW, RIVER_MSG_FERRY_SHALLOW,
               RIVER_MSG_FERRY_NO_MONEY, RIVER_MSG_GUIDE_LACK } RiverMsg;

// Event message styles, one per BASIC display routine.
typedef enum {
    EV_BOX,         // 710: box, wait, restore
    EV_TOAST,       // 700: box left up until the next day (no wait)
    EV_LOSE_DAYS,   // 550: strip message, rest days, then wait
    EV_LIST,        // 900: box with text and lines
} EventStyle;

// Event art (the & IMAGE drawn with the message; EVENTS.IMA numbers).
typedef enum { ART_NONE, ART_SNOW, ART_STORM, ART_BLIZZARD, ART_HAIL, ART_FIRE,
               ART_FRUIT, ART_THIEF } EventArt;

// HUNT.LIB result flags.
enum { HUNT_GOT = 1, HUNT_FULL = 2, HUNT_ROOM = 4, HUNT_CARRY100 = 8, HUNT_NONE = 16 };

typedef struct {
    ReqKind kind;
    int a, b, c;
    double x;
    const char *text, *text2;
    const char (*lines)[OT_LINE_MAX];
    int nlines;
    char *out;                      // text answers (names, epitaph, Top Ten name)
    int outlen;
    char (*names)[OT_NAME_MAX];     // REQ_NAMES
    const void *data;               // REQ_TOPTEN_NAME: TopTen
} Req;

// ---------------------------------------------------------------------------
// Game state
// ---------------------------------------------------------------------------
typedef struct Game Game;
typedef int (*IoFn)(void *ctx, Game *g, Req *r);
typedef double (*RndFn)(void *state);

typedef struct {
    int yokes, food, clothing, boxes, parts[3];     // PEEK(905..912) at Matt's
    long bill_cents;                                // TB
} Matt;

typedef struct {
    int hp, dir, lp, tc, rock, col, nr;             // FLOAT variables
    int fl[4];                                      // FL(0..1) rocks, FL(2) sign, FL(3) trail
    double rx[4], ry[4];
    int sx, sy, tx, ty;                             // sign / landing-trail positions
    int x, y, x1, y1, x2, y2, x3, y3, x4, y4;       // raft and rock boxes
} Raft;

typedef struct {
    long n[8];          // PT(0..7,0): people, wagon, oxen, parts, clothes, bullets, food, -
    double cash;        // PT(7,0), dollars
    long pts[8];        // PT(0..7,1) before the occupation bonus
    long base;          // SC before the bonus
    int occupation;     // PEEK(915), also the multiplier
    int band;           // PEEK(916), health band at The Dalles
    long total;         // SC after the bonus (WIN:625)
    int rating;         // R: 0 Trail guide, 1 Adventurer, 2 Greenhorn
    int row;            // Top Ten row taken, -1 if none
} Score;

typedef enum {
    OUTCOME_NONE,
    OUTCOME_DIED,           // TOMB.LIB:50040 or FLOAT:760
    OUTCOME_ARRIVED,        // WIN ran
    OUTCOME_ABANDONED,      // Esc Esc in the hunt (GOTO 31000)
    OUTCOME_ERROR,          // an Applesoft error the original trapped (COMMON.LIB:32120)
} Outcome;

struct Game {
    // host hooks
    IoFn io; void *io_ctx;
    RndFn rnd; void *rnd_state;
    unsigned long long seed;        // built-in PRNG state when rnd is NULL
    Persist *persist;

    // setup
    int occupation;                 // PEEK(915) 1 banker, 2 carpenter, 3 farmer
    Matt matt;

    // party (4.1)
    int np;                         // NP
    char name[5][OT_NAME_MAX];      // N$
    double h1[5], h2[5];            // H1, H2
    double inv[INV_COUNT];          // I(0..8)
    double pf;                      // PF
    long my_cents;                  // MY, kept exactly in cents (every price is a multiple of 5 cents)
    int b;                          // B

    // travel and time (4.2)
    int lm, nm;                     // LM, NM
    double d, dd, m, md;            // D, DD, M, MD
    int p, r;                       // P, R
    int am, ad, ay;                 // AM, AD, AY
    int zo;                         // ZO
    int sd;                         // SD
    int ll;                         // LL
    int q[17], q1;                  // Q(), Q1
    double sn, sn_rec[2], ml[2], dl;// SN, SN(), ML(), DL
    int ln;                         // LN
    int s;                          // S, disk side 0/1
    int a;                          // A, talk speaker

    // health and environment (4.3)
    double h, h0, hr, fs;           // H, H0, HR, FS
    int tm, w, pp;                  // TM, W, PP
    double qt, qp, ar, as, tr, ts;  // QT, QP, AR, AS, TR, TS
    double op, fc, f0, bs;          // OP, FC, F0, BS
    double re[15];                  // RE()
    int w1;                         // W1, river crossing in progress
    int sr;                         // SR, status panel shows health/food (display)
    int x5;                         // X5, rest date row (display)

    // loss lists (T$, Z)
    char t[OT_LOSS_MAX][OT_LINE_MAX];
    int nt;

    // river (RIVER.LIB): the outcome text A$ and the crossing method I
    char amsg[128];                 // A$
    int ci;                         // I: 1 ferry, 2 ford, 3 float
    int rc;                         // RC, river record
    double rd, rw, rs; int rb;      // RD, RW, RS, RB

    Raft raft;
    Score score;

    // Memory handoff area between the chained programs (spec section 30),
    // addressed as in the original: mem[900] is PEEK(900).
    unsigned char mem[2048];
    int f9;                         // F9, a 700-style message is up (display)

    Outcome outcome;
    int err, err_line;              // OUTCOME_ERROR: Applesoft error number and line
    jmp_buf over;                   // TOMB.LIB / FLOAT "& RNH,MENU": leave from any depth
};

// Set up a game with host hooks. rnd may be NULL for the built-in PRNG seeded by seed.
void game_init(Game *g, IoFn io, void *io_ctx, RndFn rnd, void *rnd_state,
               unsigned long long seed, Persist *persist);

// Watch every request: called with answered 0 before the host's io() and 1
// (with its answer) after. NULL to stop.
typedef void (*ReqObserver)(void *ctx, const Game *g, const Req *r, int answered, int answer);
void ot_set_observer(ReqObserver fn, void *ctx);

// Play one game from "1. Travel the trail" to its end (MENU:9000 onward).
Outcome game_play(Game *g);

// Esc Esc during the game: GOTO 31000, back to the menu from any depth.
void game_abandon(Game *g);

// ---- rules, exposed for the host and the tests ----
double ot_rnd(Game *g);
void climate(const Game *g, double *qt, double *qp);        // FN W(0), FN W(1)
void recompute(Game *g);                                    // OREGON TRAIL:650-660
void river_conditions(Game *g);                             // RIVER.LIB:50150
int  zone_of(int lm);                                       // 1000
double store_price(int item, int lm);                       // BUY.LIB:50030, dollars
long store_price_cents(int item, int lm);
void score_compute(Game *g);                                // WIN:130-140, 625-630, from g->mem

// Raft game (FLOAT), exposed for the host's drawing and for tests.
void raft_start(Game *g);
int  raft_tick(Game *g, int key);   // returns RAFT_* status
enum { RAFT_GO, RAFT_HIT, RAFT_LANDED, RAFT_MISSED };

#endif
