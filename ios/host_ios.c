// The iOS host: modern mode by touch, on its own thread (ios_main.m starts
// it), owning the loop as on every other platform. Once per 1/60 s of the
// game's time Mach.idle (here) mixes the sound, draws the scene on the
// software canvas (src/modern/draw.c) and hands it to Metal as one texture,
// turns taps into keys, and waits for the display's next frame. In the
// background it waits without drawing, the game's clock held. No raylib.
#include "plat_ios.h"
#include "core.h"
#include "draw.h"
#include "modern.h"
#include "music.h"
#include "screen.h"
#include "version.h"
#include <stdio.h>
#include <string.h>

static Mach s_m;
static Game s_g;
static Persist s_persist;
static MShell s_ms;
static MusicSynth s_music;
static Canvas s_cv;

static const char VERSION[] = "v1.0." OT_VERSION;     // the family's: v1.0.<release>

// Where the canvas sits in the safe rectangle: as large as fits, by fractions.
static void fit(int cw, float *x, float *y, float *w, float *h, float *scale) {
    int dw, dh, sx, sy, sw, sh;
    plat_ios_screen(&dw, &dh, &sx, &sy, &sw, &sh);
    float k = (float)sw / cw < (float)sh / MS_CANVAS_H ? (float)sw / cw : (float)sh / MS_CANVAS_H;
    *scale = k;
    *w = cw * k; *h = MS_CANVAS_H * k;
    *x = sx + (sw - *w) / 2; *y = sy + (sh - *h) / 2;
}

static void frame(void) {
    int dw, dh, sx, sy, sw, sh;
    plat_ios_screen(&dw, &dh, &sx, &sy, &sw, &sh);
    int cw = modern_canvas_w(sw, sh);
    cv_size(&s_cv, cw, MS_CANVAS_H);
    s_ms.sc.touch = 1;
    s_ms.sc.sound = s_m.sound;
    modern_draw(&s_cv, &s_ms.sc, -1, -1, 0, plat_ios_time(), s_m.version);
    float x, y, w, h, k;
    fit(cw, &x, &y, &w, &h, &k);
    ios_present(s_cv.px, cw, MS_CANVAS_H, x, y, w, h);
    float tx, ty;                                   // the taps since, into keys (modern_tap)
    while (plat_ios_pop_tap(&tx, &ty)) {
        int keys[4], n = modern_tap(&s_ms.sc, (tx - x) / k, (ty - y) / k, keys, 4);
        for (int i = 0; i < n; i++) mach_key_push(&s_m, (uint8_t)keys[i]);
    }
}

static void idle(void *ctx) {
    (void)ctx;
    core_pump_audio(&s_m);
    do {                                            // the touch menu holds the game here
        while (!plat_ios_active()) plat_ios_wait_tick();   // in the background: nothing drawn, the clock held
        frame();
        plat_ios_wait_tick();
    } while (modern_paused());
}

void ios_game_main(void) {
    ios_log_stdout();
    core_set_data_dir(ios_data_dir());
    persist_load(&s_persist);
    sh_power_on(&s_m);
    s_m.version = VERSION;
    s_m.idle = idle;
    s_m.idle_ctx = NULL;
    s_m.load = host_load;
    core_audio_init(&s_m, CORE_AUDIO_RATE);
    music_init(&s_music, CORE_AUDIO_RATE, s_m.cycles);
    if (music_load(&s_music, &s_m) == MUSIC_STAB) { s_ms.music = &s_music; core_set_music(&s_music); }
    ios_audio_start();
    s_ms.m = &s_m; s_ms.g = &s_g; s_ms.persist = &s_persist;
    s_ms.sc.touch = 1;
    printf("OpenTrail %s\n", VERSION);
    modern_program(&s_ms);                          // never returns
}
