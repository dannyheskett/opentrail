// The tune player: the original's pitch and note length, and every shipped
// tune plays to its end and renders to sound.
#include "../src/machine/mach.c"
#include "../src/machine/tune.c"
#include "../src/machine/speaker.c"
#include "../src/modern/music.c"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TUNE_AT 0x9300

uint16_t gfx_set_addr(const Mach *m, int n) { (void)m; return n == 15 ? TUNE_AT : 0; }

static int load(const char *name, uint8_t *buf) {
    char p[256];
    snprintf(p, sizeof p, "assets/%s.bin", name);
    FILE *f = fopen(p, "rb");
    if (!f) return -1;
    int n = (int)fread(buf, 1, 4096, f);
    fclose(f);
    return n;
}

// REQ-279f: pitch $1C is 429.9 Hz.
static void test_pitch(void) {
    static Mach m;
    mach_init(&m);
    tune_note(&m, 0x1C, 45);
    int n = (m.spk_tail - m.spk_head + SPKQ) % SPKQ;
    double period = (double)(m.spk[m.spk_head + 2] - m.spk[m.spk_head]);
    double hz = A2_HZ / period;
    if (fabs(hz - 429.9) > 0.5) { fprintf(stderr, "FAIL: pitch $1C = %.1f Hz\n", hz); exit(1); }
    double ms = (double)m.cycles / A2_HZ * 1000;
    if (fabs(ms - 45 * 5.772 - 0.2) > 0.5) { fprintf(stderr, "FAIL: 45 units = %.2f ms\n", ms); exit(1); }
    printf("PASS: tone pitch %.1f Hz, %d clicks, %.1f ms (REQ-279f/g)\n", hz, n, ms);
}

// Every shipped tune clicks, ends, lasts a sensible time, and renders to
// samples that are not silent.
static void test_tunes(void) {
    static const char *const names[] = { "MS0.BIN", "MS1.BIN", "MS2.BIN", "MS3.BIN", "MS4.BIN",
        "MS5.BIN", "MS6.BIN", "MS7.BIN", "MS8.BIN", "MS9.BIN", "MS10.BIN", "MS11.BIN", "MS12.BIN",
        "MS13.BIN", "MS14.BIN", "MS15.BIN", "MS16.BIN", "MS17.BIN", "TS.BIN" };
    for (unsigned i = 0; i < sizeof names / sizeof *names; i++) {
        uint8_t buf[4096];
        int n = load(names[i], buf);
        if (n <= 0) { fprintf(stderr, "FAIL: missing %s\n", names[i]); exit(1); }
        static Mach m;
        mach_init(&m);
        memcpy(m.ram + TUNE_AT, buf, (size_t)n);
        static Speaker spk;
        speaker_init(&spk, 44100, m.cycles);
        amp_pt(&m, 1, ARG_NONE);
        double sec = (double)m.cycles / A2_HZ;
        static int16_t pcm[44100 * 60];
        int got = speaker_render(&spk, &m, m.cycles, pcm, (int)(sizeof pcm / sizeof *pcm));
        int loud = 0;
        for (int k = 0; k < got; k++) if (pcm[k] > 1000 || pcm[k] < -1000) { loud = 1; break; }
        if (m.clicks < 100 || sec < 0.5 || sec > 60 || !loud) {
            fprintf(stderr, "FAIL: %s: %lu clicks, %.2f s, %d samples\n", names[i], m.clicks, sec, got);
            exit(1);
        }
    }
    printf("PASS: all 19 tunes play and render\n");
}

static int mload(void *ctx, const char *name, uint8_t *buf, int max) {
    (void)ctx; (void)max;
    return load(name, buf);
}

// Modern mode's music: every tune loads, keeps the original's length (the
// speaker's, above), is heard, never clips or goes wrong, ends when played
// once, loops when asked, fades out on music_stop, and renders the same twice.
static void test_music(void) {
    static Mach m;
    mach_init(&m);
    m.load = mload;
    static MusicSynth mu;
    music_init(&mu, 44100, 0);
    if (music_load(&mu, &m) != MUSIC_STAB) { fprintf(stderr, "FAIL: music: %d tunes\n", mu.loaded); exit(1); }
    static float lr[2 * 44100 * 4];
    for (int t = 0; t <= MUSIC_STAB; t++) {
        // the length against the original's player (tune.c)
        if (t < MUSIC_STAB) {
            static Mach o;
            mach_init(&o);
            for (int k = 0; k < mu.nnote[t]; k++) {
                int d = (int)((mu.note[t][k].len - 258) / (256 * 23));
                tune_note(&o, mu.note[t][k].pitch, d == 256 ? 0 : d);
            }
            double diff = fabs((double)o.cycles - (double)mu.length[t]) / (double)o.cycles;
            if (diff > .01) { fprintf(stderr, "FAIL: music %d: %.3f%% off the original's length\n", t, diff * 100); exit(1); }
        }
        music_init(&mu, 44100, 0);
        music_play(&mu, t, 0, 0);
        unsigned long long at = 0, end = mu.length[t] + 2ULL * A2_HZ;
        double sum = 0; float peak = 0; long frames = 0; int bad = 0;
        while (at < end) {
            at += A2_HZ;                                     // a second at a time
            int n = music_render(&mu, at, lr, 44100 * 4);
            for (int i = 0; i < 2 * n; i++) {
                float x = lr[i];
                if (x != x || x > 1 || x < -1) bad = 1;
                sum += x * x; if (fabsf(x) > peak) peak = fabsf(x);
            }
            frames += n;
        }
        double rms = sqrt(sum / (2.0 * frames));
        if (bad || rms < .01 || peak > .999f || music_playing(&mu) >= 0) {
            fprintf(stderr, "FAIL: music %d: rms %.3f peak %.3f bad %d playing %d\n", t, rms, peak, bad, music_playing(&mu));
            exit(1);
        }
    }
    // looping goes on past the end; music_stop fades it out within a second
    music_init(&mu, 44100, 0);
    music_play(&mu, 11, 1, 0);
    unsigned long long at = mu.length[11] + A2_HZ;
    while (music_render(&mu, at, lr, 44100 * 4) == 44100 * 4) {}
    if (music_playing(&mu) != 11) { fprintf(stderr, "FAIL: music: the loop stopped\n"); exit(1); }
    music_stop(&mu);
    music_render(&mu, at + A2_HZ, lr, 44100 * 4);
    if (music_playing(&mu) >= 0) { fprintf(stderr, "FAIL: music: the fade did not end\n"); exit(1); }
    // the same twice
    static float a[2 * 44100], b[2 * 44100];
    music_init(&mu, 44100, 0); music_play(&mu, 3, 0, 0); music_render(&mu, A2_HZ, a, 44100);
    music_init(&mu, 44100, 0); music_play(&mu, 3, 0, 0); music_render(&mu, A2_HZ, b, 44100);
    if (memcmp(a, b, sizeof a)) { fprintf(stderr, "FAIL: music: not deterministic\n"); exit(1); }
    printf("PASS: modern music: 19 tunes and the raft's stabs at the original's lengths, loop, fade, determinism\n");
}

int main(void) {
    test_pitch();
    test_tunes();
    test_music();
    return 0;
}
