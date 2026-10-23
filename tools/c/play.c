// Drive the whole game headlessly: type a key script, then save the screen.
// usage: play "KEYS" OUT.bin [settle_frames] [seed]
//   KEYS: characters as typed; '~' = Return, '^' = Escape, '|' = wait 60 frames.
// Keys go in one at a time once the previous one has been read and a few
// frames have passed. When the script is used up and settle_frames more
// frames have passed, page 2 (or the shown page) is written to OUT.bin;
// with OT_OVERLAY=quit, the page with the quit dialog over it.
#include "screen.h"
#include "overlay.h"
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Mach s_m;
static const char *s_keys, *s_out;
static int s_pos, s_wait, s_settle, s_done_at = -1;
static long s_frame;
static jmp_buf s_end;

void persist_save(const Persist *p, int which) { (void)p; (void)which; }

static int load(void *ctx, const char *name, uint8_t *buf, int max) {
    (void)ctx;
    static const char *const dirs[] = { "assets", "legacy/extracted/sideA", "legacy/extracted/sideB" };
    char path[512];
    for (int i = 0; i < 3; i++) {
        snprintf(path, sizeof path, "%s/%s.bin", dirs[i], name);
        FILE *f = fopen(path, "rb");
        if (!f) continue;
        int n = (int)fread(buf, 1, (size_t)max, f);
        fclose(f);
        return n;
    }
    fprintf(stderr, "play: missing asset %s\n", name);
    return -1;
}

static void idle(void *ctx) {
    (void)ctx;
    s_frame++;
    if (s_frame > 2000000) { fprintf(stderr, "play: frame limit\n"); longjmp(s_end, 1); }
    if (s_wait > 0) { s_wait--; return; }
    if (s_keys[s_pos]) {
        if (s_frame < 300) return;                  // the boot title: nobody is listening yet
        if ((s_m.kbd & 0x80) || s_m.kq_head != s_m.kq_tail) return;   // previous key not read yet
        char c = s_keys[s_pos++];
        if (c == '|') { s_wait = 60; return; }
        mach_key_push(&s_m, (uint8_t)(c == '~' ? 13 : c == '^' ? 27 : c));
        s_wait = 3;
        return;
    }
    if (s_done_at < 0) s_done_at = (int)s_frame;
    if (s_frame - s_done_at >= s_settle) longjmp(s_end, 1);
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: play KEYS OUT.bin [settle] [seed]\n"); return 2; }
    s_keys = argv[1]; s_out = argv[2];
    // "monkey:N:SEED": N random keys a player might press
    static char mk[200000];
    if (!strncmp(s_keys, "monkey:", 7)) {
        int n = atoi(s_keys + 7);
        unsigned seed = (unsigned)atoi(strchr(s_keys + 7, ':') + 1);
        const char *pool = getenv("POOL") ? getenv("POOL") : "123456789~~~~   YYYYNN0A";
        if (n > (int)sizeof mk - 1) n = (int)sizeof mk - 1;
        for (int i = 0; i < n; i++) { seed = seed * 1103515245u + 12345u; mk[i] = pool[(seed >> 16) % strlen(pool)]; }
        mk[n] = 0;
        s_keys = mk;
    }
    s_settle = argc > 3 ? atoi(argv[3]) : 120;
    static Game g; static Persist p; static Shell sh;
    persist_default(&p);
    sh_power_on(&s_m);
    s_m.idle = idle; s_m.load = load;
    if (argc > 4) s_m.cycles = strtoull(argv[4], NULL, 10);
    sh.m = &s_m; sh.g = &g; sh.persist = &p;
    if (!setjmp(s_end)) menu_program(&sh);
    {   // font still intact? (debug aid)
        static uint8_t font[0x439];
        int n = load(NULL, "FONT", font, sizeof font), bad = 0, first = -1;
        for (int i = 0; i < n; i++) if (s_m.ram[0x6BEB + i] != font[i]) { bad++; if (first < 0) first = i; }
        if (bad) fprintf(stderr, "play: %d font bytes changed, first at $%04X\n", bad, 0x6BEB + first);
    }
    FILE *f = fopen(s_out, "wb");
    s_m.idle = NULL;
    const uint8_t *page = getenv("OT_OVERLAY") ? overlay_quit(&s_m) : s_m.ram + (s_m.page2 ? 0x4000 : 0x2000);
    fwrite(page, 1, 8192, f);
    fclose(f);
    fprintf(stderr, "play: frame %ld, cycles %llu, %d keys used, landmark %d, %d/%d/%d, party %d\n", s_frame, s_m.cycles, s_pos, g.lm, g.am, g.ad, g.ay, g.np);
    return 0;
}
