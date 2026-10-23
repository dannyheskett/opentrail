// The persistent files (the graves and the Top Ten), one per disk file, in the
// user's data directory. See core.h. (Not in the harness, which keeps its
// games' files to itself.)
#include "core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
#ifdef _WIN32
#include <direct.h>
#define make_dir(p) _mkdir(p)
#else
#include <sys/stat.h>
#define make_dir(p) mkdir(p, 0755)
#endif

// ---------------------------------------------------------------------------
static char s_dir[512];

// Persistent files live in the user's data directory, one per disk file:
// %APPDATA%\opentrail on Windows, ~/Library/Application Support/opentrail on
// macOS, $XDG_DATA_HOME/opentrail (~/.local/share/opentrail) elsewhere.
void core_set_data_dir(const char *dir) { snprintf(s_dir, sizeof s_dir, "%s", dir); }

static const char *data_dir(void) {
    if (s_dir[0]) return s_dir;                     // named by the platform (Android, iOS)
#if defined(__EMSCRIPTEN__)
    // the browser's IndexedDB, mounted and read in before the game starts
    snprintf(s_dir, sizeof s_dir, "/opentrail");
    emscripten_run_script("FS.mkdir('/opentrail'); FS.mount(IDBFS, {}, '/opentrail'); Module.otSynced = 0;"
                          "FS.syncfs(true, function(err) { Module.otSynced = 1; });");
    while (!emscripten_run_script_int("Module.otSynced")) emscripten_sleep(10);
    return s_dir;
#elif defined(_WIN32)
    const char *app = getenv("APPDATA");
    snprintf(s_dir, sizeof s_dir, "%s/opentrail", app && *app ? app : ".");
#elif defined(__APPLE__)
    const char *home = getenv("HOME");
    snprintf(s_dir, sizeof s_dir, "%s/Library/Application Support/opentrail", home ? home : ".");
#else
    const char *home = getenv("HOME");
    const char *xdg = getenv("XDG_DATA_HOME");
    if (xdg && *xdg) snprintf(s_dir, sizeof s_dir, "%s/opentrail", xdg);
    else snprintf(s_dir, sizeof s_dir, "%s/.local/share/opentrail", home ? home : ".");
#endif
    char parent[512];
    snprintf(parent, sizeof parent, "%s", s_dir);
    char *slash = strrchr(parent, '/');
    if (slash) { *slash = 0; make_dir(parent); }
    make_dir(s_dir);
    return s_dir;
}

static const char *const FILES[3] = { "TOMB1.SEQ", "TOMB2.SEQ", "HISCORE.SEQ" };

static unsigned char *file_buf(Persist *p, int i) {
    return i < 2 ? p->tomb[i] : p->hiscore;
}

void persist_load(Persist *p) {
    persist_default(p);
    for (int i = 0; i < 3; i++) {
        char path[640];
        snprintf(path, sizeof path, "%s/%s", data_dir(), FILES[i]);
        FILE *f = fopen(path, "rb");
        if (!f) continue;
        memset(file_buf(p, i), 0, SEQ_SIZE);
        if (fread(file_buf(p, i), 1, SEQ_SIZE, f)) {}
        fclose(f);
    }
}

// REQ_SAVE: 0 = both tombstone files, 1 = the Top Ten.
void persist_save(const Persist *p, int which) {
    for (int i = which ? 2 : 0; i <= (which ? 2 : 1); i++) {
        char path[640];
        snprintf(path, sizeof path, "%s/%s", data_dir(), FILES[i]);
        FILE *f = fopen(path, "wb");
        if (!f) { fprintf(stderr, "opentrail: cannot write %s\n", path); continue; }
        fwrite(i < 2 ? p->tomb[i] : p->hiscore, 1, SEQ_SIZE, f);
        fclose(f);
    }
#ifdef __EMSCRIPTEN__
    emscripten_run_script("FS.syncfs(false, function(err) {});");   // out to IndexedDB
#endif
}

