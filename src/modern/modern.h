// Modern mode: the same engine and rules, shown as new-art screens on a
// 252-pixel-high canvas (384 to 480 wide, following the window's shape): the
// art's 216 rows at the top and a band under them for its caption and prompt.
//
// This part is raylib-free. The screens are described as an MScene: a
// background, sprites, an optional status bar, and a dialog panel with a text,
// numbered choices or a text field. src/host/view_modern.c draws the current
// scene every frame. Input arrives as Apple key codes through the machine's
// keyboard queue (the host turns clicks on choices into digit keys), and
// waiting spends emulated time, so the pacing, music and hunt of the machine
// core work unchanged.
#ifndef OT_MODERN_H
#define OT_MODERN_H

#include "game.h"
#include "mach.h"
#include "music.h"

#define MS_ID 48
#define MS_SPRITES 24
#define MS_OPTS 12
#define MS_TEXT 1600
#define MS_LINES 20
#define MS_MARKS 20
#define MS_SCENE_H 128              // the travel scene's rows: a 64-row backdrop at 2x (the original's 111 of 192)
#define MS_ART_H 216                // a picture's rows (the canvas is 252: the caption and prompt under it)

// The panel's kinds. A box: at the bottom, or on the travel scene's ground
// under the wagon (as the original's boxes on the trail). Tall: over the art,
// a portrait at its right (Matt's store), its hint in the band under the
// picture. A page: alone on the screen, as the
// original's text screens (the title, the dividers when rule is set, the
// text, the prompt at the bottom). A caption: a landmark's name and date in a
// band under its picture, nothing over the picture.
enum { PANEL_NONE, PANEL_BOX, PANEL_TALL, PANEL_PAGE, PANEL_CAPTION };

enum { MP_NONE, MP_CONTINUE, MP_CHOOSE, MP_YESNO, MP_TEXT };

typedef struct {
    char id[MS_ID];                 // assets/modern/<id>.png
    float x, y;                     // top-left, canvas pixels
    int flip;                       // mirror horizontally
    int bottom;                     // y is the bottom edge (it stands on the ground)
    int right;                      // x is the right edge
    int scale;                      // drawn at this whole-number scale (0 or 1: as it is)
    float size;                     // or at this fraction (0: as it is): the crossing's distance
    int anchor;                     // (x, y) is where the image's point (ax, ay) lands (mirrored with it)
    float ax, ay;                   // the hunt: each sprite's feet (hunt_anchor.h)
    int above_box;                  // the travel scene: kept above the box (a landmark standing on the ground)
    float sink;                     // rows of the image under the water from its waterline ay (a ford gone wrong)
} MSprite;

typedef struct { float x0, y0, x1, y1; } MLine;    // canvas pixels

// A touch button on the scene: tapped, it types key (an Apple key code); no
// label: an invisible area (the raft's halves).
typedef struct { float x, y, w, h; char label[16]; int key; } MButton;
#define MS_BUTTONS 4

enum { MK_FORT, MK_LANDMARK, MK_START, MK_FINISH };
typedef struct { float x, y; int kind; } MMark;

typedef struct {
    char bg[MS_ID];                 // full-canvas background, "" for none
    char overlay[MS_ID];            // a picture centred in the panel's space (portraits, banners)
    float bg_hide, overlay_hide;    // fading in: 1 unseen .. 0 fully shown (the opening)
    float bg_x, bg_y;               // bg's top left on the canvas, from the 384 columns' (the crossing moves over a big strip)
    int box_mid;                    // the box a band across the picture's middle, not at the bottom (the crossing's outcome)

    // the travel view: bg is the backdrop, repeated across the scene's rows and
    // moved right with the landmark (the wagon walks in place); the landmark
    // icon and the wagon are sprites
    int travel;
    float pan;                      // the backdrop's offset, canvas pixels
    int wagon_frame;                // 1..8

    MSprite spr[MS_SPRITES];
    int nspr;

    // the trail map, drawn over its background with a legend
    int map;
    MLine line[MS_LINES];           // the route travelled
    int nline;
    MMark mark[MS_MARKS];           // the landmarks
    int nmark;

    // the status bar
    int status;
    char st[6][80];                 // date, weather, health, food, next landmark, miles

    // the band under a picture (MS_ART_H..), for the scenes with no caption
    // there (the hunt, the raft, the crossing): its first line's left and
    // right, and a second line centred ("" none)
    char bar[3][80];

    // the dialog panel
    int panel;                      // PANEL_*
    char title[128];
    char body[MS_TEXT];
    char opts[MS_OPTS][80];
    int nopts, opt_base;            // choices are numbered opt_base, opt_base+1, ...
    int opt_key[MS_OPTS];           // a choice's key when not its number (0: its number)
    int prompt;                     // MP_*
    char input[64];                 // text being typed
    char allow[24];                 // the characters it takes (as & INP: "-AZ-az" ranges), for the touch keyboard
    char hint[80];
    int hover;                      // choice under the mouse, -1 none (host)
    char pic[MS_ID];                // a small picture at the panel's top right (a supply icon), "" none
    char opt_pic[MS_OPTS][MS_ID];   // a choice's picture, shown there while the mouse is on it
    int rule;                       // the divider under the title (a page: above and below its text)
    char icon[8][MS_ID];            // a row of pictures under the body (the supplies'), nicon of them
    int nicon;
    char spot[MS_ID];               // a page's picture at (spot_x, spot_y), spot_k times its size ("" none)
    float spot_x, spot_y;
    int spot_k;
    float keys_x, keys_y;           // a page's grid of the hunt's expert keys there (0, 0: none)
    int version;                    // the opening: the version (Mach.version) at the bottom right

    int music;                      // the tune playing (music.h), -1 none
    int veil;                       // over the scene, under the sprites: 0 none, 1 a storm's dark, 2 a blizzard's white
    float ripple_x, ripple_y, ripple; // rings on the water round (x, y), radius ripple (0 none; the crossing)
    float ripple_k;                 // the rings' and the splash's size there (the distance)
    int splash;                     // and a splash there (a ford gone wrong)
    char carve[160];                // words cut in the tombstone's face ("Here lies ..."), "" none
    int dot;                        // the hunt's shot in flight, a dot at (dot_x, dot_y)
    float dot_x, dot_y;

    // touch (set by the host: a phone, or a touch seen on the web): taps on
    // the scene stand for the keys, and the words say "tap"
    int touch;
    int sound;                      // the sound is on (the host's copy, for the touch menu)
    int in_game;                    // a game is on (the touch menu's last item: quit it)
    int tap_key;                    // a tap that meets nothing else types this (0: none)
    MButton btn[MS_BUTTONS];        // buttons on the scene (the hunt's Fire and Walk, the raft's halves)
    int nbtn;
    int hunting;                    // the hunt's field: a tap walks the hunter there, a double-tap shoots that way
    float hunter_x, hunter_y;       // the hunter's middle, canvas pixels (the 384 columns')
    int hunt_goal;                  // 0 none, 1 walk to (hunt_gx, hunt_gy), 2 turn that way and fire
    float hunt_gx, hunt_gy;
} MScene;

typedef struct {
    Mach *m;
    Game *g;
    Persist *persist;
    MScene sc;
    int in_game;
    MusicSynth *music;              // the orchestra (the host mixes it in), NULL for silence
} MShell;

void modern_program(MShell *ms);                    // the modern MENU, never returns
int  modern_io(void *ctx, Game *g, Req *r);         // the engine's io() callback

// The trail map's route and marks for g (REQ_MAP).
void modern_map_scene(MScene *sc, const Game *g);

// Choice keys the host sends for a click on option i (0-based).
int  modern_option_key(const MScene *sc, int i);

#endif
