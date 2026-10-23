// Engine core: setup, host plumbing, persistence, and the two programs that
// run before the trail (MENU's new-game path and BUY SUPPLIES).
#include "engine.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Host plumbing
// ---------------------------------------------------------------------------
static double splitmix(void *state) {
    unsigned long long *s = state;
    unsigned long long z = (*s += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    z ^= z >> 31;
    return (double)(z >> 11) / 9007199254740992.0;     // [0, 1)
}

void game_init(Game *g, IoFn io, void *io_ctx, RndFn rnd, void *rnd_state,
               unsigned long long seed, Persist *persist) {
    memset(g, 0, sizeof *g);
    g->io = io; g->io_ctx = io_ctx;
    if (rnd) { g->rnd = rnd; g->rnd_state = rnd_state; }
    else { g->seed = seed; g->rnd = splitmix; g->rnd_state = &g->seed; }
    g->persist = persist;
}

double ot_rnd(Game *g) { return g->rnd(g->rnd_state); }

// The observer sees every request before the host answers it and again after
// (the harness: tools/c/harness.c). It only watches.
static ReqObserver s_obs; static void *s_obs_ctx;
void ot_set_observer(ReqObserver fn, void *ctx) { s_obs = fn; s_obs_ctx = ctx; }

int ot_ask(Game *g, Req *r) {
    if (s_obs) s_obs(s_obs_ctx, g, r, 0, 0);
    int v = g->io(g->io_ctx, g, r);
    if (s_obs) s_obs(s_obs_ctx, g, r, 1, v);
    return v;
}

void game_abandon(Game *g) { game_over(g, OUTCOME_ABANDONED); }

void game_over(Game *g, Outcome o) {
    g->outcome = o;
    longjmp(g->over, 1);
}

// COMMON.LIB:32000-32120. Only errors the shipped game can actually raise
// reach here; they print the MECC message and return to the menu (31000).
void a2_error(Game *g, int err, int line, const char *program) {
    g->err = err; g->err_line = line;
    ASK(g, .kind = REQ_ERROR, .a = err, .b = line, .text = program);
    game_over(g, OUTCOME_ERROR);
}

void poke(Game *g, int addr, double v, int line) {
    double t = floor(v);
    if (t < 0 || t > 255) a2_error(g, 53, line, "OREGON TRAIL");
    g->mem[addr] = (unsigned char)t;
}

int peek(const Game *g, int addr) { return g->mem[addr]; }

// VAL: leading number, spaces ignored, 0 when there is none.
double a2_val(const char *s) {
    char buf[40]; int n = 0;
    for (; *s && n < 39; s++) {
        if (*s == ' ') continue;
        if ((*s >= '0' && *s <= '9') || *s == '.' || *s == '-' || *s == '+' ||
            *s == 'E' || *s == 'e') buf[n++] = *s;
        else break;
    }
    buf[n] = 0;
    double v = 0;
    sscanf(buf, "%lf", &v);
    return v;
}

// ---------------------------------------------------------------------------
// Persistent files
// ---------------------------------------------------------------------------
static const char *const ORIGINAL_TOP[10][3] = {    // MANAGEMENT:20000-20010
    { "Stephen Meek", "7650", "Trail guide" }, { "David Hastings", "5694", "Adventurer" },
    { "Andrew Sublette", "4138", "Adventurer" }, { "Celinda Hines", "2945", "Greenhorn" },
    { "Ezra Meeker", "2052", "Greenhorn" }, { "William Vaughn", "1401", "Greenhorn" },
    { "Mary Bartlett", "937", "Greenhorn" }, { "William Wiggins", "615", "Greenhorn" },
    { "Charles Hopper", "396", "Greenhorn" }, { "Elijah White", "250", "Greenhorn" },
};

void persist_erase_tombs(Persist *p) {
    for (int s = 0; s < 2; s++) {
        int pos = 0;
        seq_write(p->tomb[s], SEQ_SIZE, &pos, "0\r0\r");
    }
}

void topten_write(Persist *p, TopTen hi) {
    int pos = 0;
    seq_write(p->hiscore, SEQ_SIZE, &pos, "L\r");        // write-protect probe (WIN:535)
    pos = 0;
    for (int l = 0; l < 10; l++)
        for (int k = 0; k < 3; k++) {
            seq_write(p->hiscore, SEQ_SIZE, &pos, hi[l][k]);
            seq_write(p->hiscore, SEQ_SIZE, &pos, "\r");
        }
}

void persist_reset_topten(Persist *p) {
    TopTen hi;
    for (int l = 0; l < 10; l++)
        for (int k = 0; k < 3; k++) snprintf(hi[l][k], OT_NAME_MAX, "%s", ORIGINAL_TOP[l][k]);
    topten_write(p, hi);
}

void persist_default(Persist *p) {
    memset(p, 0, sizeof *p);
    persist_erase_tombs(p);
    persist_reset_topten(p);
}

void topten_read(const Persist *p, TopTen hi) {
    int pos = 0;
    for (int l = 0; l < 10; l++)
        for (int k = 0; k < 3; k++) seq_read(p->hiscore, SEQ_SIZE, &pos, hi[l][k], OT_NAME_MAX);
}

// OREGON TRAIL:29004 and FLIP.LIB:50030: segment id and miles of records 0, 1.
void tomb_read_segments(Game *g) {
    const unsigned char *f = g->persist->tomb[g->s];
    char buf[64];
    for (int l = 0; l < 2; l++) {
        int pos = l * 49;
        seq_read(f, SEQ_SIZE, &pos, buf, sizeof buf); g->sn_rec[l] = a2_val(buf);
        seq_read(f, SEQ_SIZE, &pos, buf, sizeof buf); g->ml[l] = a2_val(buf);
    }
}

// TOMB.LIB:50100
void tomb_read(Game *g, int rec, char *name, char *epitaph) {
    const unsigned char *f = g->persist->tomb[g->s];
    char buf[256];
    int pos = rec * 49;
    seq_read(f, SEQ_SIZE, &pos, buf, sizeof buf);
    seq_read(f, SEQ_SIZE, &pos, buf, sizeof buf);
    seq_read(f, SEQ_SIZE, &pos, buf, sizeof buf); snprintf(name, OT_NAME_MAX, "%.*s", OT_NAME_MAX - 1, buf);
    seq_read(f, SEQ_SIZE, &pos, buf, sizeof buf); snprintf(epitaph, OT_EPITAPH_MAX, "%.*s", OT_EPITAPH_MAX - 1, buf);
}

// TOMB.LIB:50035: record 0 of the current side; it may run past 49 bytes.
void tomb_write(Game *g, const char *epitaph) {
    unsigned char *f = g->persist->tomb[g->s];
    char num[20];
    int pos = 0;
    seq_write(f, SEQ_SIZE, &pos, a2_str(g->nm * 100 + g->lm, num)); seq_write(f, SEQ_SIZE, &pos, "\r");
    seq_write(f, SEQ_SIZE, &pos, a2_str(g->d, num));                seq_write(f, SEQ_SIZE, &pos, "\r");
    seq_write(f, SEQ_SIZE, &pos, g->name[g->np]);                   seq_write(f, SEQ_SIZE, &pos, "\r");
    seq_write(f, SEQ_SIZE, &pos, epitaph);                          seq_write(f, SEQ_SIZE, &pos, "\r");
    ASK(g, .kind = REQ_SAVE, .a = 0);
}

// ---------------------------------------------------------------------------
// Rules shared with the host
// ---------------------------------------------------------------------------
// FN W (OREGON TRAIL:105): base temperature and precipitation for month AM, zone ZO.
void climate(const Game *g, double *qt, double *qp) {
    const char *wc = WC[g->zo];
    *qt = (unsigned char)wc[g->am * 2 - 2] - 50;
    *qp = .003 * ((unsigned char)wc[g->am * 2 - 1] - 30);
}

int zone_of(int lm) { return (lm > 2) + (lm > 5) + (lm > 10) + (lm > 13); }

// 650-660
void recompute(Game *g) {
    double v = g->inv[INV_OXEN] / 4;
    if (v > 1) v = 1;
    g->fc = g->np * (4 - g->r);
    g->bs = g->md * v * (g->p + 1) / 2;
    g->op = g->inv[INV_CLOTHES] / g->np;
    g->f0 = 2 * (g->r - 1);
}

// BUY.LIB:50003, 50030: V = VAL(S$(L-1,1)): V = V + .25*Q*V
long store_price_cents(int item, int lm) {
    static const long base[7] = { 2000, 1000, 200, 1000, 1000, 1000, 20 };
    int q = (lm > 2) + (lm > 4) + (lm > 7) + (lm > 10) + (lm > 12) + (lm > 13);
    return base[item - 1] * (4 + q) / 4;
}
double store_price(int item, int lm) { return store_price_cents(item, lm) / 100.0; }

// ---------------------------------------------------------------------------
// Play one game
// ---------------------------------------------------------------------------
Outcome game_play(Game *g) {
    if (setjmp(g->over)) return g->outcome;
    menu_new_game(g);
    buy_supplies(g);
    trail_run(g);
    return g->outcome;      // not reached: the trail ends in WIN or a death
}

// MENU:9000 -> 4000 (occupation), 6000 (names), then BUY SUPPLIES.
void menu_new_game(Game *g) {
    poke(g, 901, 48, 9000);
    int z = ASK(g, .kind = REQ_OCCUPATION);                     // 4025
    g->occupation = z;
    poke(g, 915, z, 4030);
    long tenths = ((z == 1) * 1600 + (z == 2) * 800 + (z == 3) * 400) * 10;
    poke(g, 913, tenths % 256, 4030);
    poke(g, 914, tenths / 256, 4030);

    // 6000-6015: the ten default names dealt into ten slots at random.
    char n[10][OT_NAME_MAX] = { { 0 } };
    for (int l = 0; l < 10; l++) {
        int k = (int)floor(RND(g) * 10);
        while (n[k][0]) { k = k + 1; k = k * (k < 10); }
        snprintf(n[k], OT_NAME_MAX, "%s", DEFAULT_NAMES[l]);
    }
    char names[5][OT_NAME_MAX];
    for (int l = 0; l < 5; l++) memcpy(names[l], n[l], OT_NAME_MAX);
    ASK(g, .kind = REQ_NAMES, .names = names);                  // 6015-6042

    // 6045: names into memory at 1920, NUL-terminated.
    int na = 1920;
    for (int l = 0; l < 5; l++) {
        for (const char *c = names[l]; *c; c++) poke(g, na++, (unsigned char)*c, 6045);
        poke(g, na++, 0, 6045);
    }
}

// BUY SUPPLIES:4000 (910-950): the bill, in cents.
static long matt_bill(const Game *g) {
    return 4000L * peek(g, 905) + 20L * (peek(g, 907) * 256 + peek(g, 906)) +
           1000L * peek(g, 908) + 200L * peek(g, 909) +
           1000L * (peek(g, 910) + peek(g, 911) + peek(g, 912));
}

static void matt_sync(Game *g) {
    Matt *m = &g->matt;
    m->yokes = peek(g, 905);
    m->food = peek(g, 907) * 256 + peek(g, 906);
    m->clothing = peek(g, 908);
    m->boxes = peek(g, 909);
    for (int i = 0; i < 3; i++) m->parts[i] = peek(g, 910 + i);
    m->bill_cents = matt_bill(g);
}

void buy_supplies(Game *g) {
    // 120: money arrives in tenths of a dollar; 904-912 cleared.
    long my_cents = (peek(g, 913) + peek(g, 914) * 256L) * 10;
    for (int a = 904; a <= 912; a++) poke(g, a, 0, 120);

    // 6000-6030: departure month.
    poke(g, 903, 1, 6000);
    int c = ASK(g, .kind = REQ_MONTH);
    poke(g, 902, c + 2, 6030);

    ASK(g, .kind = REQ_MATT_INTRO, .x = my_cents / 100.0);    // 2000-2030
    int change = 0;
    for (;;) {                                                  // 1010
        poke(g, 904, 1, 900);                                   // the wagon (900)
        matt_sync(g);
        g->my_cents = my_cents;
        int z = ASK(g, .kind = REQ_MATT_MENU, .a = change);
        if (z == 0) z = 6;                                      // 1015
        if (z == 1) {                                           // 400
            poke(g, 905, ASK(g, .kind = REQ_MATT_QTY, .a = 1), 405);
        } else if (z == 2) {                                    // 500-530
            for (;;) {
                int q = ASK(g, .kind = REQ_MATT_QTY, .a = 2);
                if (q > 2000) { ASK(g, .kind = REQ_MATT_MSG, .a = MATT_MSG_FOOD_LIMIT); continue; }
                poke(g, 906, q % 256, 530);
                poke(g, 907, q / 256, 530);
                break;
            }
        } else if (z == 3) {                                    // 600-610
            poke(g, 908, ASK(g, .kind = REQ_MATT_QTY, .a = 3), 610);
        } else if (z == 4) {                                    // 700
            poke(g, 909, ASK(g, .kind = REQ_MATT_QTY, .a = 4), 700);
        } else if (z == 5) {                                    // 800-815
            for (int l = 0; l <= 2; l++) {
                int q = ASK(g, .kind = REQ_MATT_QTY, .a = 5, .b = l);
                poke(g, 910 + l, q, 810);
                if (q > 3) {
                    matt_sync(g);
                    ASK(g, .kind = REQ_MATT_MSG, .a = MATT_MSG_PART_LIMIT, .b = l);
                    l = l - 1;
                }
            }
        } else {                                                // 5000-5020
            long tb = matt_bill(g);
            if (my_cents < tb) {
                ASK(g, .kind = REQ_MATT_MSG, .a = MATT_MSG_OVER_BUDGET);
                change = 1;
                continue;
            }
            if (!peek(g, 905)) { ASK(g, .kind = REQ_MATT_MSG, .a = MATT_MSG_NO_OXEN); continue; }
            long tenths = (my_cents - tb) / 10;
            poke(g, 913, tenths % 256, 5010);
            poke(g, 914, tenths / 256, 5010);
            matt_sync(g);
            ASK(g, .kind = REQ_MATT_DEPART);
            return;
        }
    }
}
