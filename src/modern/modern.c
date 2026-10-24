// Modern mode's screens: every engine Req answered with a scene, and the
// modern main menu. The words are the original's; the layout is new.
#include "modern.h"
#include "draw.h"
#include "data.h"
#include "a2str.h"
#include "hunt.h"
#include "text.h"
#include "gfx.h"
#include <ctype.h>
#include <math.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void persist_save(const Persist *p, int which);         // host/main.c

#define DAY_CYCLES (A2_HZ / 2)          // a travel day on screen (the original's BASIC took about this)
#define DAY_PAUSE  (A2_HZ * 11 / 10)    // a travel day's pause, then a wagon step per 4 pixels the
#define WAGON_STEP (A2_HZ / 10)         // landmark moves (as legacy mode, trailscr.c)
#define KEY_RETURN 0x8D
#define KEY_ESC 0x9B

static MShell *S;                        // the running shell (one per process)
static int s_fx, s_ix;                   // FX, IX (OREGON TRAIL 3000): the leg's strip, Apple pixels
#define WAGON_X 191.0f                   // the wagon's 160x64 frames: its wheels (frame row 58) on the
#define WAGON_Y 17.0f                    // ground (y=76), its rear (column 145) at x=336, leaving the
#define WAGON_FRAMES 8                   // thief room behind it (the original's at the wagon's rear)
#define LANDMARK_STOP 200.0f             // a landmark's right edge on arrival: just short of the ox

// The landmark art (travel_icon_NN), modern mode's composition: where each one
// stands. ON_GROUND: the wagon's scale, beside the trail; ON_HORIZON: the
// backdrop's scale (drawn 2x), far off; WATER: a river from the horizon down to
// the hedge. y is the image's bottom edge on the canvas (the ground line is 76,
// the horizon 56, the hedge 92).
enum { ON_GROUND, ON_HORIZON, WATER };
static const struct { unsigned char kind, y; } LANDMARK_ART[18] = {
    [1]  = { WATER, 89 },              // Kansas River crossing: 3 px above the hedge
    [2]  = { WATER, 89 },              // Big Blue River crossing
    [3]  = { ON_GROUND, 76 },          // Fort Kearney
    [4]  = { ON_HORIZON, 56 },         // Chimney Rock
    [5]  = { ON_GROUND, 76 },          // Fort Laramie
    [6]  = { ON_GROUND, 76 },          // Independence Rock
    [7]  = { ON_HORIZON, 56 },         // South Pass: two snowy mountains far off, as the original's
    [8]  = { ON_GROUND, 76 },          // Fort Bridger
    [9]  = { WATER, 89 },              // Green River crossing
    [10] = { ON_GROUND, 97 },          // Soda Springs: its pool 20 px below the ground line
    [11] = { ON_GROUND, 76 },          // Fort Hall
    [12] = { WATER, 89 },              // Snake River crossing
    [13] = { ON_GROUND, 76 },          // Fort Boise
    [14] = { ON_HORIZON, 56 },         // the Blue Mountains
    [15] = { ON_GROUND, 76 },          // Fort Walla Walla
    [16] = { WATER, 89 },              // The Dalles
    [17] = { WATER, 92 },              // the Willamette Valley: its wide end on the hedge
};

// ---------------------------------------------------------------------------
// Scene helpers
// ---------------------------------------------------------------------------
static MScene *sc(void) { return &S->sc; }

// The words for a key, or for a tap when the player taps (MScene.touch).
#define TOUCH (sc()->touch)
#define SAY(keys, taps) (TOUCH ? (taps) : (keys))
#define PRESS_SPACE SAY("Press SPACE BAR to continue", "Tap to continue")
#define PRESS_RETURN SAY("Press RETURN to size up the situation", "Tap to size up the situation")

static void clear_panel(void) {
    MScene *s = sc();
    s->panel = 0; s->title[0] = s->body[0] = s->hint[0] = s->input[0] = s->allow[0] = 0;
    memset(s->opt_key, 0, sizeof s->opt_key);
    s->nopts = 0; s->opt_base = 1; s->prompt = MP_NONE; s->overlay[0] = 0; s->overlay_hide = 0;
    s->pic[0] = 0; s->rule = 0; s->nicon = 0; s->spot[0] = 0; s->keys_x = s->keys_y = 0;
    for (int i = 0; i < MS_OPTS; i++) s->opt_pic[i][0] = 0;
}

// A page: the text alone on the screen, as the original's text screens
// (with the dividers above and below the text where the original has them).
static void page(int rule) {
    clear_panel();
    sc()->panel = PANEL_PAGE;
    sc()->rule = rule;
}

static void set_bg(const char *id) {
    MScene *s = sc();
    snprintf(s->bg, MS_ID, "%s", id);
    s->travel = 0; s->nspr = 0; s->status = 0; s->map = 0; s->dot = 0; s->bg_hide = 0; s->veil = 0;
    s->bg_x = s->bg_y = 0; s->box_mid = 0; s->box_top = 0; s->bubbles = 0;
    s->ripple = 0; s->splash = 0; s->carve[0] = 0;
    memset(s->bar, 0, sizeof s->bar);
    s->tap_key = 0; s->nbtn = 0; s->hunting = 0;
    clear_panel();
}

static void title(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt); vsnprintf(sc()->title, sizeof sc()->title, fmt, ap); va_end(ap);
}

static void body(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt); vsnprintf(sc()->body, MS_TEXT, fmt, ap); va_end(ap);
}

static void body_add(const char *fmt, ...) {
    MScene *s = sc();
    size_t n = strlen(s->body);
    va_list ap; va_start(ap, fmt); vsnprintf(s->body + n, MS_TEXT - n, fmt, ap); va_end(ap);
}

static void opt(const char *fmt, ...) {
    MScene *s = sc();
    if (s->nopts >= MS_OPTS) return;
    va_list ap; va_start(ap, fmt); vsnprintf(s->opts[s->nopts++], 80, fmt, ap); va_end(ap);
}

// The picture for the choice just added (shown while the mouse is on it).
static void opt_pic(const char *id) {
    MScene *s = sc();
    if (s->nopts > 0) snprintf(s->opt_pic[s->nopts - 1], MS_ID, "%s", id);
}

static void pic(const char *id) { snprintf(sc()->pic, MS_ID, "%s", id); }

// The supply icons: Matt's items (1-5) and the fort store's (STORE_NAME order).
static const char *const MATT_ICON[6] = { "", "ui_icon_oxen", "ui_icon_food", "ui_icon_clothing",
                                          "ui_icon_bullets", "ui_icon_wheel" };
static const char *const PART_ICON[3] = { "ui_icon_wheel", "ui_icon_axle", "ui_icon_tongue" };
static const char *const STORE_ICON[7] = { "ui_icon_oxen", "ui_icon_clothing", "ui_icon_bullets",
                                           "ui_icon_wheel", "ui_icon_axle", "ui_icon_tongue", "ui_icon_food" };

static void sprite(const char *id, float x, float y, int flip) {
    MScene *s = sc();
    if (s->nspr >= MS_SPRITES) return;
    MSprite *p = &s->spr[s->nspr++];
    snprintf(p->id, MS_ID, "%s", id);
    p->x = x; p->y = y; p->flip = flip; p->bottom = 0; p->right = 0; p->scale = 1; p->size = 0; p->anchor = 0; p->above_box = 0; p->water = 0;
}

// A sprite placed by its bottom-right corner, at a whole-number scale.
static void sprite_by_corner(const char *id, float right, float bottom, int scale) {
    sprite(id, right, bottom, 0);
    MSprite *p = &S->sc.spr[S->sc.nspr - 1];
    p->right = p->bottom = 1;
    p->scale = scale;
}

// The original's 280x192 screen mapped onto the canvas (FLOAT, the travel strip).
static float rx(float x) { return x * 384.0f / 280.0f; }
static float ry(float y) { return y * MS_ART_H / 192.0f; }   // onto the art (the canvas's top 216 rows)

int modern_option_key(const MScene *s, int i) {
    if (s->prompt == MP_YESNO) return i == 0 ? '1' : '2';
    if (i >= 0 && i < MS_OPTS && s->opt_key[i]) return s->opt_key[i];
    return '0' + s->opt_base + i;
}

// ---------------------------------------------------------------------------
// Waiting for the player
// ---------------------------------------------------------------------------
// A key, as the original's readers take it: Ctrl-S turns the sound off or on;
// Esc twice in a row leaves the game for the menu (& INP / & SPACE / USR: GOTO
// 31000). Modern choices take their key at once, without the original's
// Return, so a Return straight after one (a player's habit, "1 Return") is let
// go rather than passed to the next screen (s_after_choice).
static int s_after_choice;
static const char *s_word_rest;                     // after Y or N: the rest of YES / NO, let go as typed
static void on_escape(Mach *m);

static int key(void) {
    Mach *m = S->m;
    static int esc;
    for (;;) {
        int k = mach_key_wait(m);
        if (k == 0x93) { m->sound = !m->sound; return 0; }          // Ctrl-S
        if (k == 0x9B) {                                            // Esc
            if (esc) { esc = 0; on_escape(m); }
            esc = 1;
            continue;
        }
        esc = 0;
        if (s_word_rest && *s_word_rest && toupper(k & 0x7F) == *s_word_rest) { s_word_rest++; continue; }   // "YES", "NO"
        s_word_rest = NULL;
        if (k == KEY_RETURN && s_after_choice) { s_after_choice = 0; continue; }
        s_after_choice = 0;
        return k;
    }
}

// A message that can be sized up (EVENT_END, PART_MSG): Space goes on, Return
// stops to size up the situation. With touch, two buttons say so. Returns
// whether it was Return.
static void wait_continue(void);
static int wait_or_size_up(void) {
    MScene *s = sc();
    if (!TOUCH) { wait_continue(); return (S->m->kbd & 0x7F) == 13; }
    s->panel = s->panel ? s->panel : PANEL_BOX;
    s->nopts = 0;
    opt("Continue"); opt("Size up the situation");
    s->opt_key[0] = ' '; s->opt_key[1] = 0x0D;
    s->prompt = MP_CHOOSE;
    s->hint[0] = 0;
    int ret;
    for (;;) {
        int k = key() & 0x7F;
        if (k == ' ' || k == '1') { ret = 0; break; }
        if (k == 13 || k == '2') { ret = 1; break; }
    }
    s->nopts = 0; s->prompt = MP_NONE;
    return ret;
}

// Show the panel and wait for Space, Return or a click.
static void wait_continue(void) {
    MScene *s = sc();
    s->panel = s->panel ? s->panel : PANEL_BOX;
    s->prompt = MP_CONTINUE;
    if (!s->hint[0]) snprintf(s->hint, sizeof s->hint, "%s", PRESS_SPACE);
    for (;;) {
        int k = key();
        if (k == 0xA0 || k == KEY_RETURN) break;
    }
    s->prompt = MP_NONE; s->hint[0] = 0;
}

// The scene alone: no panel, a small "Press SPACE BAR" at the bottom.
static void wait_look(void) {
    MScene *s = sc();
    s->panel = 0;
    s->prompt = MP_CONTINUE;
    snprintf(s->hint, sizeof s->hint, "%s", PRESS_SPACE);
    for (;;) {
        int k = key();
        if (k == 0xA0 || k == KEY_RETURN) break;
    }
    s->prompt = MP_NONE; s->hint[0] = 0;
}

// The music (music.c), where the original played its tunes. s_slot is the
// tune the original's buffer holds: the trail's (MS0) from Independence, each
// landmark's own from its arrival (MS<lm>, loaded whether or not the party
// looks around), the arrival's (MS17), the tombstone's (TS).
static int s_slot;

static void music_start(int t, int loop) {
    sc()->music = t;
    if (S->music && S->m->sound) music_play(S->music, t, loop, S->m->cycles);
}

static void music_end(void) {
    sc()->music = -1;
    if (S->music) music_stop(S->music);
}

// The original's "Press SPACE BAR" with the tune playing over and over
// (sh_space_tune, OREGON TRAIL:950-958): Space or Return (or a click) goes
// on, the tune fading out; Ctrl-S turns the sound off or on.
static void wait_tune(void) {
    Mach *m = S->m;
    MScene *s = sc();
    s->panel = s->panel ? s->panel : PANEL_BOX;
    s->prompt = MP_CONTINUE;
    snprintf(s->hint, sizeof s->hint, "%s", PRESS_SPACE);
    mach_key_clear(m);
    music_start(s_slot, 1);
    for (;;) {                                      // 956: any key but Ctrl-S goes on
        int was = m->sound, k = key();
        if (!k) {                                   // 957: Ctrl-S
            if (m->sound && !was) music_start(s_slot, 1);
            if (!m->sound && was) music_end();
            continue;
        }
        break;
    }
    music_end();
    s->prompt = MP_NONE; s->hint[0] = 0;
}

// Numbered choices opt_base..; a digit picks at once (so does a click).
static int choose(void) {
    MScene *s = sc();
    s->panel = s->panel ? s->panel : PANEL_BOX;
    s->prompt = MP_CHOOSE;
    if (!s->hint[0]) snprintf(s->hint, sizeof s->hint, "What is your choice?");
    for (;;) {
        int k = key() & 0x7F;
        int c = k - '0';
        if (c >= s->opt_base && c < s->opt_base + s->nopts) { s->prompt = MP_NONE; s->hint[0] = 0; s_after_choice = 1; return c; }
        if (k == 1 && s->opt_base == 1 && s->nopts == 4 && !S->in_game) return -1;   // Ctrl-A at the main menu
    }
}

static int yesno(void) {
    MScene *s = sc();
    s->panel = s->panel ? s->panel : PANEL_BOX;
    s->prompt = MP_YESNO; s->nopts = 0;
    opt("Yes"); opt("No");
    for (;;) {
        int k = key() & 0x7F;
        // the original reads a word (& INP,3,"YESNOyesno"): YES or NO typed out still answers once
        // or 1 for Yes and 2 for No, as every other choice is picked
        if (k == 'y' || k == 'Y' || k == '1') { s->prompt = MP_NONE; s->nopts = 0; s_after_choice = 1; s_word_rest = k == '1' ? NULL : "ES"; return 1; }
        if (k == 'n' || k == 'N' || k == '2') { s->prompt = MP_NONE; s->nopts = 0; s_after_choice = 1; s_word_rest = k == '2' ? NULL : "O"; return 0; }
    }
}

// A typed field: letters in `allow` (ranges as in & INP), up to max chars.
static int allowed(int c, const char *allow) {
    for (const char *p = allow; *p; p++) {
        if (*p == '-' && p[1] && p[2]) { if (c >= p[1] && c <= p[2]) return 1; p += 2; }
        else if (c == *p) return 1;
    }
    return 0;
}

static void text_input(char *out, int max, const char *allow, int required) {
    MScene *s = sc();
    s->panel = s->panel ? s->panel : PANEL_BOX;
    s->prompt = MP_TEXT;
    s->input[0] = 0;
    snprintf(s->allow, sizeof s->allow, "%s", allow);
    int n = 0;
    for (;;) {
        int k = key() & 0x7F;
        if (k == 13) { if (n || !required) break; continue; }
        if ((k == 8 || k == 127) && n) { s->input[--n] = 0; continue; }
        if (n < max && k >= 32 && allowed(k, allow)) { s->input[n++] = (char)k; s->input[n] = 0; }
    }
    memcpy(out, s->input, (size_t)n + 1);       // n <= max, so out needs max + 1
    s->prompt = MP_NONE;
}

static int number(int lo, int hi) {
    char buf[64];
    for (;;) {
        text_input(buf, hi >= 1000 ? 4 : hi >= 100 ? 3 : hi >= 10 ? 2 : 1, "-09", 1);
        int v = atoi(buf);
        if (v >= lo && v <= hi) return v;
    }
}

// ---------------------------------------------------------------------------
// Travel view
// ---------------------------------------------------------------------------
// 260: the landmark's name without a leading "the "
static const char *lm_name(int lm) {
    return strncmp(LM_NAME[lm], "the ", 4) ? LM_NAME[lm] : LM_NAME[lm] + 4;
}

static const char *td(const Game *g, char *buf) {
    // after the raft the year is PEEK(901), the year less 1800 (WIN:601 prints "18" before it)
    sprintf(buf, "%s %d, %d", MONTH[g->am - 1], g->ad, g->ay < 100 ? g->ay + 1800 : g->ay);
    return buf;
}

static void status_fill(void) {
    Game *g = S->g;
    MScene *s = sc();
    char n[24];
    s->status = 1;
    td(g, s->st[0]);
    snprintf(s->st[1], 80, "%s", WEATHER[g->w]);
    snprintf(s->st[2], 80, "%s", HEALTH[(int)((g->h > 139 ? 139 : g->h) / 35)]);
    snprintf(s->st[3], 80, "%s pounds", a2_str(g->pf, n));      // 320: the original's words
    snprintf(s->st[4], 80, "%s miles", a2_str(floor(g->d + .5), n));
    snprintf(s->st[5], 80, "%s miles", a2_str(floor(g->m + .5), n));
}

// 310: the travel strip. The original drew its horizon (PRAIRIE.IMA 4, or
// MOUNTAINS.IMA 4 past Fort Laramie) and a ground band in the ground's colour
// (CC); here one backdrop for each. The landmark's icon at (IX, 31), the wagon
// at (186, 32).
static void travel_view(void) {
    static const char *const HORIZON[2] = { "prairie", "mountains" };
    static const char *const GROUND[3] = { "grass", "dry", "snow" };
    Game *g = S->g;
    MScene *s = sc();
    char id[MS_ID];
    int mountains = g->lm >= 5 && !(g->lm == 5 && g->nm == 5);
    int ground = g->as >= 1 ? 2 : g->ar <= .2 ? 1 : 0;
    snprintf(id, sizeof id, "travel_bg_%s_%s", HORIZON[mountains], GROUND[ground]);
    set_bg(id);
    s->travel = 1;
    s->pan = rx((float)s_ix);
    if (!s->wagon_frame) s->wagon_frame = 1;
    if (g->nm > 0) {                                             // it slides in as IX does, stopping at
        snprintf(id, sizeof id, "travel_icon_%02d", g->nm);     // LANDMARK_STOP, and stays on arrival
        int k = LANDMARK_ART[g->nm].kind == ON_HORIZON ? 2 : 1;
        const CvImage *im = modern_image(id);
        float stop = LANDMARK_STOP;                              // far off, a wide one (the Blue Mountains)
        if (k == 2 && im && 8 + im->w * k > stop) stop = 8.0f + im->w * k;   // stops whole in view, behind the wagon
        sprite_by_corner(id, stop + rx((float)s_ix) - rx((float)s_fx), LANDMARK_ART[g->nm].y, k);
        s->spr[s->nspr - 1].above_box = LANDMARK_ART[g->nm].kind == ON_GROUND;   // a river runs on under it
    }
    snprintf(id, sizeof id, "travel_wagon_%d", s->wagon_frame);
    sprite(id, WAGON_X, WAGON_Y, 0);                             // walking in place, as at (186, 32)
    status_fill();
}

// RIVER.LIB: every river screen is headed by the crossing and the date.
static void river_title(void) {
    char buf[40];
    title("%s\n%s", lm_name(S->g->lm), td(S->g, buf));
}

// 300/305: the landmark's picture, whole, its name and the date in a band
// under it (L<n>.PCK, the caption at 160-180).
static void landmark_view(void) {
    Game *g = S->g;
    char id[MS_ID], buf[40];
    snprintf(id, sizeof id, "scene_landmark_%02d", g->lm);
    set_bg(id);
    sc()->panel = PANEL_CAPTION;
    title("%s", lm_name(g->lm));
    body("%s", td(g, buf));
}

// ---------------------------------------------------------------------------
// Messages
// ---------------------------------------------------------------------------
static const char *ART_ID[] = { "", "event_snowdrift", "event_thunderstorm", "event_blizzard",
                                "event_hail", "event_fire", "event_fruit", "event_thief" };

// The event's picture on the travel scene, where the original drew it (the
// original's 280-wide strip; its wagon at (186, 32), ours at WAGON_X): the fire
// and the broken part take the wagon's place (they are the team and the
// wagon), the fruit and the snowdrift lie on the ground beside it, the thief
// stands at the wagon's rear (the original's at x 263, where its wagon ends),
// and the weather fills the sky at twice its size over a
// veil. Elsewhere (no travel scene) it is the panel's picture.
#define FROM_WAGON(x) (WAGON_X + ((x) - 186) * 384.0f / 280.0f)

static void wagon_becomes(const char *id, float x, float y) {
    MScene *s = sc();
    for (int i = 0; i < s->nspr; i++)
        if (!strncmp(s->spr[i].id, "travel_wagon_", 13)) {
            snprintf(s->spr[i].id, MS_ID, "%s", id);
            s->spr[i].x = x; s->spr[i].y = y;
        }
}

static void event_art(int art) {
    MScene *s = sc();
    if (art <= 0 || art >= 8) return;
    if (!s->travel) { snprintf(s->overlay, MS_ID, "%s", ART_ID[art]); return; }
    switch (art) {
    // the pictures' own boxes (their opaque parts) on the wagon's: right edge at
    // column 145 of its frame, the wheels on row 58
    case ART_FIRE: wagon_becomes("event_fire", WAGON_X + 145 - 205, WAGON_Y + 58 - 60); break;
    case ART_FRUIT: sprite("event_fruit", FROM_WAGON(150), 78, 0); s->spr[s->nspr - 1].bottom = 1; break;
    case ART_SNOW: sprite("event_snowdrift", FROM_WAGON(130), 80, 0); s->spr[s->nspr - 1].bottom = 1; break;
    case ART_THIEF:                              // its left edge (column 12) past the rear, its feet (row 64) on the ground
        sprite("event_thief", WAGON_X + 147 - 12, WAGON_Y + 59 + 72 - 64, 0); s->spr[s->nspr - 1].bottom = 1; break;
    case ART_STORM: case ART_BLIZZARD: case ART_HAIL:                // the original's (105,0) and (118,0)
        s->veil = art == ART_BLIZZARD ? 2 : 1;
        sprite(ART_ID[art], rx(art == ART_HAIL ? 118 : 105), 4, 0);
        s->spr[s->nspr - 1].scale = 2;
        break;
    }
}

static void message(const char *text, const char (*lines)[OT_LINE_MAX], int n, int art) {
    MScene *s = sc();
    clear_panel();
    s->panel = PANEL_BOX;
    event_art(art);
    body("%s", text);
    for (int i = 0; i < n; i++) body_add("\n   %s", lines[i]);
}

// ---------------------------------------------------------------------------
// The engine's requests
// ---------------------------------------------------------------------------
static int occupation(void) {
    for (;;) {
        set_bg("scene_title");
        sc()->panel = PANEL_PAGE;
        body("Many kinds of people made the trip to Oregon.\n\nYou may:");
        opt("Be a banker from Boston"); opt("Be a carpenter from Ohio");
        opt("Be a farmer from Illinois"); opt("Find out the differences between these choices");
        int c = choose();
        if (c != 4) return c;
        clear_panel(); sc()->panel = PANEL_PAGE;
        body("Traveling to Oregon isn't easy!  But if you're a banker, you'll have more money for "
             "supplies and services than a carpenter or a farmer.\n\nHowever, the harder you have to "
             "try, the more points you deserve!  Therefore, the farmer earns the greatest number of "
             "points and the banker earns the least.");
        wait_continue();
    }
}

static void names(char n[5][OT_NAME_MAX]) {
    char buf[OT_NAME_MAX];
    set_bg("scene_title");
    sc()->panel = PANEL_PAGE;
    body("What is the first name of the wagon leader?");
    text_input(buf, 9, "-AZ-az '.-", 1);
    snprintf(n[0], OT_NAME_MAX, "%s", buf);
    for (int l = 1; l <= 4; l++) {
        body("What are the first names of the four other members in your party?\n\n"
             "Member %d (press Return for %s):", l + 1, n[l]);
        text_input(buf, 9, "-AZ-az '.-", 0);
        if (!buf[0]) break;
        snprintf(n[l], OT_NAME_MAX, "%s", buf);
    }
    for (;;) {
        body("Your party:\n\n  1. %s\n  2. %s\n  3. %s\n  4. %s\n  5. %s\n\nAre these names correct?",
             n[0], n[1], n[2], n[3], n[4]);
        if (yesno()) break;
        body("Change which name?");
        sc()->nopts = 0;
        for (int i = 0; i < 5; i++) opt("%s", n[i]);
        int z = choose() - 1;
        body("New name for %s:", n[z]);
        text_input(buf, 9, "-AZ-az '.-", 0);
        if (buf[0]) snprintf(n[z], OT_NAME_MAX, "%s", buf);
    }
}

static int month(void) {
    static const char *const M[6] = { "", "March", "April", "May", "June", "July" };
    for (;;) {
        set_bg("scene_landmark_00");
        sc()->panel = PANEL_PAGE;
        body("It is 1848.  Your jumping off place for Oregon is Independence, Missouri.  You must "
             "decide which month to leave Independence.");
        for (int l = 1; l <= 5; l++) opt("%s", M[l]);
        opt("Ask for advice");
        int c = choose();
        if (c != 6) return c;
        clear_panel(); sc()->panel = PANEL_PAGE;
        body("You attend a public meeting held for \"folks with the California - Oregon fever.\"  "
             "You're told:\n\nIf you leave too early, there won't be any grass for your oxen to eat.  "
             "If you leave too late, you may not get to Oregon before winter comes.  If you leave at "
             "just the right time, there will be green grass and the weather will still be cool.");
        wait_continue();
    }
}

static void matt_intro(long cents) {
    char k[24];
    set_bg("scene_landmark_00");
    page(0);
    body("Before leaving Independence, you should buy equipment and supplies.  You have $%s in "
         "cash, but you don't have to spend it all now.", a2_money(cents, k));
    wait_continue();
    body("You can buy whatever you need at Matt's General Store.");
    wait_continue();
    set_bg("scene_matt_store");                  // the store and Matt first, then his greeting
    snprintf(sc()->overlay, MS_ID, "portrait_matt");
    wait_look();
    title("Matt's General Store");
    body("Hello, I'm Matt.  So you're going to Oregon!  I can fix you up with what you need:\n\n"
         "- a team of oxen to pull your wagon\n- clothing for both summer and winter\n"
         "- plenty of food for the trip\n- ammunition for your rifles\n- spare parts for your wagon");
    wait_continue();
}

static long matt_cost(const Matt *mt, int l) {
    switch (l) {
    case 1: return 4000L * mt->yokes;
    case 2: return 20L * mt->food;
    case 3: return 1000L * mt->clothing;
    case 4: return 200L * mt->boxes;
    case 5: return 1000L * (mt->parts[0] + mt->parts[1] + mt->parts[2]);
    }
    return 0;
}

static int matt_menu(int change) {
    const Matt *mt = &S->g->matt;
    char a[24], b[24];
    static const char *const ITEMS[6] = { "", "Oxen", "Food", "Clothing", "Ammunition", "Spare parts" };
    set_bg("scene_matt_store");
    snprintf(sc()->overlay, MS_ID, "portrait_matt");
    sc()->panel = PANEL_TALL;
    title("Matt's General Store\n%s 1, 1848", MONTH[S->g->mem[902] - 1]);   // the date under the name: clear of the portrait
    body("Total bill: $%s\nAmount you have: $%s", a2_money(mt->bill_cents, a), a2_money(S->g->my_cents, b));
    for (int l = 1; l <= 5; l++) { opt("%-12s $%s", ITEMS[l], a2_money(matt_cost(mt, l), a)); opt_pic(MATT_ICON[l]); }
    opt("Leave store");
    pic("ui_icon_money");
    snprintf(sc()->hint, sizeof sc()->hint, "Which item would you like to %s?%s", change ? "change" : "buy", SAY("  (Space bar: leave)", ""));
    MScene *s = sc();
    s->prompt = MP_CHOOSE;
    for (;;) {                                      // 250-255: 1-5, or the space bar at once
        int k = key() & 0x7F;
        if (k == ' ' || k == '6') { s->prompt = MP_NONE; s->hint[0] = 0; s_after_choice = 1; return 0; }
        if (k >= '1' && k <= '5') { s->prompt = MP_NONE; s->hint[0] = 0; s_after_choice = 1; return k - '0'; }
    }
}

static int matt_qty(int item, int part) {
    static const char *const SP[3] = { "wagon wheel", "wagon axle", "wagon tongue" };
    clear_panel();
    sc()->panel = PANEL_TALL;
    snprintf(sc()->overlay, MS_ID, "portrait_matt");
    pic(item == 5 ? PART_ICON[part] : MATT_ICON[item]);
    char bill[24];                                   // BUY SUPPLIES: "Bill so far" under each question
    snprintf(sc()->hint, sizeof sc()->hint, "Bill so far: $%s", a2_money(S->g->matt.bill_cents, bill));
    switch (item) {
    case 1: body("There are 2 oxen in a yoke; I recommend at least 3 yoke.  I charge $40 a yoke.\n\nHow many yoke do you want?"); return number(1, 9);
    case 2: body("I recommend you take at least 200 pounds of food for each person in your family.  I see that you have 5 people in all.  You'll need flour, sugar, bacon, and coffee.  My price is 20 cents a pound.\n\nHow many pounds of food do you want?"); return number(0, 9999);
    case 3: body("You'll need warm clothing in the mountains.  I recommend taking at least 2 sets of clothes per person.  Each set is $10.00.\n\nHow many sets of clothes do you want?"); return number(0, 99);
    case 4: body("I sell ammunition in boxes of 20 bullets.  Each box costs $2.00.\n\nHow many boxes do you want?"); return number(0, 99);
    }
    body("It's a good idea to have a few spare parts for your wagon.  A wagon wheel, axle or tongue is $10 each.\n\nHow many %ss?", SP[part]);
    return number(0, 9);
}

static void matt_msg(int msg, int part) {
    static const char *const SP[3] = { "wagon wheel", "wagon axle", "wagon tongue" };
    char a[24], b[24];
    clear_panel();
    switch (msg) {
    case MATT_MSG_FOOD_LIMIT: body("Your wagon may only carry 2000 pounds of food."); break;
    case MATT_MSG_PART_LIMIT: body("Your wagon may only carry 3 %ss.", SP[part]); break;
    case MATT_MSG_OVER_BUDGET:
        body("Okay, that comes to a total of $%s.  But I see that you only have $%s.  We'd better go over the list again.",
             a2_money(S->g->matt.bill_cents, a), a2_money(S->g->my_cents, b));
        break;
    case MATT_MSG_NO_OXEN: body("Don't forget, you'll need oxen to pull your wagon."); break;
    }
    wait_continue();
}

static void matt_depart(void) {
    clear_panel();
    body("Well then, you're ready to start.  Good luck!  You have a long and difficult journey ahead of you.");
    wait_continue();
}

// The action menu (4000-4060): the same numbering as the original.
static int action_menu(void) {
    Game *g = S->g;
    char buf[40];
    page(0);                                     // 1000-1100: the place and date, the four lines, the choices
    if (g->ll) title("%s", td(g, buf));
    else title("%s\n%s", lm_name(g->lm), td(g, buf));
    body("Weather: %s\nHealth: %s\nPace: %s\nRations: %s\n\nYou may:", WEATHER[g->w],
         HEALTH[(int)((g->h > 139 ? 139 : g->h) / 35)], PACE[g->p - 1], RATION[g->r - 1]);
    for (int l = 0; l <= 6; l++) opt("%s", ACTION[l]);
    if (!g->ll) { opt("%s", ACTION[7]); if (LM_TYPE[g->lm] == 1) opt("%s", ACTION[8]); }
    else opt("%s", ACTION[9]);
    return choose();
}

// TRADE.LIB:50000: the original shows your supplies beside every trade.
static void trade_have(void) {
    const Game *g = S->g;
    long n[7];
    for (int l = 2; l <= 8; l++) n[l - 2] = (long)floor(g->inv[l] + .51);
    body_add("\n\nYou have %ld ox%s, %ld set%s of clothing, %ld bullet%s, %ld wheel%s, %ld axle%s, %ld tongue%s and %ld pound%s of food.",
             n[0], n[0] != 1 ? "en" : "", n[1], n[1] != 1 ? "s" : "", n[2], n[2] != 1 ? "s" : "", n[3], n[3] != 1 ? "s" : "",
             n[4], n[4] != 1 ? "s" : "", n[5], n[5] != 1 ? "s" : "", n[6], n[6] != 1 ? "s" : "");
}

static void supplies(void) {
    Game *g = S->g;
    char k[24];
    clear_panel();
    sc()->panel = PANEL_PAGE;
    title("Your Supplies");
    body("");
    for (int l = 2; l <= 8; l++) body_add("%-20s %4.0f\n", ITEM[l], floor(g->inv[l] + .51));
    body_add("%-20s $%s", "money left", a2_money(g->my_cents, k));
    static const char *const PICS[8] = { "ui_icon_oxen", "ui_icon_clothing", "ui_icon_bullets", "ui_icon_wheel",
                                         "ui_icon_axle", "ui_icon_tongue", "ui_icon_food", "ui_icon_money" };
    MScene *s = sc();                                // 4100: the goods drawn beside the list
    for (int i = 0; i < 8; i++) snprintf(s->icon[s->nicon++], MS_ID, "%s", PICS[i]);
    wait_continue();
}

// The original map (M0.PCK) without its marks, one Apple pixel per pixel,
// at (MAP_X, MAP_Y) (tools/art.py local); the route and marks drawn over it.
#define MAP_X 52                           // in the middle (art.py MAP_X)
#define MAP_Y 12

static void map_line(MScene *s, double x0, double y0, double x1, double y1) {
    if (s->nline < MS_LINES)
        s->line[s->nline++] = (MLine){ (float)x0 + MAP_X, (float)y0 + MAP_Y, (float)x1 + MAP_X, (float)y1 + MAP_Y };
}

// 4200 + MAP.LIB, as trail_map draws it in legacy mode
void modern_map_scene(MScene *s, const Game *g) {
    s->map = 1;
    s->nline = s->nmark = 0;
    for (int l = 0; l < 18; l++) {
        int k = l == 0 ? MK_START : l == 17 ? MK_FINISH : LM_TYPE[l] == 1 ? MK_FORT : MK_LANDMARK;
        if (LM_TYPE[l] == 2) continue;                          // the rivers are unmarked
        s->mark[s->nmark++] = (MMark){ (float)MAP_XY[l][0] + MAP_X, (float)MAP_XY[l][1] + MAP_Y, k };
    }
    if (g->q1 - 1)
        for (int l = 0; l <= g->q1 - 2; l++)
            map_line(s, MAP_XY[g->q[l]][0], MAP_XY[g->q[l]][1],
                     MAP_XY[g->q[l + 1]][0], MAP_XY[g->q[l + 1]][1]);
    if (g->lm != g->nm) {                                        // 50010
        double x1 = MAP_XY[g->nm][0], x2 = MAP_XY[g->lm][0];
        double y1 = MAP_XY[g->nm][1], y2 = MAP_XY[g->lm][1];
        double sl = (y2 - y1) / (x2 - x1);
        double z = g->d * (x2 - x1) / g->dd;
        double x = z + x1, y = y2 - sl * (x2 - x);
        map_line(s, x2, y2, x, y);
    }
}

static void map_screen(void) {
    MScene *s = sc();
    set_bg("scene_map");
    modern_map_scene(s, S->g);
    wait_look();                                 // the legend and the prompt under it (draw.c draw_map)
}

static int pace(void) {
    Game *g = S->g;
    for (;;) {
        page(1);
        title("Change pace (currently \"%s\")", PACE[g->p - 1]);
        body("The pace at which you travel can change.  Your choices are:");
        for (int l = 0; l < 3; l++) opt("a %s pace", PACE[l]);
        opt("find out what these different paces mean");
        int c = choose();
        if (c != 4) return c;
        page(1);
        body("%s - You travel about 8 hours a day, taking frequent rests.  You take care not to get too tired.\n\n"
             "%s - You travel about 12 hours a day, starting just after sunrise and stopping shortly before sunset.  You stop to rest only when necessary.  You finish each day feeling very tired.\n\n"
             "%s - You travel about 16 hours a day, starting before sunrise and continuing until dark.  You almost never stop to rest.  You do not get enough sleep at night.  You finish each day feeling absolutely exhausted, and your health suffers.",
             PACE[0], PACE[1], PACE[2]);
        wait_continue();
    }
}

static int rations(void) {
    Game *g = S->g;
    page(1);
    title("Change food rations (currently \"%s\")", RATION[g->r - 1]);
    body("The amount of food the people in your party eat each day can change.  These amounts are:");
    opt("%s - meals are large and generous.", RATION[0]);
    opt("%s - meals are small, but adequate.", RATION[1]);
    opt("%s - meals are very small; everyone stays hungry.", RATION[2]);
    return choose();
}

static void talk(int speaker) {
    Game *g = S->g;
    Mach *m = S->m;
    static uint8_t seq[32768];
    char name[24], a[256] = "", z[256] = "", v[256] = "";
    snprintf(name, sizeof name, "OREGON%d.SEQ", g->s + 1);
    int n = m->load ? m->load(m->load_ctx, name, seq, sizeof seq) : -1;
    if (n > 0) {
        int pos = (g->lm - g->s * 5) * 768 + speaker * 256;
        seq_read(seq, n, &pos, a, sizeof a);
        if (!seq_read(seq, n, &pos, z, sizeof z)) seq_read(seq, n, &pos, v, sizeof v);
    }
    page(1);
    body("%s tells you:\n\n\"%s%s\"", a, z, v);
    wait_continue();
}

static int store_menu(void) {
    Game *g = S->g;
    char k[24];
    clear_panel();
    sc()->panel = PANEL_PAGE;
    title("%s - store", LM_NAME[g->lm]);
    body("You have $%s to spend.", a2_money(g->my_cents, k));
    for (int l = 0; l <= 6; l++) {
        opt("%-14s $%s per %s", STORE_NAME[l], a2_money(store_price_cents(l + 1, g->lm), k), STORE_UNIT[l]);
        opt_pic(STORE_ICON[l]);
    }
    opt("Leave store");
    pic("ui_icon_money");
    snprintf(sc()->hint, sizeof sc()->hint, "Which number?");
    return choose();
}

static void store_msg(int msg, int l) {
    page(0);
    switch (msg) {
    case STORE_MSG_AFFORD: body("You cannot afford that."); break;
    case STORE_MSG_AFFORD_MANY: body("You cannot afford that many."); break;
    case STORE_MSG_OXEN_CAP: body("You may only take 20 oxen."); break;
    case STORE_MSG_FOOD_CAP: body("Your wagon may only carry 2000 pounds of food."); break;
    case STORE_MSG_PART_CAP: body("Your wagon may only carry 3 %s.", ITEM[l + 1]); break;
    }
    wait_continue();
}

// FLOAT, drawn with the modern sprites at the original's positions.

static int raft_frame(void) {
    Game *g = S->g;
    Mach *m = S->m;
    const Raft *r = &g->raft;
    MScene *s = sc();
    set_bg("scene_columbia");
    // placed by what collides (finish.c): the raft's hull on its strip (x+6..26,
    // y+18..25 of its box), each rock's top on its strip (x+11..31, y+1..7), so
    // what the picture shows touching is what hits
    sprite("raft_raft", rx(84 + 8 * r->hp + 16), ry(5 * r->hp - 6 + 21.5f), 0);
    s->spr[s->nspr - 1].anchor = 1; s->spr[s->nspr - 1].ax = 32; s->spr[s->nspr - 1].ay = 34;
    for (int z = 0; z < 2; z++) if (r->fl[z]) {
        sprite("raft_rock", rx((float)r->rx[z] + 21), ry((float)r->ry[z] + 4), 0);
        s->spr[s->nspr - 1].anchor = 1; s->spr[s->nspr - 1].ax = 25; s->spr[s->nspr - 1].ay = 6;
    }
    if (r->fl[2]) sprite("raft_sign", rx(r->sx), ry(r->sy), 0);
    if (r->fl[3]) sprite("raft_landing", rx(r->tx), ry(r->ty), 0);
    snprintf(s->bar[0], sizeof s->bar[0], "The Columbia River");
    snprintf(s->bar[2], sizeof s->bar[2], "%s", SAY("Use the arrow keys to guide your raft", "Tap left or right of the raft to steer"));
    if (TOUCH) {                                 // the screen's halves steer: left (Ctrl-H), right (Ctrl-U)
        s->btn[0] = (MButton){ .x = 0, .y = 0, .w = 192, .h = MS_ART_H, .key = 0x08 };
        s->btn[1] = (MButton){ .x = 192, .y = 0, .w = 192, .h = MS_ART_H, .key = 0x15 };
        s->nbtn = 2;
    }
    mach_advance(m, A2_HZ / 3);
    int k = mach_key_peek(m);
    if (k & 0x80) { mach_key_clear(m); return k; }
    return 0;
}

// The hunt: the original's game (hunt.c, unchanged) drawn with the modern
// sprites. Before each frame the field is read back (hunt_view) and rebuilt as
// sprites on the zone's ground, at the original's positions scaled to the
// canvas: each stands with its feet (hunt_anchor.h, measured from the art) at
// the middle of its box's bottom edge, so a turn or a step moves the art no
// more than the original moved its box. The hunter's aims are the field's
// directions in its 3/4 view (up = north, away from the viewer). The modern
// hunter faces right, so he is mirrored where the original's (facing left) is
// not; the animals face right in both.
static const char *const HUNT_ANIMAL[6] = { "deer_buck", "bear", "buffalo", "deer", "rabbit", "squirrel" };
static const char *const HUNT_TERRAIN[5] = { "tree", "pine", "grass", "cactus", "rock" };
static int s_hunt_last[2], s_hunt_step[2];          // the animals' 4-frame cycle

#include "hunt_anchor.h"
static int anchor_cmp(const void *k, const void *e) { return strcmp(k, ((const HuntAnchor *)e)->id); }
static const HuntAnchor *hunt_anchor(const char *id) {
    return bsearch(id, HUNT_ANCHOR, sizeof HUNT_ANCHOR / sizeof HUNT_ANCHOR[0], sizeof HUNT_ANCHOR[0], anchor_cmp);
}

// The original's pose -> the modern aim. The art is the one hunter turned to
// each direction (hunt_hunter_dirs): north and south face the viewer's way,
// east and the south-east face right, and the north-east view faces left
// (*left), so it is mirrored the other way round.
static const char *hunter_aim(int img, int *left) {
    *left = 0;
    switch ((img - 1) / 3) {
    case 0: return "up";
    case 1: *left = 1; return "upright";            // the original's up-left, mirrored
    case 2: return "right";
    case 3: return "downright";
    }
    return "down";
}

// Touch in the hunt (modern_tap sets MScene.hunt_goal): the hunt is the
// original's game, steered by its own keys. A tap walks the hunter to the
// spot: a distance map over the field (the hunt's own field and boxes: he may
// not leave it or touch scenery or a carcass, REQ-192i) is made from the spot,
// and he is aimed (the expert key for a direction) and started (Return) in
// whichever of his eight steps gets furthest down the map in a straight line,
// aimed anew when that line stops gaining, and stopped (Return) there. A
// double-tap stops him, turns him that way and fires (Space) once he faces
// it. After each key the game is given a few frames to take it.
static const int STEP_X[8] = { 2, 2, 0, -2, -2, -2, 0, 2 }, STEP_Y[8] = { 0, -1, -1, -1, 0, 1, 1, 1 };

// The direction whose step (and shot) points nearest (dx, dy), canvas pixels:
// a step is 2 pixels across and 1 row up or down, so the diagonals are shallow.
static int aim_dir(float dx, float dy) {
    int best = 0;
    float bv = -1e9f, len = sqrtf(dx * dx + dy * dy) + 1e-6f;
    for (int d = 0; d < 8; d++) {
        float mx = rx((float)STEP_X[d]), my = ry((float)STEP_Y[d]), ml = sqrtf(mx * mx + my * my);
        float c = (mx * dx + my * dy) / (ml * len);
        if (c > bv) { bv = c; best = d; }
    }
    return best;
}

#define FIELD_W 280
#define FIELD_H 192
#define CELLS_X (FIELD_W / 2)
static short s_dist[FIELD_H][CELLS_X];          // steps to the spot, -1 none; x = 2 * cell + parity
static int s_par, s_hw, s_hh;                     // the hunter's x parity and box size, as planned

static int hunter_box(const HuntView *v, int *x1, int *y1, int *x2, int *y2) {
    for (int i = 0; i < v->n; i++)
        if (v->s[i].kind == HV_HUNTER) { *x1 = v->s[i].x1; *y1 = v->s[i].y1; *x2 = v->s[i].x2; *y2 = v->s[i].y2; return 1; }
    return 0;
}

// The hunter's box at (x, y) (its top left) is in the field and touches no
// scenery or carcass (the hunt's own inclusive test).
static int free_at(const HuntView *v, int x, int y) {
    if (x < 0 || y < 0 || x + s_hw >= FIELD_W || y + s_hh >= FIELD_H) return 0;
    for (int i = 0; i < v->n; i++) {
        const HuntSprite *p = &v->s[i];
        if (p->kind != HV_SCENERY && p->kind != HV_DEAD) continue;
        if (x + s_hw >= p->x1 && p->x2 >= x && y + s_hh >= p->y1 && p->y2 >= y) return 0;
    }
    return 1;
}

static short dist_at(int x, int y) {
    int c = (x - s_par) / 2;
    if (y < 0 || y >= FIELD_H || c < 0 || c >= CELLS_X || (x - s_par) % 2) return -1;
    return s_dist[y][c];
}

// The distance map from the spot (fx, fy) (field pixels): breadth first
// over the hunter's positions, from those within reach of the spot (the
// nearest free ones when the spot itself is blocked).
static void plan(const HuntView *v, float fx, float fy) {
    static int qx[FIELD_H * CELLS_X], qy[FIELD_H * CELLS_X];
    int x1, y1, x2, y2;
    if (!hunter_box(v, &x1, &y1, &x2, &y2)) return;
    s_par = x1 & 1; s_hw = x2 - x1; s_hh = y2 - y1;
    memset(s_dist, 0xFF, sizeof s_dist);
    int n = 0;
    for (int r = 6; r <= 60 && !n; r += 6)
        for (int y = 0; y < FIELD_H; y++)
            for (int c = 0; c < CELLS_X; c++) {
                int x = 2 * c + s_par;
                float mx = x + s_hw / 2.0f - fx, my = y + s_hh / 2.0f - fy;
                if (mx * mx + my * my <= r * r && free_at(v, x, y)) { s_dist[y][c] = 0; qx[n] = c; qy[n] = y; n++; }
            }
    for (int h = 0; h < n; h++)
        for (int d = 0; d < 8; d++) {
            int c = qx[h] - STEP_X[d] / 2, y = qy[h] - STEP_Y[d];          // whence a step d arrives here
            if (y < 0 || y >= FIELD_H || c < 0 || c >= CELLS_X || s_dist[y][c] >= 0) continue;
            if (!free_at(v, 2 * c + s_par, y)) continue;
            s_dist[y][c] = (short)(s_dist[qy[h]][qx[h]] + 1);
            qx[n] = c; qy[n] = y; n++;
        }
}

// Where a straight walk in direction d from (x, y) gets, down the map: the
// least distance it passes.
static int run_best(const HuntView *v, int x, int y, int d) {
    int best = dist_at(x, y);
    for (int k = 0; k < 80; k++) {
        x += STEP_X[d]; y += STEP_Y[d];
        int dd = dist_at(x, y);
        if (dd < 0 || !free_at(v, x, y)) break;
        if (dd < best) best = dd;
    }
    return best;
}

static void hunt_drive(MScene *s, const HuntView *v) {
    static const char EXPERT[8] = { ';', 'P', 'O', 'I', 'K', ',', '.', '/' };
    static int wait, frames, way = -1;
    static float gx = -1, gy = -1;
    if (!s->hunt_goal) { frames = 0; gx = gy = -1; return; }
    if (wait > 0) { wait--; return; }
    if (++frames > 60 * 20) { s->hunt_goal = 0; if (v->walking) mach_key_push(S->m, 0x0D); return; }   // give up
    if (s->hunt_goal == 2) {                         // shoot that way
        int want = aim_dir(s->hunt_gx - s->hunter_x, s->hunt_gy - s->hunter_y);
        if (v->walking) { mach_key_push(S->m, 0x0D); wait = 6; return; }
        if (v->turning) return;
        if (v->aim != want) { mach_key_push(S->m, (uint8_t)EXPERT[want]); wait = 6; return; }
        if (!v->shot && v->bullets > 0) mach_key_push(S->m, ' ');
        s->hunt_goal = 0;
        return;
    }
    int x1, y1, x2, y2;
    if (!hunter_box(v, &x1, &y1, &x2, &y2)) return;
    if (s->hunt_gx != gx || s->hunt_gy != gy) {      // a new spot: its map
        gx = s->hunt_gx; gy = s->hunt_gy;
        plan(v, gx * FIELD_W / 384.0f, gy * FIELD_H / (float)MS_ART_H);
        way = -1;
    }
    int here = dist_at(x1, y1);
    if (here == 0 || here < 0) {                     // there (or nowhere to go)
        if (v->walking) mach_key_push(S->m, 0x0D);
        s->hunt_goal = 0;
        return;
    }
    if (v->turning) return;
    // keep on while the line still gains; else the best straight line from here
    int nx = x1 + STEP_X[v->aim], ny = y1 + STEP_Y[v->aim], nd = dist_at(nx, ny);
    if (v->walking && way == v->aim && nd >= 0 && nd < here && free_at(v, nx, ny)) return;
    int best = here, bd = -1;
    for (int d = 0; d < 8; d++) {
        int r = run_best(v, x1, y1, d);
        if (r < best || (r == best && d == v->aim && bd >= 0)) { best = r; bd = d; }
    }
    if (bd < 0) {                                    // no line gains: a step that does (round a corner)
        for (int d = 0; d < 8; d++) {
            int dd = dist_at(x1 + STEP_X[d], y1 + STEP_Y[d]);
            if (dd >= 0 && dd < here && free_at(v, x1 + STEP_X[d], y1 + STEP_Y[d])) { bd = d; break; }
        }
        if (bd < 0) { if (v->walking) mach_key_push(S->m, 0x0D); s->hunt_goal = 0; return; }
    }
    way = bd;
    if (v->aim != bd) { mach_key_push(S->m, (uint8_t)EXPERT[bd]); wait = 6; return; }
    if (!v->walking) { mach_key_push(S->m, 0x0D); wait = 6; }
}

static void hunt_frame(void *ctx) {
    (void)ctx;
    HuntView v;
    hunt_view(&v);
    MScene *s = sc();
    s->nspr = 0;
    int a = 0;
    for (int i = 0; i < v.n; i++) {
        const HuntSprite *p = &v.s[i];
        char id[MS_ID];
        int flip = p->mirrored;
        switch (p->kind) {
        case HV_SCENERY:
            if (p->img < 1 || p->img > 15) continue;
            snprintf(id, sizeof id, "hunt_%s_%d", HUNT_TERRAIN[(p->img - 1) / 3], (p->img - 1) % 3 + 1);
            flip = 0;
            break;
        case HV_DEAD: snprintf(id, sizeof id, "hunt_%s_dead", HUNT_ANIMAL[p->img % 6]); break;
        case HV_ANIMAL:
            if (a < 2) {
                if (p->frame != s_hunt_last[a]) { s_hunt_last[a] = p->frame; s_hunt_step[a] = s_hunt_step[a] % 4 + 1; }
                if (!s_hunt_step[a]) s_hunt_step[a] = 1;
            }
            snprintf(id, sizeof id, "hunt_%s_%d", HUNT_ANIMAL[p->img % 6], a < 2 ? s_hunt_step[a] : 1);
            a++;
            break;
        default: {
            int left;
            const char *aim = hunter_aim(p->img, &left);
            s->hunter_x = rx((p->x1 + p->x2 + 1) / 2.0f);   // the rifle's aim is taken from his middle
            s->hunter_y = ry((p->y1 + p->y2 + 1) / 2.0f);
            snprintf(id, sizeof id, "hunt_hunter_%s_%d", aim, p->frame + 1);
            flip = !p->mirrored ^ left;
            break;
        }
        }
        sprite(id, rx((p->x1 + p->x2 + 1) / 2.0f), ry((float)p->y2 + 1), flip);
        const HuntAnchor *an = hunt_anchor(id);
        MSprite *sp = &s->spr[s->nspr - 1];
        sp->anchor = an != NULL;                    // none: drawn from its corner, and test_harness fails
        sp->bottom = !an;
        sp->ax = an ? an->x : 0; sp->ay = an ? an->y : 0;
        if (p->kind == HV_HUNTER && an) {           // bigger than the original's: kept whole in the field, as his is
            const float W = 48, H = 64;             // the hunter's pictures
            float left = sp->x - (sp->flip ? W - sp->ax : sp->ax), top = sp->y - sp->ay;
            float dx = left < 0 ? -left : left + W > 384 ? 384 - W - left : 0;
            float dy = top < 0 ? -top : top + H > MS_ART_H ? MS_ART_H - H - top : 0;
            sp->x += dx; sp->y += dy; s->hunter_x += dx; s->hunter_y += dy;
        }
    }
    s->dot = v.shot;
    s->dot_x = rx((float)v.sx); s->dot_y = ry((float)v.sy);
    snprintf(s->bar[0], sizeof s->bar[0], "Bullets left: %d", v.bullets);
    if (TOUCH) {                                 // a tap walks there, a double-tap shoots that way
        snprintf(s->bar[2], sizeof s->bar[2], "Tap: walk there   Double-tap: shoot");
        s->hunting = 1;
        hunt_drive(s, &v);
    } else snprintf(s->bar[2], sizeof s->bar[2], "Arrows aim   Space fires   Return walks");
}

// CROSS.LIB, animated as the original's: the wagon held still where the
// original's is in its window while the river goes by under it. The original
// drew the wagon afloat with the near bank at the lower right, swept the
// water in over that bank, then drew the outcome: the far bank coming in at
// the upper left, the water over a ford gone wrong, or the wagon tipped over.
// Here a 384 x 216 window moves up and to the left over the strip
// (scene_river_strip, art.py draw_crossing_strip: its banks run up to the
// right, as the original's) on the original's clock, as much of it bank
// frame by frame as of the original's (cross_way): the near bank gone by
// CROSS_TROUBLE, the far one in at the upper left by CROSS_END. Trouble stops
// it in open water at CROSS_TROUBLE, and what went wrong is shown before the
// message, as the original's water over the wagon is: a ford goes down into
// the river behind its waterline (CROSS_SINKING frames, the rear first), a
// float or a ferry tips over and drifts off with the current settling into
// it, each held CROSS_HOLD frames; the message then comes under it.
#define CROSS_CAM_X 106.0f                 // the window's top left on the strip at the start (art.py
#define CROSS_CAM_Y 212.0f                 // STRIP_*: afloat, the near bank behind), (0, 0) at the end
#define CROSS_X 177.0f                     // the wagon's waterline on the canvas: where the original's
#define CROSS_Y 125.0f                     // wagon is in its window, up and left of the middle
#define CROSS_START 36                     // frames (the original's at 60 a second): the wagon on the bank,
#define CROSS_TROUBLE 104                  // the bank under water (the trouble here),
#define CROSS_END 183                      // the far bank reached,
#define CROSS_SINKING 60                   // what went wrong, happening,
#define CROSS_HOLD 30                      // and held before the message
// The 112 x 56 pictures, pinned by (56, 50): where each meets the water, the
// rows at its left and right edges (the art draws it in perspective: the
// ford's oxen stand in it at row 33, its wheels at 41; art.py waterlines),
// and how far a ford goes down (to the cover's top, rows 7-15, above it).
static const float CROSS_WL[5][2] = { { 0, 0 }, { 41, 47 }, { 33, 41 }, { 38, 45 }, { 38, 50 } };
#define CROSS_TIPPED 4                     // cross_tipped's row in CROSS_WL
#define CROSS_DOWN 22.0f                   // a ford gone wrong: rows it goes down
#define CROSS_SETTLE 14.0f                 // a tipped wagon: rows it settles into the water
#define CROSS_TILT 6.0f                    // the rear under first: rows deeper there, at most
// How far across at frame f, 0 to 1: every 8 frames from CROSS_START (the
// last at CROSS_END), straight between, fitted so as much of the window is
// bank, frame by frame, as of the original's while its water and far bank
// sweep in (within 3% of each half; measured on its recorded frames).
static const float CROSS_WAY[19] = {
    0, .056f, .1f, .149f, .187f, .236f, .292f, .349f, .417f, .49f, .565f, .656f, .716f, .773f, .814f,
    .863f, .901f, .942f, 1 };
static float cross_way(int f) {
    if (f <= CROSS_START) return 0;
    if (f >= CROSS_END) return 1;
    int i = (f - CROSS_START) / 8;
    if (i > 17) i = 17;                    // the last span: 172 to CROSS_END
    float a = CROSS_START + i * 8.0f, b = i == 17 ? (float)CROSS_END : a + 8;
    return CROSS_WAY[i] + (CROSS_WAY[i + 1] - CROSS_WAY[i]) * (f - a) / (b - a);
}
static void cross(int method, int losses) {
    static const char *const M[4] = { "", "cross_ferry", "cross_ford", "cross_float" };
    static const char *const HOW[4] = { "", "Taking the ferry across", "Fording the river", "Floating the wagon across" };
    Mach *m = S->m;
    MScene *s = sc();
    set_bg("scene_river_strip");
    s->box_mid = 1;                                              // the outcome's box on the picture
    int mi = method >= 1 && method <= 3 ? method : 2;
    snprintf(s->bar[0], sizeof s->bar[0], "%s", lm_name(S->g->lm));
    td(S->g, s->bar[1]);
    snprintf(s->bar[2], sizeof s->bar[2], "%s", HOW[mi]);
    sprite(M[mi], CROSS_X, CROSS_Y, mi == 1);                   // the ferry's art heads up-right: mirrored, as the others, to the far bank
    MSprite *w = &s->spr[s->nspr - 1];
    w->anchor = 1; w->ax = 56; w->ay = 50;                       // the 112x56 pictures: the waterline's middle
    const float top = CROSS_Y - 50;                              // the pictures' top row on the canvas
    int ford_bad = losses && method == 2, tip = losses && method != 2;
    int frames = !losses ? CROSS_END : CROSS_TROUBLE + CROSS_SINKING + CROSS_HOLD - 1;
    for (int f = 0; f <= frames; f++) {
        float u = cross_way(losses && f > CROSS_TROUBLE ? CROSS_TROUBLE : f);
        s->bg_x = -(float)(int)(CROSS_CAM_X * (1 - u));
        s->bg_y = -(float)(int)(CROSS_CAM_Y * (1 - u));
        int after = f - CROSS_TROUBLE;                           // frames into the trouble
        if (losses && after >= 0) {
            float t = after < CROSS_SINKING ? (float)after / CROSS_SINKING : 1;
            int pic = tip ? CROSS_TIPPED : mi;
            if (tip && after == 0) snprintf(w->id, MS_ID, "cross_tipped");   // 50050
            const float *wl = CROSS_WL[pic];
            float wl_l = w->flip ? wl[1] : wl[0], wl_r = w->flip ? wl[0] : wl[1];   // mirrored: its edges swap
            float dx = 0, dy = 0, down;
            if (ford_bad) {                                      // 50060: down into the river, faster as it goes
                down = CROSS_DOWN * powf(t, 1.6f);
                wl_r -= CROSS_TILT * sinf(3.14159265f * t);      // the rear under first, level by the end
            } else {                                             // carried off with the current (two across,
                dx = 36 * sqrtf(t); dy = -dx / 2;                // one up), settling into it
                down = CROSS_SETTLE * sqrtf(t);
            }
            w->x = CROSS_X + dx; w->y = CROSS_Y + dy + down;
            w->water = 1;
            w->wl_l = top + dy + wl_l; w->wl_r = top + dy + wl_r;   // the water stays; the picture goes down
            s->ripple_x = w->x; s->ripple_y = top + dy + (wl[0] + wl[1]) / 2; s->ripple_k = 1;
            s->ripple = after * .6f + 1;                         // rings round it
            s->splash = after < 20;                              // and a splash as it goes
            s->bubbles = ford_bad && after > 40 ? (float)(after - 40) : 0;
            s->box_top = (float)(int)(top + dy + (wl[0] > wl[1] ? wl[0] : wl[1]) + 8);   // the message under it
        } else if (!losses && f == frames)
            s->box_top = (float)(int)(top + (CROSS_WL[mi][0] > CROSS_WL[mi][1] ? CROSS_WL[mi][0] : CROSS_WL[mi][1]) + 8);
        mach_advance(m, FRAME_CYCLES);
    }
    s->splash = 0; s->bubbles = 0;
}

// HUNT.LIB:50000-50011: the instructions, a page as the original's, with its
// grid of the expert keys (the eight ways the rifle points).
static int hunt(Req *r) {
    Mach *m = S->m;
    char bg[MS_ID];
    snprintf(bg, sizeof bg, "scene_hunt_zone%d", r->c >= 0 && r->c <= 4 ? r->c : 0);
    page(0);                                     // the original's screen: the keys and their meanings in
    MScene *s = sc();                            // two columns, the expert keys' grid (OREGON.IMA 1 at
    title("Hunting Instructions");               // (30, 78)), the bullets' picture at the lower left (6)
    if (TOUCH)                                   // the same controls, as taps
        body("  Tap            To walk to that spot\n\n\n"
             "  Double-tap     To turn that way and fire\n\n\n"
             "  The hunter walks and turns in the eight\n"
             "  directions of the compass.");
    else {
        body("  Return Key     To start or stop walking\n\n\n\n"
             "  Arrow Keys     To point the rifle\n"
             "                 (novice hunters)\n\n\n"
             "                 To point the rifle\n"
             "                 (expert hunters)\n\n\n\n"
             "  Space Bar      To fire the rifle");
        s->keys_x = 40; s->keys_y = 104;
    }
    snprintf(s->spot, MS_ID, "ui_icon_bullets"); s->spot_x = 16; s->spot_y = 176; s->spot_k = 2;
    wait_continue();
    set_bg(bg);
    int pounds = 0, left = r->a;
    hunt_set_frame_hook(hunt_frame, NULL);
    int rc = hunt_play(m, r->a, r->b & 1, (r->b >> 1) & 1, (r->b >> 2) & 1, 1, r->c, &pounds, &left);
    hunt_set_frame_hook(NULL, NULL);
    sc()->dot = 0; sc()->hint[0] = 0;              // the field stays behind the result
    r->a = pounds; r->b = left;
    return rc;
}

static void hunt_result(int flags, int l, int shot) {
    page(0);
    body("");
    if (flags & HUNT_GOT) body_add("From the animals you shot, you got %d pound%s of meat.  ", shot, shot == 1 ? "" : "s");
    if (flags & HUNT_FULL) body_add("However, your wagon is full.");
    if (flags & HUNT_ROOM) body_add("However, your wagon will only hold another %d pounds of food.", l);
    if (flags & HUNT_CARRY100) body_add("However, you were only able to carry 100 pounds back to the wagon.");
    if (flags & HUNT_NONE) body_add("You were unable to shoot any food.");
    wait_continue();
}

static void win(void) {
    Game *g = S->g;
    const Score *s = &g->score;
    char k[24];
    static const char *const OCC[2][2] = { { "carpenter", "doubled" }, { "farmer", "tripled" } };
    char d[40];
    set_bg("scene_landmark_17");
    title("The Willamette Valley, Oregon");
    body("%s\n\nCongratulations!  You have made it to Oregon!  Let's see how many points you have received.", td(g, d));
    s_slot = MUSIC_ARRIVAL;                                      // WIN:100: MS17.BIN
    wait_tune();
    clear_panel();
    sc()->panel = PANEL_PAGE;
    snprintf(sc()->overlay, MS_ID, "ui_banner_arrival");       // the banner is the title (WIN:606)
    // WIN:610-616: the words by the count, "1 ox" and "2 oxen" (PL$)
    char line[8][48];
    snprintf(line[0], 48, "%ld %s in %s health", s->n[0], s->n[0] > 1 ? "people" : "person", HEALTH[s->band]);
    snprintf(line[1], 48, "1 wagon");
    snprintf(line[2], 48, "%ld ox%s", s->n[2], s->n[2] != 1 ? "en" : "");
    snprintf(line[3], 48, "%ld spare wagon part%s", s->n[3], s->n[3] != 1 ? "s" : "");
    snprintf(line[4], 48, "%ld set%s of clothing", s->n[4], s->n[4] != 1 ? "s" : "");
    snprintf(line[5], 48, "%ld bullet%s", s->n[5], s->n[5] != 1 ? "s" : "");
    snprintf(line[6], 48, "%ld pound%s of food", s->n[6], s->n[6] != 1 ? "s" : "");
    snprintf(line[7], 48, "$%s cash", a2_money((long)floor(s->cash * 100 + .5), k));
    long pts[8] = { s->pts[0], 50, s->pts[2], s->pts[3], s->pts[4], s->pts[5], s->pts[6], s->pts[7] };
    body("");
    for (int i = 0; i < 8; i++) body_add("%-34s %5ld\n", line[i], pts[i]);
    body_add("\n%-34s %5ld", "Total:", s->base);
    wait_continue();
    if (s->occupation > 1) {
        clear_panel();
        body("For going as a %s, your points are %s.\n\nTotal: %ld", OCC[s->occupation - 2][0], OCC[s->occupation - 2][1], s->total);
        wait_continue();
    }
}

// The Top Ten (MENU:2005-2015, WIN:700): a header, then the ten rows; `blank`
// is the row the player's name is about to fill (-1 none).
static void topten_table(const TopTen hi, int blank) {
    body("%-16s %6s  %s\n", "Name", "Points", "Rating");
    for (int l = 0; l < 10; l++) body_add("%-16s %6s  %s\n", l == blank ? "" : hi[l][0], hi[l][1], hi[l][2]);
}

// MENU:2200-2235: how points are earned.
static void points_help(void) {
    static const char *const HEALTH_PTS[4][2] = { { "good", "500" }, { "fair", "400" }, { "poor", "300" }, { "very poor", "200" } };
    static const char *const ITEM_PTS[7][2] = { { "wagon", "50" }, { "ox", "4" }, { "spare wagon part", "2" },
        { "set of clothing", "2" }, { "bullets (each 50)", "1" }, { "food (each 25 pounds)", "1" }, { "cash (each 5 dollars)", "1" } };
    clear_panel(); sc()->panel = PANEL_PAGE;
    title("How points are earned");
    body("Your most important resource is the people you have with you.  You receive points for each member of "
         "your party who arrives safely; you receive more points if they arrive in good health!\n\n%-24s %s\n",
         "Health of party", "Points per person");
    for (int l = 0; l < 4; l++) body_add("  %-22s %6s\n", HEALTH_PTS[l][0], HEALTH_PTS[l][1]);
    wait_continue();
    clear_panel(); sc()->panel = PANEL_PAGE;
    title("How points are earned");
    body("The resources you arrive with will help you get started in the new land.  You receive points for each "
         "item you bring safely to Oregon.\n\n%-24s %s\n", "Resources of party", "Points per item");
    for (int l = 0; l < 7; l++) body_add("  %-22s %6s\n", ITEM_PTS[l][0], ITEM_PTS[l][1]);
    wait_continue();
    clear_panel(); sc()->panel = PANEL_PAGE;
    title("How points are earned");
    body("You receive points for your occupation in the new land.  Because more farmers and carpenters were needed "
         "than bankers, you receive double points upon arriving in Oregon as a carpenter, and triple points for "
         "arriving as a farmer.");
    wait_continue();
}

int modern_io(void *ctx, Game *g, Req *r) {
    MShell *ms = ctx;
    S = ms; ms->g = g;
    MScene *s = sc();
    char buf[128];
    switch (r->kind) {
    case REQ_GOSUB:
        if (r->a == 300) { if (g->ll) travel_view(); else landmark_view(); }
        if (r->a == 950) wait_tune();               // after looking around: the landmark stays up
        return 0;
    case REQ_OCCUPATION: return occupation();
    case REQ_NAMES: names(r->names); return 0;
    case REQ_MONTH: return month();
    case REQ_MATT_INTRO: matt_intro((long)(r->x * 100 + .5)); return 0;
    case REQ_MATT_MENU: return matt_menu(r->a);
    case REQ_MATT_QTY: return matt_qty(r->a, r->b);
    case REQ_MATT_MSG: matt_msg(r->a, r->b); return 0;
    case REQ_MATT_DEPART: matt_depart(); s_slot = 0; return 0;     // BUY SUPPLIES loads MS0.BIN

    case REQ_LOOK_AROUND:
        s_slot = g->lm;                                          // 1000: MS<LM>.BIN
        travel_view();
        clear_panel();
        body("You are now at %s.  Would you like to look around?", LM_NAME[g->lm]);
        return yesno();
    case REQ_ACTION_MENU: return action_menu();
    case REQ_MUST_TRADE:
        page(0);
        body("You must trade for %s%s to be able to continue.", strchr("aeiou", STORE_UNIT[r->a - 2][0]) ? "an " : "a ", STORE_UNIT[r->a - 2]);
        wait_continue();
        return 0;
    case REQ_SUPPLIES: supplies(); return 0;
    case REQ_MAP: map_screen(); return 0;
    case REQ_PACE: return pace();
    case REQ_RATIONS: return rations();
    case REQ_REST_ASK:
        clear_panel();
        body("How many days would you like to rest?");
        return number(0, 9);
    case REQ_REST_DATE: status_fill(); body("%s", td(g, buf)); mach_advance(ms->m, DAY_CYCLES / 2); return 0;
    case REQ_TALK: talk(r->a); return 0;
    case REQ_STORE_MENU: return store_menu();
    case REQ_STORE_QTY:
        page(0);
        pic(STORE_ICON[r->a - 1]);
        body("How many %s%s?", STORE_UNIT[r->a - 1], STORE_PLURAL[r->a - 1]);
        return number(0, r->a == 7 ? 9999 : 999);
    case REQ_STORE_MSG: store_msg(r->a, r->b); return 0;
    case REQ_TRADE_SHOW: return 0;                               // the supplies go with each trade's message
    case REQ_TRADE_NOBODY: page(0); body("No one wants to trade with you today."); trade_have(); wait_continue(); return 0;
    case REQ_TRADE_LACK: page(0); body("You meet another emigrant who wants %s.  You don't have this.", r->text); trade_have(); wait_continue(); return 0;
    case REQ_TRADE_OFFER:
        page(0);
        body("You meet another emigrant who wants %s.  %s will trade you %s.", r->text, r->a ? "She" : "He", r->text2);
        trade_have();
        body_add("\n\nAre you willing to trade?");
        return yesno();
    case REQ_HUNT: return hunt(r);
    case REQ_HUNT_RESULT: hunt_result(r->a, r->b, r->c); return 0;
    case REQ_SPLIT:
        clear_panel();
        body("The trail divides here.  You may:");
        opt("head for %s", LM_NAME[LEG_DEST[LM_LEG[g->lm]]]);
        opt("head for %s", LM_NAME[LEG_DEST[LM_ALT[g->lm]]]);
        opt("see the map");
        return choose();
    case REQ_LEG:
        s_fx = 180 - SCENERY[g->nm - 1][1];                      // 3000
        s_ix = (int)(s_fx - g->d * 2);
        travel_view();
        body("From %s it is %s miles to %s.", LM_NAME[g->lm], a2_str(g->d, buf), LM_NAME[g->nm]);
        wait_tune();                                             // 2200: GOSUB 950
        return 0;
    case REQ_POLL: {
        int k = mach_key_peek(ms->m);
        if (k & 0x80) {
            mach_key_clear(ms->m);
            int ret = (k & 0x7F) == 13 && !s_after_choice;     // not the Return after "1. Continue on trail"
            s_after_choice = 0;
            return ret;
        }
        return 0;
    }
    case REQ_DAY_END:
        if (r->a) {
            travel_view();
            snprintf(s->hint, sizeof s->hint, "%s", PRESS_RETURN); s->tap_key = 0x0D;
            mach_advance(ms->m, DAY_PAUSE);
            int vp = (int)floor((s_fx - g->d * 2 - s_ix) / 4.0 + .5);   // 3246
            for (int l4 = 1; l4 <= vp; l4++) {
                s->wagon_frame = s->wagon_frame % WAGON_FRAMES + 1;
                s_ix += 4;
                travel_view();
                snprintf(s->hint, sizeof s->hint, "%s", PRESS_RETURN); s->tap_key = 0x0D;
                mach_advance(ms->m, WAGON_STEP);
            }
        } else status_fill();
        return 0;
    case REQ_EVENT_SHOW:
        message(r->text, r->lines, r->nlines, r->b);
        if (r->a == EV_TOAST) mach_advance(ms->m, DAY_CYCLES);
        return 0;
    case REQ_EVENT_END: {
        int ret = wait_or_size_up();
        clear_panel();
        return ret;
    }
    case REQ_GRAVE_ASK:
        clear_panel();
        body("You pass a gravesite.  Would you like to look closer?");
        return yesno();
    case REQ_GRAVE_SHOW:                                         // TOMB.LIB:50100: the words on the stone
        set_bg("scene_tombstone");
        snprintf(s->carve, sizeof s->carve, "Here lies\n%s\n\n%s", r->text, r->text2);
        wait_continue();
        return 0;
    case REQ_PART_ASK:                                           // PART.LIB:42000: at (185, 29), in the wagon's place
        clear_panel();
        if (s->travel) wagon_becomes("event_broken_part", WAGON_X + 145 - 150, WAGON_Y + 58 - 66);
        else snprintf(s->overlay, MS_ID, "event_broken_part");
        body("Broken wagon %s.  Would you like to try to repair it?", STORE_UNIT[r->a - 2]);
        return yesno();
    case REQ_PART_MSG:
        clear_panel();
        body("%s", r->text);
        return wait_or_size_up();
    case REQ_NO_OXEN:                                            // 21000: on the trail, whatever was shown before
        travel_view();
        clear_panel();
        body("You are unable to continue your journey.  You have no oxen to pull the wagon.");
        wait_continue();
        return 0;
    case REQ_TOMB_FINAL:
        set_bg("scene_tombstone");
        s_slot = MUSIC_TAPS;                                     // TOMB.LIB:50010: TS.BIN
        snprintf(s->carve, sizeof s->carve, "Here lies\n%s", r->text);
        body("Here lies %s", r->text);
        wait_tune();
        body("Here lies %s\n\nWould you like to write an epitaph?", r->text);
        r->out[0] = 0;
        if (yesno()) {
            for (;;) {
                body("Here lies %s\n\nWhat would you like on the tombstone?", r->text);
                text_input(buf, 29, "-09-AZ-az ,.'-", 0);
                body("Here lies %s\n\n%s\n\nWould you like to make changes?", r->text, buf);
                if (!yesno()) break;
            }
            snprintf(r->out, r->outlen, "%s", buf);
            snprintf(s->carve, sizeof s->carve, "Here lies\n%s\n\n%s", r->text, buf);
        }
        return 0;
    case REQ_ALL_DEAD:
        clear_panel();
        body("All of the people in your party have died.");
        wait_tune();                                             // TOMB.LIB:50040: the dirge again
        return 0;
    case REQ_FLIP: return 0;
    case REQ_SAVE: persist_save(ms->persist, r->a); return 0;
    case REQ_ERROR:
        page(0);
        body("Error %d at line #%d in %s.\n\nPlease report this error to MECC.", r->a, r->b, r->text);
        wait_continue();
        return 0;

    case REQ_RIVER_INTRO:
        set_bg("scene_river_crossing");
        page(1);
        river_title();
        body("You must cross the river in order to continue.  The river at this point is currently %s feet across, and %s feet deep in the middle.",
             a2_str(g->rw, buf), a2_str(g->rd, buf + 64));
        wait_continue();
        return 0;
    case REQ_RIVER_MENU: {
        static const char *const OPT[6] = { "attempt to ford the river", "caulk wagon and float it across",
            "wait to see if conditions improve", "get more information", "hire an Indian to help", "take a ferry across" };
        set_bg("scene_river_crossing");
        page(1);
        river_title();
        body("Weather: %s\nRiver width: %s feet\nRiver depth: %s feet", WEATHER[g->w], a2_str(g->rw, buf), a2_str(g->rd, buf + 64));
        opt("%s", OPT[0]); opt("%s", OPT[1]);
        if (r->a) opt("%s", OPT[r->a + 3]);
        opt("%s", OPT[2]); opt("%s", OPT[3]);
        return choose();
    }
    case REQ_RIVER_INFO:
        page(1); river_title();
        body("To ford a river means to pull your wagon across a shallow part of the river, with the oxen still attached.\n\n"
             "To caulk the wagon means to seal it so that no water can get in.  The wagon can then be floated across like a boat.\n\n"
             "To use a ferry means to put your wagon on top of a flat boat that belongs to someone else.  The owner of the ferry will take your wagon across the river.");
        wait_continue();
        return 0;
    case REQ_RIVER_CAMP: page(1); river_title(); body("You camp near the river for a day."); wait_continue(); return 0;
    case REQ_RIVER_CAMP_DATE: return 0;
    case REQ_RIVER_MSG:
        page(1); river_title();
        body("%s", r->a == RIVER_MSG_FLOAT_SHALLOW ? "The river is too shallow to float across." :
                   r->a == RIVER_MSG_FERRY_SHALLOW ? "The ferry is not operating today because the river is too shallow." :
                   "You do not have enough money to pay for the ferry.");
        wait_continue();
        return 0;
    case REQ_FERRY_OFFER:
        page(1); river_title();
        body("The ferry operator says that he will charge you $5.00 and that you will have to wait %d days.  Are you willing to do this?", r->a);
        return yesno();
    case REQ_GUIDE_OFFER:
        page(1); river_title();
        body("A Shoshoni guide says that he will take your wagon across the river in exchange for %d sets of clothing.", r->a);
        if (!r->b) { body_add("\n\nYou don't have %d sets of clothing.", r->a); wait_continue(); return 0; }
        body_add("\n\nWill you accept this offer?");
        return yesno();
    case REQ_GUIDE_GO:
        page(1); river_title();
        body("The Shoshoni guide will help you %s", r->a == 1 ? "ford the river." : "float your wagon across.");
        wait_continue();
        return 0;
    case REQ_CROSS: cross(r->a, r->b); return 0;

    case REQ_DALLES:
        set_bg("scene_landmark_16");
        page(1);
        body("The trail divides here.  You may:");
        opt("float down the Columbia River"); opt("take the Barlow Toll Road");
        return choose();
    case REQ_BARLOW_ASK:
        page(0);
        body("You must pay $%s to travel the Barlow road.  Are you willing to do this?", a2_money((long)floor(r->x * 100 + .5), buf));
        return yesno();
    case REQ_BARLOW_PAID: return 0;
    case REQ_BARLOW_NO_CASH: page(0); body("You do not have enough cash."); wait_continue(); return 0;
    case REQ_RAFT_INTRO:
        set_bg("scene_columbia");
        body("%s", SAY("Use the arrow keys to guide your raft through the rushing waters of the Columbia River.",
                       "Tap the left or right side of the screen to guide your raft through the rushing waters of the Columbia River."));
        wait_continue();
        body("After passing the third direction sign, land your raft at the trail to the Willamette Valley.");
        wait_continue();
        return 0;
    case REQ_RAFT_FRAME: return raft_frame();
    case REQ_RAFT_HIT:                                           // FLOAT:740: a collision chirps
        if (!strncmp(r->text, "The raft has hit", 16) || !strncmp(r->text, "The raft is destroyed", 21)) music_start(MUSIC_STAB, 0);
        message(r->text, r->lines, r->nlines, 0); wait_continue(); clear_panel(); sc()->music = -1; return 0;
    case REQ_RAFT_LAND: return 0;
    case REQ_ARRIVE: win(); return 0;
    case REQ_TOPTEN_SHOW: {
        TopTen hi;
        topten_read(ms->persist, hi);
        set_bg("scene_title");
        s->panel = PANEL_PAGE;
        snprintf(s->overlay, MS_ID, "ui_banner_topten");
        s->rule = 1;
        if (r->a) return 0;                                      // the name's screen shows the list with the new row
        topten_table(hi, -1);
        body_add("\nYou have accumulated %ld points.  This is not enough to qualify for the Open Top Ten.", g->score.total);
        wait_continue();
        return 0;
    }
    case REQ_TOPTEN_NAME: {                                      // WIN:510-530: typed into the new row
        const char (*hi)[3][OT_NAME_MAX] = r->data;
        for (;;) {
            topten_table(hi, r->a);
            body_add("\nCongratulations!  Type your name as you would like to see it on the Open Top Ten list.");
            text_input(buf, 15, "-AZ-az .'-", 1);
            TopTen shown;
            memcpy(shown, hi, sizeof shown);
            snprintf(shown[r->a][0], OT_NAME_MAX, "%.*s", OT_NAME_MAX - 1, buf);
            topten_table(shown, -1);
            body_add("\nWould you like to make any changes?");
            if (!yesno()) break;
        }
        snprintf(r->out, r->outlen, "%s", buf);
        return 1;
    }
    }
    return 0;
}

// ---------------------------------------------------------------------------
// The modern MENU
// ---------------------------------------------------------------------------
static jmp_buf s_menu;

static void on_escape(Mach *m) {
    (void)m;
    if (S->in_game) game_abandon(S->g);
    longjmp(s_menu, 1);
}

static void learn(void) {
    const char *const pages[] = {             // the last says how to turn the sound off
        "Try taking a journey by covered wagon across 2000 miles of plains, rivers, and mountains.  Try!  On the plains, will you slosh your oxen through mud and water-filled ruts or will you plod through dust six inches deep?",
        "How will you cross the rivers?  If you have money, you might take a ferry (if there is a ferry).  Or, you can ford the river and hope you and your wagon aren't swallowed alive!",
        "What about supplies?  Well, if you're low on food you can hunt.  You might get a buffalo...you might.  And there are bear in the mountains.",
        "At the Dalles, you can try navigating the Columbia River, but if running the rapids with a makeshift raft makes you queasy, better take the Barlow Road.",
        "If for some reason you don't survive -- your wagon burns, or thieves steal your oxen, or you run out of provisions, or you die of cholera -- don't give up!  Try again...and again...until your name is up with the others on The Open Top Ten.",
        SAY("You may turn the sound on or off during the program by pressing Control-S.", "You may turn the sound on or off during the program from the menu button at the top right."),
    };
    for (unsigned i = 0; i < sizeof pages / sizeof *pages; i++) {
        clear_panel();
        sc()->panel = PANEL_PAGE;
        body("%s", pages[i]);
        wait_continue();
    }
}

// The opening, once at start-up: the logo fades in on black, then the wagon
// and the family behind it, then a key (or a click) opens the menu. A key
// during the fades shows the whole picture at once and is not passed on.
#define INTRO_LOGO 60                                // frames: 1 s
#define INTRO_HOLD 20
#define INTRO_BG 60
#define INTRO_VERSION (3 * 60)                  // frames the version stays once the prompt is up

static void intro(void) {
    Mach *m = S->m;
    MScene *s = sc();
    set_bg("scene_title");
    snprintf(s->overlay, MS_ID, "ui_logo");
    s->version = 1;                              // the version at the bottom right (draw.c)
    s->tap_key = ' ';                            // a tap shows it all at once, as a key does
    int skipped = 0;
    for (int f = 0; f <= INTRO_LOGO + INTRO_HOLD + INTRO_BG && !skipped; f++) {
        s->overlay_hide = f < INTRO_LOGO ? 1 - (float)f / INTRO_LOGO : 0;
        int b = f - INTRO_LOGO - INTRO_HOLD;
        s->bg_hide = b <= 0 ? 1 : b < INTRO_BG ? 1 - (float)b / INTRO_BG : 0;
        mach_advance(m, FRAME_CYCLES);
        if (mach_key_peek(m) & 0x80) { mach_key_clear(m); skipped = 1; }
    }
    s->overlay_hide = s->bg_hide = 0;
    s->prompt = MP_CONTINUE;
    snprintf(s->hint, sizeof s->hint, "%s", SAY("Press any key to begin", "Tap to begin"));
    for (int f = 0; !(mach_key_peek(m) & 0x80); f++) {   // the version goes after a few seconds
        if (f == INTRO_VERSION) s->version = 0;
        mach_advance(m, FRAME_CYCLES);
    }
    key();
    s->version = 0;
    s->prompt = MP_NONE; s->hint[0] = 0;
}

// MANAGEMENT:1000-1015 (Ctrl-A at the main menu, or the touch menu's
// "Management options"): the Top Ten lists, and erasing them or the graves.
static void management(void) {
    static const char *const OPT[5] = { "See the current Top Ten list", "See the original Top Ten list",
        "Erase the current Top Ten list", "Erase the tombstone messages", "Return to main menu" };
    for (;;) {
        set_bg("scene_title");
        page(1);
        title("The Oregon Trail\nManagement Options");
        body("You may:");
        for (int i = 0; i < 5; i++) opt("%s", OPT[i]);
        int c = choose();
        if (c == 1 || c == 2) {                                  // 900-915
            TopTen hi;
            if (c == 2) { Persist p; persist_reset_topten(&p); topten_read(&p, hi); }
            else topten_read(S->persist, hi);
            clear_panel();
            sc()->panel = PANEL_PAGE;
            snprintf(sc()->overlay, MS_ID, "ui_banner_topten");
            sc()->rule = 1;
            topten_table(hi, -1);
            wait_continue();
        } else if (c == 3) {
            page(0);
            body("If you erase the current Top Ten list, the names and scores will be replaced by those on the original list.\n\nDo you want to do this?");
            if (yesno()) { persist_reset_topten(S->persist); persist_save(S->persist, 1); }
        } else if (c == 4) {
            page(0);
            body("There may be a tombstone message on each side of the diskette.  If you erase these messages, they will not be replaced until team leaders die along the trail.\n\nDo you want to do this?");
            if (yesno()) { persist_erase_tombs(S->persist); persist_save(S->persist, 0); }
        } else return;
    }
}

void modern_program(MShell *ms) {
    S = ms;
    Mach *m = ms->m;
    m->text.on_escape = on_escape;
    if (m->load) m->load(m->load_ctx, "FONT", m->ram + 0x6BEB, 0x7024 - 0x6BEB);   // the hunt's screen text
    sc()->music = -1;
    intro();
    setjmp(s_menu);
    for (;;) {
        ms->in_game = 0; sc()->in_game = 0;
        set_bg("scene_title");
        snprintf(sc()->overlay, MS_ID, "ui_logo");
        body("You may:");
        opt("Travel the trail"); opt("Learn about the trail"); opt("See the Open Top Ten");
        opt("Turn sound %s", m->sound ? "off" : "on");
        int c = choose();
        switch (c) {
        case -1: management(); break;                            // Ctrl-A (MENU:1000)
        case 1:
            game_init(ms->g, modern_io, ms, NULL, NULL, (unsigned long long)m->cycles * 2654435761u + 1, ms->persist);
            ms->in_game = 1; sc()->in_game = 1;
            game_play(ms->g);
            ms->in_game = 0; sc()->in_game = 0;
            break;
        case 2: learn(); break;
        case 3: {
            TopTen hi;
            topten_read(ms->persist, hi);
            clear_panel();
            sc()->panel = PANEL_PAGE;
            snprintf(sc()->overlay, MS_ID, "ui_banner_topten");
            sc()->rule = 1;
            topten_table(hi, -1);
            body_add("\nWould you like to see how points are earned?");   // MENU:2015
            if (yesno()) points_help();
            break;
        }
        case 4:
            m->sound = !m->sound;
            clear_panel();
            body("The sound is now turned %s.\n\n%s", m->sound ? "on" : "off", SAY("You may turn the sound on or off during the program by pressing Control-S.", "You may turn the sound on or off during the program from the menu button at the top right."));
            wait_continue();
            break;
        }
    }
}
