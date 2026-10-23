// OpenTrail: The Oregon Trail (MECC, Apple II, 1985), rebuilt.
// Entry point: the raylib host, the persistent files, the display mode
// (classic, called legacy in the code: the Apple II screen; modern: new art),
// and the MENU program, which owns the loop from here on. On the desktop
// --classic or --modern picks the mode, else the chooser asks (chooser.c); a
// web build is one mode, fixed when it is built (OT_WEB_MODE: 0 classic,
// 1 modern), and keeps its files in the browser (IndexedDB); Android is modern
// mode by touch, its files in the app's own directory.
#include "chooser.h"
#include "core.h"
#include "platform_android.h"
#include "host.h"
#include "platform.h"
#include "recorder.h"
#include "screen.h"
#include "version.h"
#include "view_legacy.h"
#include "view_modern.h"
#include <raylib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

// A new game starts a new recording, as in the family's games ("New game"
// while recording): its first request is the occupation.
static void on_request(void *ctx, const Game *g, const Req *r, int answered, int answer) {
    (void)ctx; (void)g; (void)answer;
    if (!answered && r->kind == REQ_OCCUPATION && recorder_active()) { recorder_stop(); recorder_start(NULL); }
}

static void program(Mach *m, void *arg) {
    Shell *sh = arg;
    sh->m = m;
    menu_program(sh);
}

static void program_modern(Mach *m, void *arg) {
    MShell *ms = arg;
    ms->m = m;
    modern_program(ms);
}

static const char *const USAGE = "usage: opentrail [--classic | --modern] [--touch] [--record [FILE.mp4]] [--version]\n";
static const char VERSION[] = "v1.0." OT_VERSION;     // the family's: v1.0.<release>

int main(int argc, char **argv) {
    static Mach m;
    static Game g;
    static Persist persist;
    static Shell sh;
    static MShell ms;
    int modern = -1, record = 0, touch = 0;          // modern -1: ask
    const char *record_path = NULL;
    for (int i = 1; i < argc; i++)
        if (!strcmp(argv[i], "--modern")) modern = 1;
        else if (!strcmp(argv[i], "--touch")) touch = 1;   // modern mode as on a phone: taps, the touch keyboard
        else if (!strcmp(argv[i], "--classic") || !strcmp(argv[i], "--legacy")) modern = 0;
        else if (!strcmp(argv[i], "--record")) {
            record = 1;
            if (i + 1 < argc && argv[i + 1][0] != '-') record_path = argv[++i];
        } else if (!strcmp(argv[i], "--version")) {
            printf("OpenTrail %s\n", VERSION);
            return 0;
        } else if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
            fputs(USAGE, stdout);
            return 0;
        } else {
            fputs(USAGE, stderr);
            return 2;
        }
#ifdef OT_WEB_MODE
    modern = OT_WEB_MODE;
#endif
#if defined(PLATFORM_ANDROID)
    android_boot();
    core_set_data_dir(android_data_dir());           // the app's own files
    modern = 1;                                      // a phone: modern mode, by touch
    touch = 1;
#endif
    persist_load(&persist);
    sh_power_on(&m);
    m.version = VERSION;
    if (modern < 0) {                                // the chooser, in a window of modern's size
        host_init(&m, "Open Trail", 1152, 756);
        modern = choose_mode();
        if (modern < 0) { host_close(); return 0; }
        if (!modern) {
            SetWindowTitle("The Oregon Trail");
            int mon = GetCurrentMonitor();
            SetWindowSize(1120, 768);
            SetWindowPosition((GetMonitorWidth(mon) - 1120) / 2, (GetMonitorHeight(mon) - 768) / 2);
        }
    } else
        host_init(&m, modern ? "Open Trail" : "The Oregon Trail", modern ? 1152 : 1120, modern ? 756 : 768);   // 3x 384x252; 2x 560x384
    if (modern) { rec_w = 1152; rec_h = 768; }        // the canvas at 3x, its height to a multiple of 16
    ot_set_observer(on_request, NULL);
    if (record && !recorder_start(record_path))
        fprintf(stderr, "opentrail: cannot record to %s\n", record_path ? record_path : "a new file");
    if (modern) {
        static MusicSynth music;                     // the orchestra, rendered on the game's clock
        music_init(&music, HOST_AUDIO_RATE, m.cycles);
        if (music_load(&music, &m) == MUSIC_STAB) { ms.music = &music; host_set_music(&music); }
        ms.m = &m; ms.g = &g; ms.persist = &persist;
        ms.sc.touch = touch;
        host_set_presenter(view_modern(&ms));
        host_run(program_modern, &ms);
    } else {
        host_set_presenter(view_legacy(&m));
        sh.g = &g;
        sh.persist = &persist;
        host_run(program, &sh);
    }
    host_close();
#ifdef __EMSCRIPTEN__
    emscripten_run_script("location.reload()");      // a page cannot close itself: quitting starts again
#endif
    return 0;
}
