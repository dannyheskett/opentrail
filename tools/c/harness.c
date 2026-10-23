// The harness: the whole game, either mode, driven by commands and stepped one
// frame at a time, for scripts and agents to play, inspect and look at.
//
// usage: harness [--modern] [--touch] [--seed N] [--size WxH]
// Reads one command per line on stdin and answers each with one JSON line on
// stdout. Nothing runs between commands: emulated time moves only in step,
// wait and until, so a script is repeatable (the game's seed comes from the
// emulated clock; --seed sets its start). The game is the real one: the same
// engine, screens and presenters as build/opentrail (a hidden window draws
// them), and the Top Ten and tombstones live in memory only.
//
// Commands:
//   key K...       queue keys: a character, or Return Esc Space Left Right Up
//                  Down Tab Delete Backspace, or Ctrl-A..Ctrl-Z. Keys are typed
//                  one at a time as the game reads them, while time runs.
//   type TEXT      queue the rest of the line as typed characters
//   click N        modern mode: click choice N (0-based)
//   esc            the window's Esc (the quit dialog); "esc no" / "esc yes" answer it
//   step [N]       run N frames (1/60 s of game time each; default 1)
//   wait [MAX]     run until the queued keys are read and the game is blocked
//                  on a key (at most MAX frames, default 36000)
//   until KIND [MAX]  run until the engine asks for request KIND (e.g. HUNT,
//                  ACTION_MENU; "any" for the next one; "field" until a hunt
//                  is on the field)
//   state          the frame, the current request, the game (null outside one),
//                  the scene (modern), the hunt field while a hunt runs
//   snap PATH [W H]  the frame as shown, as a PNG (default: the window size;
//                  modern 384 252 is the canvas at 1x, legacy 560 384 at 1x)
//   watch on|off   also print an event line for every request as it happens
//   quit
// Every answer has "ok"; step, wait and until also give the frame, whether the
// game waits on a key, and the request it is in.
#include "host.h"
#include "screen.h"
#include "modern.h"
#include "draw.h"
#include "view_legacy.h"
#include "view_modern.h"
#include "hunt.h"
#include "data.h"
#include "png.h"
#include <raylib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

void persist_save(const Persist *p, int which) { (void)p; (void)which; }

static const char *const REQ_NAME[REQ_COUNT] = {
    [REQ_GOSUB] = "GOSUB", [REQ_OCCUPATION] = "OCCUPATION", [REQ_NAMES] = "NAMES",
    [REQ_MONTH] = "MONTH", [REQ_MATT_INTRO] = "MATT_INTRO", [REQ_MATT_MENU] = "MATT_MENU",
    [REQ_MATT_QTY] = "MATT_QTY", [REQ_MATT_MSG] = "MATT_MSG", [REQ_MATT_DEPART] = "MATT_DEPART",
    [REQ_LOOK_AROUND] = "LOOK_AROUND", [REQ_ACTION_MENU] = "ACTION_MENU",
    [REQ_MUST_TRADE] = "MUST_TRADE", [REQ_SUPPLIES] = "SUPPLIES", [REQ_MAP] = "MAP",
    [REQ_PACE] = "PACE", [REQ_RATIONS] = "RATIONS", [REQ_REST_ASK] = "REST_ASK",
    [REQ_REST_DATE] = "REST_DATE", [REQ_TALK] = "TALK", [REQ_STORE_MENU] = "STORE_MENU",
    [REQ_STORE_QTY] = "STORE_QTY", [REQ_STORE_MSG] = "STORE_MSG", [REQ_TRADE_SHOW] = "TRADE_SHOW",
    [REQ_TRADE_NOBODY] = "TRADE_NOBODY", [REQ_TRADE_LACK] = "TRADE_LACK",
    [REQ_TRADE_OFFER] = "TRADE_OFFER", [REQ_HUNT] = "HUNT", [REQ_HUNT_RESULT] = "HUNT_RESULT",
    [REQ_SPLIT] = "SPLIT", [REQ_LEG] = "LEG", [REQ_POLL] = "POLL", [REQ_DAY_END] = "DAY_END",
    [REQ_EVENT_SHOW] = "EVENT_SHOW", [REQ_EVENT_END] = "EVENT_END", [REQ_GRAVE_ASK] = "GRAVE_ASK",
    [REQ_GRAVE_SHOW] = "GRAVE_SHOW", [REQ_PART_ASK] = "PART_ASK", [REQ_PART_MSG] = "PART_MSG",
    [REQ_NO_OXEN] = "NO_OXEN", [REQ_TOMB_FINAL] = "TOMB_FINAL", [REQ_ALL_DEAD] = "ALL_DEAD",
    [REQ_FLIP] = "FLIP", [REQ_SAVE] = "SAVE", [REQ_ERROR] = "ERROR",
    [REQ_RIVER_INTRO] = "RIVER_INTRO", [REQ_RIVER_MENU] = "RIVER_MENU",
    [REQ_RIVER_INFO] = "RIVER_INFO", [REQ_RIVER_CAMP] = "RIVER_CAMP",
    [REQ_RIVER_CAMP_DATE] = "RIVER_CAMP_DATE", [REQ_RIVER_MSG] = "RIVER_MSG",
    [REQ_FERRY_OFFER] = "FERRY_OFFER", [REQ_GUIDE_OFFER] = "GUIDE_OFFER",
    [REQ_GUIDE_GO] = "GUIDE_GO", [REQ_CROSS] = "CROSS", [REQ_DALLES] = "DALLES",
    [REQ_BARLOW_ASK] = "BARLOW_ASK", [REQ_BARLOW_PAID] = "BARLOW_PAID",
    [REQ_BARLOW_NO_CASH] = "BARLOW_NO_CASH", [REQ_RAFT_INTRO] = "RAFT_INTRO",
    [REQ_RAFT_FRAME] = "RAFT_FRAME", [REQ_RAFT_HIT] = "RAFT_HIT", [REQ_RAFT_LAND] = "RAFT_LAND",
    [REQ_ARRIVE] = "ARRIVE", [REQ_TOPTEN_NAME] = "TOPTEN_NAME", [REQ_TOPTEN_SHOW] = "TOPTEN_SHOW",
};

static Mach s_m;
static Game s_g;
static Persist s_p;
static Shell s_sh;
static MShell s_ms;
static int s_touch;                     // --touch: modern mode as on a phone
static int s_modern, s_w = 1152, s_h = 756;
static long s_frame;

// keys waiting to be typed
static uint8_t s_keys[4096];
static int s_kh, s_kt, s_gap;

// the run in progress
enum { RUN_NONE, RUN_STEP, RUN_WAIT, RUN_UNTIL };
static int s_run;
static long s_left;                 // frames left (step) or the limit (wait, until)
static int s_until = -1;            // REQ kind, REQ_COUNT = any
static int s_hit;                   // until: the request came

// the engine's requests, seen by the observer
static Req s_req;                   // the latest (text copied)
static char s_text[512], s_text2[512];
static int s_in_req, s_have_req, s_answer;
static long s_nreq;
static int s_watch;

// ---------------------------------------------------------------------------
// JSON
// ---------------------------------------------------------------------------
static void jstr(const char *s) {
    putchar('"');
    for (; s && *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '"' || c == '\\') printf("\\%c", c);
        else if (c == '\n') fputs("\\n", stdout);
        else if (c < 0x20 || c >= 0x7F) printf("\\u%04x", c);
        else putchar(c);
    }
    putchar('"');
}

static const char *req_name(int k) { return k >= 0 && k < REQ_COUNT && REQ_NAME[k] ? REQ_NAME[k] : "?"; }

static void req_json(void) {
    if (!s_have_req) { fputs("null", stdout); return; }
    printf("{\"kind\":"); jstr(req_name(s_req.kind));
    printf(",\"n\":%ld,\"active\":%s,\"a\":%d,\"b\":%d,\"c\":%d,\"x\":%g", s_nreq,
           s_in_req ? "true" : "false", s_req.a, s_req.b, s_req.c, s_req.x);
    if (s_text[0]) { fputs(",\"text\":", stdout); jstr(s_text); }
    if (s_text2[0]) { fputs(",\"text2\":", stdout); jstr(s_text2); }
    if (!s_in_req) printf(",\"answer\":%d", s_answer);
    putchar('}');
}

static void brief(void) {
    printf("\"frame\":%ld,\"waiting\":%s,\"keys_queued\":%d,\"modal\":%s,\"req\":", s_frame,
           s_m.waiting ? "true" : "false", s_kt - s_kh, host_view_modal() ? "true" : "false");
    req_json();
}

static void game_json(void) {
    const Game *g = &s_g;
    printf("{\"date\":\"%d/%d/%d\",\"landmark\":%d,", g->am, g->ad, g->ay, g->lm);
    fputs("\"landmark_name\":", stdout); jstr(g->lm >= 0 && g->lm < 18 ? LM_NAME[g->lm] : "");
    printf(",\"on_trail\":%d,\"lm_type\":%d,\"river_depth\":%g,\"river_width\":%g,\"weather\":%d,\"split\":%d",
           g->ll, g->lm >= 0 && g->lm < 18 ? LM_TYPE[g->lm] : 0, g->rd, g->rw, g->w, g->lm >= 0 && g->lm < 18 ? LM_ALT[g->lm] : 0);
    printf(",\"next\":%d,\"miles\":%g,\"to_next\":%g,\"pace\":%d,\"rations\":%d,\"health\":%g,"
           "\"cash\":%.2f,\"food\":%g,\"bullets\":%g,\"oxen\":%g,\"clothes\":%g,"
           "\"wheels\":%g,\"axles\":%g,\"tongues\":%g,\"party\":[",
           g->nm, g->m, g->d, g->p, g->r, g->h, g->my_cents / 100.0, g->inv[INV_FOOD],
           g->inv[INV_BULLETS], g->inv[INV_OXEN], g->inv[INV_CLOTHES], g->inv[INV_WHEELS],
           g->inv[INV_AXLES], g->inv[INV_TONGUES]);
    for (int i = 0; i < g->np && i < 5; i++) { if (i) putchar(','); jstr(g->name[i]); }
    const Raft *r = &g->raft;
    printf("],\"outcome\":%d,\"raft\":{\"tc\":%d,\"hp\":%d,\"dir\":%d,\"rocks\":[", g->outcome, r->tc, r->hp, r->dir);
    for (int z = 0; z < 2; z++) printf("%s[%d,%g,%g]", z ? "," : "", r->fl[z], r->rx[z], r->ry[z]);
    printf("],\"sign\":%d,\"trail\":%d}}", r->fl[2], r->fl[3]);
}

static void scene_json(void) {
    const MScene *s = &s_ms.sc;
    static const char *const PR[] = { "none", "continue", "choose", "yesno", "text" };
    fputs("{\"bg\":", stdout); jstr(s->bg);
    fputs(",\"overlay\":", stdout); jstr(s->overlay);
    printf(",\"travel\":%d,\"pan\":%g,\"wagon_frame\":%d,\"map\":%d,\"panel\":%d,",
           s->travel, s->pan, s->wagon_frame, s->map, s->panel);
    fputs("\"title\":", stdout); jstr(s->title);
    fputs(",\"body\":", stdout); jstr(s->body);
    printf(",\"opt_base\":%d,\"opts\":[", s->opt_base);
    for (int i = 0; i < s->nopts; i++) { if (i) putchar(','); jstr(s->opts[i]); }
    printf("],\"prompt\":\"%s\",\"input\":", s->prompt >= 0 && s->prompt <= MP_TEXT ? PR[s->prompt] : "?");
    jstr(s->input);
    fputs(",\"hint\":", stdout); jstr(s->hint);
    fputs(",\"pic\":", stdout); jstr(s->pic);
    if (s->status) {
        fputs(",\"status\":[", stdout);
        for (int i = 0; i < 6; i++) { if (i) putchar(','); jstr(s->st[i]); }
        putchar(']');
    }
    fputs(",\"sprites\":[", stdout);
    for (int i = 0; i < s->nspr; i++) {
        const MSprite *p = &s->spr[i];
        if (i) putchar(',');
        fputs("{\"id\":", stdout); jstr(p->id);
        printf(",\"x\":%g,\"y\":%g,\"flip\":%d,\"bottom\":%d,\"right\":%d,\"scale\":%d",
               p->x, p->y, p->flip, p->bottom, p->right, p->scale);
        if (p->anchor) printf(",\"anchor\":[%g,%g]", p->ax, p->ay);
        putchar('}');
    }
    putchar(']');
    printf(",\"music\":%d", s->music);
    if (s->dot) printf(",\"dot\":[%g,%g]", s->dot_x, s->dot_y);
    putchar('}');
}

static void hunt_json(void) {
    static const char *const K[] = { "scenery", "dead", "animal", "hunter" };
    HuntView v;
    hunt_view(&v);
    printf("{\"zone\":%d,\"bullets\":%d,\"aim\":%d,\"turning\":%d,\"walking\":%d,\"shot\":", v.zone, v.bullets, v.aim, v.turning, v.walking);
    if (v.shot) printf("[%d,%d]", v.sx, v.sy); else fputs("null", stdout);
    fputs(",\"sprites\":[", stdout);
    for (int i = 0; i < v.n; i++) {
        const HuntSprite *p = &v.s[i];
        printf("%s{\"kind\":\"%s\",\"img\":%d,\"frame\":%d,\"mirrored\":%d,\"box\":[%d,%d,%d,%d]}",
               i ? "," : "", K[p->kind], p->img, p->frame, p->mirrored, p->x1, p->y1, p->x2, p->y2);
    }
    fputs("]}", stdout);
}

static void state_json(void) {
    printf("{\"ok\":true,\"mode\":\"%s\",", s_modern ? "modern" : "legacy");
    brief();
    printf(",\"cycles\":%llu,\"clicks\":%lu,\"sound\":%d,\"game\":", s_m.cycles, s_m.clicks, s_m.sound);
    if (s_g.io && s_g.outcome == OUTCOME_NONE) game_json(); else fputs("null", stdout);
    if (s_modern) { fputs(",\"scene\":", stdout); scene_json(); }
    if (hunt_running()) { fputs(",\"hunt\":", stdout); hunt_json(); }
    puts("}");
}

static void ok(void) { puts("{\"ok\":true}"); }
static void fail(const char *why) { fputs("{\"ok\":false,\"error\":", stdout); jstr(why); puts("}"); }

// ---------------------------------------------------------------------------
// The engine's requests
// ---------------------------------------------------------------------------
static void observe(void *ctx, const Game *g, const Req *r, int answered, int answer) {
    (void)ctx; (void)g;
    if (r->kind == REQ_GOSUB || r->kind == REQ_POLL) return;    // display points and travel polls: noise
    if (!answered) {
        s_req = *r;
        snprintf(s_text, sizeof s_text, "%s", r->text ? r->text : "");
        snprintf(s_text2, sizeof s_text2, "%s", r->text2 ? r->text2 : "");
        s_req.text = s_req.text2 = NULL; s_req.lines = NULL; s_req.out = NULL; s_req.names = NULL; s_req.data = NULL;
        s_in_req = 1; s_have_req = 1; s_nreq++;
        if (s_run == RUN_UNTIL && (s_until == REQ_COUNT || s_until == (int)r->kind)) s_hit = 1;
        if (s_watch) { printf("{\"event\":\"req\",\"frame\":%ld,\"req\":", s_frame); req_json(); puts("}"); fflush(stdout); }
    } else if (r->kind == s_req.kind) {
        s_in_req = 0; s_answer = answer;
        if (s_watch) {
            printf("{\"event\":\"answer\",\"frame\":%ld,\"kind\":\"%s\",\"answer\":%d}\n", s_frame, req_name(r->kind), answer);
            fflush(stdout);
        }
    }
}

// ---------------------------------------------------------------------------
// Commands
// ---------------------------------------------------------------------------
static int key_code(const char *k) {
    static const struct { const char *n; int c; } N[] = {
        { "return", 13 }, { "enter", 13 }, { "esc", 27 }, { "escape", 27 }, { "space", ' ' },
        { "left", 8 }, { "backspace", 8 }, { "right", 0x15 }, { "up", 0x0B }, { "down", 0x0A },
        { "tab", 9 }, { "delete", 0x7F },
    };
    if (!k[1]) return (unsigned char)k[0];
    char low[32];
    int i = 0;
    for (; k[i] && i < 31; i++) low[i] = (char)tolower((unsigned char)k[i]);
    low[i] = 0;
    for (size_t j = 0; j < sizeof N / sizeof N[0]; j++) if (!strcmp(low, N[j].n)) return N[j].c;
    if (!strncmp(low, "ctrl-", 5) && low[5] >= 'a' && low[5] <= 'z' && !low[6]) return low[5] - 'a' + 1;
    return -1;
}

static int same(const char *a, const char *b) {        // case-blind
    while (*a && tolower((unsigned char)*a) == tolower((unsigned char)*b)) a++, b++;
    return !*a && !*b;
}

static void queue(int c) { if (s_kt - s_kh < (int)sizeof s_keys) s_keys[s_kt++ % sizeof s_keys] = (uint8_t)c; }

static void snap(const char *path, int w, int h) {
    uint32_t *px = malloc((size_t)w * h * 4);
    if (!px) { fail("out of memory"); return; }
    host_render(px, w, h);
    int r = write_png(path, px, w, h);
    free(px);
    if (r) { fail("cannot write the PNG"); return; }
    printf("{\"ok\":true,\"path\":"); jstr(path); printf(",\"w\":%d,\"h\":%d,", w, h); brief(); puts("}");
}

// Read commands until one sets time running; return to let the game go on.
static void command_loop(void) {
    char line[2048];
    for (;;) {
        fflush(stdout);
        if (!fgets(line, sizeof line, stdin)) { host_close(); exit(0); }
        line[strcspn(line, "\r\n")] = 0;
        char *cmd = line;
        while (*cmd == ' ') cmd++;
        if (!*cmd || *cmd == '#') continue;
        char *arg = cmd + strcspn(cmd, " ");
        if (*arg) *arg++ = 0;
        while (*arg == ' ') arg++;
        if (!strcmp(cmd, "quit")) { ok(); fflush(stdout); host_close(); exit(0); }
        else if (!strcmp(cmd, "state")) state_json();
        else if (!strcmp(cmd, "key")) {
            char *t; int bad = 0;
            for (t = strtok(arg, " "); t; t = strtok(NULL, " ")) {
                int c = key_code(t);
                if (c < 0) { fail(t); bad = 1; break; }
                queue(c);
            }
            if (!bad) ok();
        } else if (!strcmp(cmd, "type")) { for (char *c = arg; *c; c++) queue((unsigned char)*c); ok(); }
        else if (!strcmp(cmd, "click")) {
            int i = atoi(arg);
            if (!s_modern || i < 0 || i >= s_ms.sc.nopts) fail("no such choice");
            else { queue(modern_option_key(&s_ms.sc, i)); ok(); }
        } else if (!strcmp(cmd, "tap") || !strcmp(cmd, "tapopt")) {   // a tap at canvas pixels X Y, or on choice N
            float x = 0, y = 0, w, h;
            static Canvas cv;                        // the scene as it is now (384 wide), so the tap meets it
            if (s_modern) { cv_size(&cv, 384, 252); modern_draw(&cv, &s_ms.sc, -1, -1, 0, s_frame / 60.0, NULL); }
            int bad = !s_modern;
            if (!bad && cmd[3] == 'o') {
                bad = !modern_option_rect(atoi(arg), &x, &y, &w, &h);
                x += w / 2; y += h / 2;
            } else if (!bad) bad = sscanf(arg, "%f %f", &x, &y) != 2;
            if (bad) fail("tap X Y | tapopt N (modern)");
            else {
                int k[4], n = modern_tap(&s_ms.sc, x, y, k, 4);
                for (int j = 0; j < n; j++) queue(k[j]);
                printf("{\"ok\":true,\"keys\":%d,\"paused\":%s}\n", n, modern_paused() ? "true" : "false");
                fflush(stdout);
            }
        } else if (!strcmp(cmd, "esc")) {
            int k = !strcmp(arg, "yes") ? KEY_Y : !strcmp(arg, "no") ? KEY_N : KEY_ESCAPE;
            host_view_key(k);
            ok();
        } else if (!strcmp(cmd, "watch")) { s_watch = !strcmp(arg, "on"); ok(); }
        else if (!strcmp(cmd, "snap")) {
            char path[1024]; int w = s_w, h = s_h;
            if (sscanf(arg, "%1023s %d %d", path, &w, &h) < 1) { fail("snap PATH [W H]"); continue; }
            if (w < 1 || h < 1 || w > 4096 || h > 4096) { fail("bad size"); continue; }
            snap(path, w, h);
        } else if (!strcmp(cmd, "step")) {
            s_run = RUN_STEP; s_left = *arg ? atol(arg) : 1;
            if (s_left < 1) s_left = 1;
            return;
        } else if (!strcmp(cmd, "wait")) {
            s_run = RUN_WAIT; s_left = *arg ? atol(arg) : 36000;
            return;
        } else if (!strcmp(cmd, "until")) {
            char kind[64] = ""; long max = 36000;
            sscanf(arg, "%63s %ld", kind, &max);
            s_until = -1;
            if (!strcmp(kind, "any")) s_until = REQ_COUNT;
            if (!strcmp(kind, "field")) s_until = REQ_COUNT + 1;
            for (int k = 0; k < REQ_COUNT; k++) if (REQ_NAME[k] && same(kind, REQ_NAME[k])) s_until = k;
            if (s_until < 0) { fail("unknown request kind"); continue; }
            s_run = RUN_UNTIL; s_left = max; s_hit = 0;
            return;
        } else fail("unknown command");
    }
}

static void stop(const char *why) {
    s_run = RUN_NONE;
    printf("{\"ok\":true,\"stopped\":\"%s\",", why);
    brief();
    puts("}");
    command_loop();
}

// Once a frame of game time: type the next key when the last was read, then
// carry on with the run or stop and take commands.
static void idle(void *ctx) {
    (void)ctx;
    s_frame++;
    if (s_gap > 0) s_gap--;
    else if (s_kh != s_kt && !(s_m.kbd & 0x80) && s_m.kq_head == s_m.kq_tail) {
        mach_key_push(&s_m, s_keys[s_kh++ % sizeof s_keys]);
        s_gap = 2;
    }
    int keys_done = s_kh == s_kt && !(s_m.kbd & 0x80) && s_m.kq_head == s_m.kq_tail;
    switch (s_run) {
    case RUN_STEP: if (--s_left <= 0) stop("steps"); break;
    case RUN_WAIT:
        if (keys_done && s_m.waiting && s_gap == 0) stop("waiting");
        else if (--s_left <= 0) stop("limit");
        break;
    case RUN_UNTIL:
        if (s_until == REQ_COUNT + 1 ? hunt_running() : s_hit) stop("request");
        else if (--s_left <= 0) stop("limit");
        break;
    default: command_loop(); break;
    }
    while (host_view_modal()) {         // the quit dialog: the game waits, commands still run
        if (s_run != RUN_NONE) stop("modal");   // time cannot run under the dialog: say so at once
        else command_loop();
    }
}

static void program(Mach *m, void *arg) {
    (void)m; (void)arg;
    if (s_modern) modern_program(&s_ms);
    else menu_program(&s_sh);
}

int main(int argc, char **argv) {
    unsigned long long seed = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--modern")) s_modern = 1;
        else if (!strcmp(argv[i], "--touch")) s_touch = 1;
        else if (!strcmp(argv[i], "--legacy")) s_modern = 0;
        else if (!strcmp(argv[i], "--seed") && i + 1 < argc) seed = strtoull(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--size") && i + 1 < argc && sscanf(argv[++i], "%dx%d", &s_w, &s_h) == 2) {}
        else { fprintf(stderr, "usage: harness [--modern] [--touch] [--seed N] [--size WxH]\n"); return 2; }
    }
    if (!s_modern && s_w == 1152 && s_h == 756) { s_w = 1120; s_h = 768; }
    persist_default(&s_p);
    sh_power_on(&s_m);
    host_init_hidden(&s_m, s_w, s_h, idle, NULL);
    s_m.cycles = seed;
    s_m.frame_at = seed;
    ot_set_observer(observe, NULL);
    if (s_modern) {
        static MusicSynth music;                     // played, not heard: the scene says which tune
        music_init(&music, HOST_AUDIO_RATE, s_m.cycles);
        if (music_load(&music, &s_m) == MUSIC_STAB) s_ms.music = &music;
        s_ms.m = &s_m; s_ms.g = &s_g; s_ms.persist = &s_p;
        s_ms.sc.touch = s_touch;
        host_set_presenter(view_modern(&s_ms));
    } else {
        s_sh.m = &s_m; s_sh.g = &s_g; s_sh.persist = &s_p;
        host_set_presenter(view_legacy(&s_m));
    }
    puts("{\"ok\":true,\"ready\":true}");
    command_loop();
    host_run(program, NULL);
    host_close();
    return 0;
}
