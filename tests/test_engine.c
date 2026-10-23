// Unit and soak tests for the rules engine: no raylib, no window, no files.
// The engine sources are included directly so their file-static routines are
// visible. Each test cites the spec requirement it checks.
// Built and run by `make test`; a non-zero exit means a failure.
#include "../src/engine/data.c"
#include "../src/engine/a2str.c"
#include "../src/engine/a2num.c"
#include "../src/engine/game.c"
#include "../src/engine/trail.c"
#include "../src/engine/river.c"
#include "../src/engine/finish.c"
#include "script_io.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int checks;
#define CHECK(cond, ...) do { checks++; if (!(cond)) { \
    fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); fprintf(stderr, __VA_ARGS__); \
    fprintf(stderr, "\n"); exit(1); } } while (0)
#define CHECK_STR(got, want) CHECK(strcmp((got), (want)) == 0, "got \"%s\", want \"%s\"", (got), (want))
#define NEAR(a, b) (fabs((a) - (b)) < 1e-9)

static Persist persist;
static ScriptIO io;
static ScriptRnd rs;

// A game with a scripted host and RNG, positioned on the trail with the
// VAR.BIN presets, for testing single routines.
static Game *fresh(void) {
    static Game g;
    script_init(&io);
    memset(&rs, 0, sizeof rs);
    rs.fallback = 12345;
    persist_default(&persist);
    game_init(&g, script_io, &io, script_rnd, &rs, 0, &persist);
    g.np = 5; g.p = 1; g.r = 1; g.am = 5; g.ad = 1; g.ay = 1848; g.sr = 1;
    for (int i = 0; i < 5; i++) snprintf(g.name[i], OT_NAME_MAX, "M%d", i);
    g.inv[INV_OXEN] = 6; g.inv[INV_CLOTHES] = 10; g.inv[INV_BULLETS] = 200;
    g.inv[INV_FOOD] = g.pf = 1000; g.my_cents = 50000;
    return &g;
}

// ---------------------------------------------------------------------------
static void test_str(void) {
    char b[32];
    CHECK_STR(a2_str(0, b), "0");
    CHECK_STR(a2_str(5, b), "5");
    CHECK_STR(a2_str(.5, b), ".5");
    CHECK_STR(a2_str(-.5, b), "-.5");
    CHECK_STR(a2_str(2.5, b), "2.5");
    CHECK_STR(a2_str(1716, b), "1716");
    CHECK_STR(a2_str(69.125, b), "69.125");
    CHECK_STR(a2_str(.05, b), ".05");
    CHECK_STR(a2_str(123456789, b), "123456789");
    CHECK_STR(a2_str(1e9, b), "1E+09");
    CHECK_STR(a2_str(.001, b), "1E-03");
    CHECK_STR(a2_str(1.0 / 3, b), ".333333333");
    CHECK_STR(a2_money(0, b), "0.00");
    CHECK_STR(a2_money(5, b), "0.05");
    CHECK_STR(a2_money(1234, b), "12.34");
    CHECK_STR(a2_money(160000, b), "1600.00");
    CHECK(NEAR(a2_val("69.125"), 69.125) && a2_val("JBH J") == 0 && a2_val("1 2") == 12, "VAL");

    // loss-list names (RIVER.LIB 50250, TRADE.LIB 50250)
    a2_item_name(3, 1, 0, b, sizeof b); CHECK_STR(b, "set of clothing");
    a2_item_name(3, 2, 0, b, sizeof b); CHECK_STR(b, "sets of clothing");
    a2_item_name(8, 1, 0, b, sizeof b); CHECK_STR(b, "pound of food");
    a2_item_name(5, 1, 0, b, sizeof b); CHECK_STR(b, "wagon wheel");
    a2_item_name(4, 1, 0, b, sizeof b); CHECK_STR(b, "bullet");
    a2_item_name(2, 1, 0, b, sizeof b); CHECK_STR(b, "oxen");        // LF.LIB: "1 oxen"
    a2_item_name(2, 1, 1, b, sizeof b); CHECK_STR(b, "ox");          // TRADE.LIB
    printf("PASS: Applesoft STR$, money, VAL, item names\n");
}

// Appendix A tables.
static void test_data(void) {
    Game g = { 0 };
    double qt, qp;
    g.zo = 0; g.am = 1; climate(&g, &qt, &qp); CHECK(qt == 9 && NEAR(qp, .039), "zone 0 Jan");
    g.zo = 2; g.am = 6; climate(&g, &qt, &qp); CHECK(qt == 42 && NEAR(qp, .039), "zone 2 Jun");
    g.zo = 3; g.am = 1; climate(&g, &qt, &qp); CHECK(qt == -1 && NEAR(qp, .015), "zone 3 Jan");
    g.zo = 4; g.am = 12; climate(&g, &qt, &qp); CHECK(qt == 12 && NEAR(qp, .042), "zone 4 Dec");
    g.zo = 5; g.am = 12; climate(&g, &qt, &qp); CHECK(qt == 21 && NEAR(qp, .192), "zone 5 Dec");

    // REQ-077: route totals to The Dalles.
    int bridger = LEG_MILES[0] + LEG_MILES[1] + LEG_MILES[2] + LEG_MILES[3] + LEG_MILES[4] +
                  LEG_MILES[5] + LEG_MILES[6] + LEG_MILES[8] + LEG_MILES[9] + LEG_MILES[11] +
                  LEG_MILES[12] + LEG_MILES[13] + LEG_MILES[14];
    CHECK(bridger + LEG_MILES[15] + LEG_MILES[17] == 1907, "Bridger/Walla Walla total");
    int green = bridger - LEG_MILES[8] - LEG_MILES[9] + LEG_MILES[7] + LEG_MILES[10];
    CHECK(green + LEG_MILES[16] == 1771, "Green River/direct total");

    // REQ-100: zones.
    CHECK(zone_of(2) == 0 && zone_of(3) == 1 && zone_of(5) == 1 && zone_of(6) == 2 &&
          zone_of(10) == 2 && zone_of(11) == 3 && zone_of(13) == 3 && zone_of(14) == 4, "zones");

    // REQ-211: fort prices.
    CHECK(store_price_cents(1, 0) == 2000, "ox at Independence");
    CHECK(store_price_cents(7, 3) == 25, "food at Kearney");
    CHECK(store_price_cents(3, 13) == 450, "ammo at Boise");
    CHECK(store_price_cents(7, 15) == 50, "food at Walla Walla");
    CHECK(store_price_cents(2, 11) == 2000, "clothes at Fort Hall");
    printf("PASS: data tables, climate, zones, prices\n");
}

// ---------------------------------------------------------------------------
// REQ-111: the daily health update, on a rest day with known weather.
static void test_health_day(void) {
    Game *g = fresh();
    g->zo = 0; g->am = 5;           // May, zone 0: QT 45
    g->tm = 3; g->w = 3; g->pp = 0; g->op = 2; g->fc = 15; g->f0 = 0;
    g->h = 50; g->sd = 1; g->d = 0; g->p = 0;
    rnd_push(&rs, .9);              // no weather roll (RND < .5 fails)
    day3100(g);
    // stress: ZT 0, ZC max(0,5-6-2)=0, ZF 0, ZP 0, FS .5*0, H0 0, HR 0
    CHECK(NEAR(g->h, 45), "H decays by 10%%: %f", g->h);
    CHECK(g->pf == 985, "15 lb eaten: %f", g->pf);
    CHECK(g->ad == 2, "calendar");

    // Cold, no clothes, bare bones, grueling: ZT 2, ZC 5, ZF 4, ZP 6 (travel day).
    g = fresh();
    g->zo = 0; g->am = 1; g->tm = 0; g->w = 0; g->op = 0; g->r = 3; recompute(g);
    g->op = 0; g->p = 3; g->h = 0; g->d = 0; g->dl = -1;   // D = 0: exactly one day
    for (int k = 0; k < 15; k++) rnd_push(&rs, .99);    // no events
    rnd_push(&rs, .9);                                  // no weather roll
    day3100(g);
    // FS: ZC > .5 so FS = 0 + .8; H = 0 + 2 + 5 + 4 + 6 + .8
    CHECK(NEAR(g->h, 17.8), "stress sum %f", g->h);
    printf("PASS: daily health update (REQ-111)\n");
}

// REQ-090: calendar, no leap years.
static void test_calendar(void) {
    Game *g = fresh();
    g->am = 2; g->ad = 28; g->sd = 1; g->d = 0;
    rnd_push(&rs, .9);
    day3100(g);
    CHECK(g->am == 3 && g->ad == 1, "Feb 28 -> Mar 1");
    g->am = 12; g->ad = 31; g->sd = 1;
    rnd_push(&rs, .9);
    day3100(g);
    CHECK(g->am == 1 && g->ad == 1 && g->ay == 1849, "new year");
    printf("PASS: calendar (REQ-090)\n");
}

// REQ-142: movement and the snap to the landmark.
static void test_movement(void) {
    Game *g = fresh();
    g->md = 20; g->p = 1; recompute(g);      // 6 oxen -> factor 1, steady
    CHECK(NEAR(g->bs, 20), "BS %f", g->bs);
    g->p = 3; recompute(g); CHECK(NEAR(g->bs, 40), "grueling doubles");
    g->inv[INV_OXEN] = 2; g->p = 1; recompute(g); CHECK(NEAR(g->bs, 10), "2 oxen halve");
    g = fresh();
    g->md = 20; recompute(g);
    g->d = 21; g->dl = -1; g->zo = 0; g->am = 5; g->h1[1] = 3; g->h2[1] = 5;
    for (int day = 0; day < 2; day++) {
        for (int k = 0; k < 15; k++) rnd_push(&rs, .99);
        rnd_push(&rs, .9);
    }
    day3100(g);         // the leg: 18 miles (one afflicted, 20 * .9), then 3 (1.1 * 18 > 3 snaps)
    CHECK(NEAR(g->d, 0) && NEAR(g->m, 21) && g->ad == 3 && io.count[REQ_DAY_END] == 2,
          "D %f M %f days %d", g->d, g->m, io.count[REQ_DAY_END]);
    printf("PASS: movement (REQ-142)\n");
}

// REQ-123 / REQ-341: the member picker never takes the leader while others live.
static void test_pick(void) {
    Game *g = fresh();
    rnd_push(&rs, 0);   CHECK(pick(g) == 1, "Z=0 -> 1");
    rnd_push(&rs, .99); CHECK(pick(g) == 1, "Z=4 -> 0 -> 1");
    rnd_push(&rs, .5);  CHECK(pick(g) == 3, "Z=2 -> 3");
    g->np = 1;
    rnd_push(&rs, .5);  CHECK(pick(g) == 0, "alone: the leader");
    g = fresh();
    g->h1[2] = -1;
    rnd_push(&rs, .2);  CHECK(pick(g) == 3, "skips the dead");
    printf("PASS: member picker (REQ-123)\n");
}

// REQ-122 / REQ-340: a broken arm overwrites an illness and counts as well.
static void test_broken_arm(void) {
    Game *g = fresh();
    g->h1[1] = 5; g->h2[1] = 4;             // cholera
    rnd_push(&rs, .5);                      // not a broken part
    rnd_push(&rs, .2);                      // injury, not an ox
    rnd_push(&rs, 0);                       // pick -> member 1
    rnd_push(&rs, .1);                      // V = 0: a broken arm
    event(g, 8);
    CHECK(g->h1[1] == 0 && g->h2[1] == 30, "arm cured the cholera");
    CHECK(script_has(&io, "M1 has a broken arm."), "message");
    printf("PASS: broken arm cures (REQ-340)\n");
}

// REQ-124: illness, then death on a second roll.
static void test_illness_death(void) {
    Game *g = fresh();
    rnd_push(&rs, 0);                       // V = 3 (exhaustion)
    rnd_push(&rs, 0);                       // member 1
    illness10300(g);
    CHECK(g->h1[1] == 3 && g->h2[1] == 10 && g->hr == 20, "ill");
    rnd_push(&rs, 0); rnd_push(&rs, 0);
    g->h = 130;
    illness10300(g);
    CHECK(g->np == 4 && g->h == 105, "died, H capped at 105");
    CHECK(strcmp(g->name[4], "M1") == 0 && g->h1[4] == -1, "dead swapped to the end");
    CHECK(strcmp(g->name[1], "M4") == 0, "last living moved into the slot");
    CHECK(script_has(&io, "M1 has died."), "message");
    printf("PASS: illness and death (REQ-124, REQ-126)\n");
}

// REQ-201: trade arithmetic.
static void test_trade(void) {
    Game *g = fresh();
    g->inv[INV_CLOTHES] = 10;
    // X = 1 (clothing $10), Y = 6 (food .20): Z = 10 / (.2 * (1 + 1.2*0)) = 50
    rnd_push(&rs, 1.0 / 7 + .01); rnd_push(&rs, 6.0 / 7 + .01); rnd_push(&rs, 0);
    rnd_push(&rs, .5);                      // someone trades
    rnd_push(&rs, .9);                      // She
    trade4900(g);
    CHECK(g->inv[INV_CLOTHES] == 9 && g->inv[INV_FOOD] == 1050, "1 set for 50 lb");
    CHECK(script_has(&io, "TRADE_OFFER 1 set of clothing / 50 pounds of food"), "offer text");

    // X = 2 (bullets), Y = 6 (food): per-unit .1 vs .2, Z < 1 -> V = INT(1/Z), both x50
    g = fresh();
    rnd_push(&rs, 2.0 / 7 + .01); rnd_push(&rs, 6.0 / 7 + .01); rnd_push(&rs, 0);
    rnd_push(&rs, .5); rnd_push(&rs, .1);
    trade4900(g);
    CHECK(g->inv[INV_BULLETS] == 100 && g->inv[INV_FOOD] == 1050, "100 bullets for 50 lb");

    // Y == X is bumped to X + 1.
    g = fresh();
    rnd_push(&rs, 0); rnd_push(&rs, 0); rnd_push(&rs, 0);
    rnd_push(&rs, .99);                     // nobody today
    trade4900(g);
    CHECK(io.count[REQ_TRADE_NOBODY] == 1, "nobody");
    printf("PASS: trading (REQ-201..203)\n");
}

// REQ-193: hunting post-processing.
static void test_hunt(void) {
    Game *g = fresh();
    g->inv[INV_FOOD] = g->pf = 1950;
    script_push(&io, REQ_HUNT, 150);        // raw 150 -> 75 -> room for 50
    hunt4600(g);
    CHECK(g->pf == 2000, "filled to 2000: %f", g->pf);
    g = fresh();
    script_push(&io, REQ_HUNT, 250);        // 125 -> carry 100
    hunt4600(g);
    CHECK(g->pf == 1100, "carried 100");
    g = fresh();
    script_push(&io, REQ_HUNT, 2);          // below 3: not halved
    hunt4600(g);
    CHECK(g->pf == 1002, "small game kept whole");
    printf("PASS: hunting results (REQ-193)\n");
}

// REQ-235..236 and river conditions.
static void test_river(void) {
    Game *g = fresh();
    g->lm = 1; g->ar = .5; g->rc = 0;
    river_conditions(g);
    CHECK(NEAR(g->rd, 2) && g->rw == 607 && NEAR(g->rs, 3.5) && g->rb == 0, "Kansas");
    g->ar = .26; river_conditions(g);
    CHECK(NEAR(g->rd, 1.5), "RD rounds to one decimal: %f", g->rd);

    // Ferry at the Kansas: shallow river refuses, then a ford succeeds.
    g = fresh();
    g->lm = 1; g->ar = .1;
    script_push(&io, REQ_RIVER_MENU, 3);    // ferry
    script_push(&io, REQ_RIVER_MENU, 1);    // ford
    river_run(g);
    CHECK(io.count[REQ_RIVER_MSG] == 1, "ferry refused (RD < 2.5)");
    CHECK_STR(g->amsg, "You made the crossing successfully.");

    // Deep ford loses things.
    g = fresh();
    g->lm = 9; g->ar = 0;                   // Green River, 20 ft
    script_push(&io, REQ_RIVER_MENU, 1);
    for (int k = 0; k < 40; k++) rnd_push(&rs, 0);
    river_run(g);
    CHECK_STR(g->amsg, "The river is too deep to ford.  You lose:");
    CHECK(g->nt > 0 && g->inv[INV_OXEN] == 0, "everything goes");
    printf("PASS: rivers (REQ-230..237)\n");
}

// REQ-241 / REQ-347: the Barlow toll with exact change loops back silently.
static void test_barlow(void) {
    Game *g = fresh();
    g->inv[INV_OXEN] = 4;
    g->my_cents = 700;                      // toll 5 + 4*.5 = $7
    script_push(&io, REQ_DALLES, 2);
    script_push(&io, REQ_BARLOW_ASK, 1);
    g->my_cents = 800;
    script_push(&io, REQ_DALLES, 2);
    script_push(&io, REQ_BARLOW_ASK, 1);
    end_lib(g);
    CHECK(g->my_cents == 100, "paid $7 from $8");
    g = fresh();
    g->inv[INV_OXEN] = 4;
    g->my_cents = 700;
    script_push(&io, REQ_DALLES, 2); script_push(&io, REQ_BARLOW_ASK, 1);
    script_push(&io, REQ_DALLES, 2); script_push(&io, REQ_BARLOW_ASK, 1);
    io.limit = 30;
    // exact change: two refusals without a message, then the policy floats
    if (!setjmp(g->over)) end_lib(g);
    CHECK(io.count[REQ_BARLOW_NO_CASH] == 0 && io.count[REQ_DALLES] == 3, "exact change refused silently");
    printf("PASS: Barlow toll (REQ-241, REQ-347)\n");
}

// REQ-251..253: points.
static void test_score(void) {
    Game *g = fresh();
    memset(g->mem, 0, sizeof g->mem);
    g->mem[900] = 5; g->mem[916] = 0; g->mem[909] = 6;
    g->mem[910] = 1; g->mem[911] = 1; g->mem[912] = 1;
    g->mem[908] = 10; g->mem[904] = 200; g->mem[906] = 500 % 256; g->mem[907] = 1;
    g->mem[913] = 123; g->mem[917] = 45; g->mem[915] = 2;
    score_compute(g);
    // 2500 + 50 + 24 + 6 + 20 + 4 + 20 + 24 = 2648, doubled
    CHECK(g->score.base == 2648, "base %ld", g->score.base);
    CHECK(g->score.total == 5296 && g->score.rating == 1, "carpenter doubles, Adventurer");
    printf("PASS: scoring (REQ-251..253)\n");
}

// REQ-254: Top Ten insertion below equal scores.
static void test_topten(void) {
    Game *g = fresh();
    TopTen hi;
    memset(g->mem, 0, sizeof g->mem);
    g->mem[900] = 5; g->mem[915] = 1;       // 2500 + 50 = 2550
    if (!setjmp(g->over)) win_run(g);
    topten_read(&persist, hi);
    CHECK(g->score.row == 4 && strcmp(hi[4][0], "Tester") == 0, "row %d", g->score.row);
    CHECK_STR(hi[4][1], "2550");
    CHECK_STR(hi[5][0], "Ezra Meeker");
    CHECK_STR(hi[9][0], "Charles Hopper");
    printf("PASS: Top Ten (REQ-254)\n");
}

// REQ-260 / REQ-358: the tombstone record can run past its 49-byte slot.
static void test_tomb(void) {
    Game *g = fresh();
    g->nm = 3; g->lm = 2; g->d = 98.7654321; g->np = 0;
    snprintf(g->name[0], OT_NAME_MAX, "Maximilia");
    tomb_write(g, "Here lies a very long epitaph");
    char name[OT_NAME_MAX], ep[OT_EPITAPH_MAX];
    tomb_read(g, 0, name, ep);
    CHECK_STR(name, "Maximilia");
    CHECK_STR(ep, "Here lies a very long epitaph");
    tomb_read_segments(g);
    CHECK(g->sn_rec[0] == 302 && NEAR(g->ml[0], 98.7654321), "segment");
    CHECK(persist.tomb[0][49] != 0, "record 0 overflowed into record 1");
    printf("PASS: tombstones (REQ-260, REQ-358)\n");
}

// REQ-244..248: raft drift and the shore.
static void test_raft(void) {
    Game *g = fresh();
    raft_start(g);
    for (int k = 0; k < 40; k++) rnd_push(&rs, .99);    // no rocks
    int st = RAFT_GO, t = 0;
    while (st == RAFT_GO && t < 30) { st = raft_tick(g, 0); t++; }
    CHECK(st == RAFT_HIT && g->raft.hp == 0 && g->raft.dir == 1 && g->raft.rock == -1,
          "drifts into the left shore at tick %d", t);
    printf("PASS: raft drift (REQ-244)\n");
}

// REQ-150: no food eaten before the first leg (FC is still 0 at Independence).
static void test_independence_rest(void) {
    Game *g = fresh();
    g->fc = 0; g->sd = 2; g->x5 = 9;
    for (int k = 0; k < 4; k++) rnd_push(&rs, .9);
    rest4505(g);
    CHECK(g->pf == 1000, "resting at Independence eats nothing: %f", g->pf);
    printf("PASS: rest before the first leg (FC = 0 quirk)\n");
}

// ---------------------------------------------------------------------------
// Whole games: every seed must end, with state in range.
static void test_soak(void) {
    int outcomes[5] = { 0 };
    for (unsigned long long seed = 1; seed <= 400; seed++) {
        static Game g;
        script_init(&io);
        io.hunt_pounds = (int)(seed % 7) * 40;
        io.matt_plan[0] = 1 + (int)(seed % 5);
        io.matt_plan[1] = 200 + (int)(seed % 9) * 150;
        persist_default(&persist);
        game_init(&g, script_io, &io, NULL, NULL, seed, &persist);
        Outcome o = game_play(&g);
        CHECK(o == OUTCOME_ARRIVED || o == OUTCOME_DIED || o == OUTCOME_ERROR, "seed %llu outcome %d", seed, o);
        CHECK(g.np >= 0 && g.np <= 5, "NP");
        for (int i = 3; i <= 8; i++) CHECK(g.inv[i] >= 0, "seed %llu inventory %d = %f", seed, i, g.inv[i]);
        CHECK(g.h >= 0 && g.h <= 140, "seed %llu H %f", seed, g.h);
        outcomes[o]++;
    }
    CHECK(outcomes[OUTCOME_ARRIVED] > 0 && outcomes[OUTCOME_DIED] > 0, "both endings occur");
    printf("PASS: 400 autoplayed games end (arrived %d, died %d, error %d)\n",
           outcomes[OUTCOME_ARRIVED], outcomes[OUTCOME_DIED], outcomes[OUTCOME_ERROR]);
}

// CROSS.LIB draws RND for splash points it never plots (REQ-238), and each
// leg clears the keyboard strobe after drawing its scene (REQ-080). Found by
// lockstep replay against the original (tools/lockstep.sh).
static long s_draws;
static int s_cross_a, s_cross_b, s_crossings, s_cross_bad, s_after_cross;
static long s_cross_at;
static int s_prev_gosub, s_usr1, s_usr1_bad;
static unsigned long long s_cr_seed;
static double counting_rnd(void *state) {
    (void)state;
    s_draws++;
    unsigned long long z = (s_cr_seed += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return (double)((z ^ (z >> 31)) >> 11) / 9007199254740992.0;
}
static int cross_io(void *ctx, Game *g, Req *r) {
    if (s_after_cross) {
        int want = 2 * (53 + (!s_cross_b ? 61 : s_cross_a == 2 ? 22 : 0)) + (s_cross_b && s_cross_a == 2 ? 22 : 0);
        if (s_draws - s_cross_at != want) s_cross_bad++;
        s_after_cross = 0;
    }
    if (r->kind == REQ_CROSS) { s_cross_a = r->a; s_cross_b = r->b; s_cross_at = s_draws; s_after_cross = 1; s_crossings++; }
    if (r->kind == REQ_GOSUB) {
        if (r->a == 3000) { s_usr1++; if (s_prev_gosub != 300) s_usr1_bad++; }
        s_prev_gosub = r->a;
    }
    return script_io(ctx, g, r);
}
static void test_cross_rnd(void) {
    for (unsigned long long seed = 1; seed <= 40; seed++) {
        static Game g;
        script_init(&io);
        persist_default(&persist);
        s_cr_seed = seed;
        game_init(&g, cross_io, &io, counting_rnd, NULL, seed, &persist);
        game_play(&g);
    }
    CHECK(s_crossings > 20 && !s_cross_bad, "CROSS.LIB draws: %d of %d crossings wrong", s_cross_bad, s_crossings);
    CHECK(s_usr1 > 0 && !s_usr1_bad, "leg USR(1): %d of %d not right after GOSUB 300", s_usr1_bad, s_usr1);
    printf("PASS: crossing animation draws its unplotted splashes; legs clear the strobe (REQ-238, REQ-080)\n");
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    test_str();
    test_data();
    test_health_day();
    test_calendar();
    test_movement();
    test_pick();
    test_broken_arm();
    test_illness_death();
    test_trade();
    test_hunt();
    test_river();
    test_barlow();
    test_score();
    test_topten();
    test_tomb();
    test_raft();
    test_cross_rnd();
    test_independence_rest();
    test_soak();
    printf("All engine tests passed (%d checks).\n", checks);
    return 0;
}
