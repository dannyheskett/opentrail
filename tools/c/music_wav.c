// Previews of modern mode's music: every tune once, as modern mode plays it
// (src/modern/music.c) and as the original's speaker did (tune.c), to WAV.
// usage: music_wav OUTDIR [tune]     (tune: 0-17, 18 = Taps; default all)
#include "mach.h"
#include "speaker.h"
#include "music.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RATE 44100

uint16_t gfx_set_addr(const Mach *m, int n) { (void)m; (void)n; return 0; }   // tune.c's & PT is not used here

static int load(void *ctx, const char *name, uint8_t *buf, int max) {
    (void)ctx;
    char p[256];
    snprintf(p, sizeof p, "assets/%s.bin", name);
    FILE *f = fopen(p, "rb");
    if (!f) return -1;
    int n = (int)fread(buf, 1, (size_t)max, f);
    fclose(f);
    return n;
}

static void put32(FILE *f, unsigned v) { fputc(v & 255, f); fputc(v >> 8 & 255, f); fputc(v >> 16 & 255, f); fputc(v >> 24 & 255, f); }
static void put16(FILE *f, unsigned v) { fputc(v & 255, f); fputc(v >> 8 & 255, f); }

static void wav(const char *path, const int16_t *s, int frames, int ch) {
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); return; }
    unsigned bytes = (unsigned)frames * ch * 2;
    fwrite("RIFF", 1, 4, f); put32(f, 36 + bytes); fwrite("WAVEfmt ", 1, 8, f);
    put32(f, 16); put16(f, 1); put16(f, ch); put32(f, RATE); put32(f, RATE * ch * 2); put16(f, ch * 2); put16(f, 16);
    fwrite("data", 1, 4, f); put32(f, bytes);
    fwrite(s, 2, (size_t)frames * ch, f);
    fclose(f);
}

static int16_t clip(float x) { x *= 32767; return (int16_t)(x > 32767 ? 32767 : x < -32768 ? -32768 : x); }

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: music_wav OUTDIR [tune]\n"); return 2; }
    static Mach m;
    mach_init(&m);
    m.load = load;
    static MusicSynth mu;
    music_init(&mu, RATE, 0);
    if (music_load(&mu, &m) < MUSIC_STAB) fprintf(stderr, "music_wav: only %d tunes\n", mu.loaded);
    int only = argc > 2 ? atoi(argv[2]) : -1;
    static int16_t pcm[RATE * 2 * 60];
    static float lr[RATE * 2 * 60];
    for (int t = 0; t < MUSIC_TUNES; t++) {
        if (only >= 0 && t != only) continue;
        char name[16], path[512];
        if (t == MUSIC_TAPS) snprintf(name, sizeof name, "taps");
        else if (t == MUSIC_STAB) snprintf(name, sizeof name, "raft_stab");
        else snprintf(name, sizeof name, "ms%02d", t);
        // modern: the tune once, then a second of its tail
        music_init(&mu, RATE, 0);
        music_play(&mu, t, 0, 0);
        unsigned long long end = mu.length[t] + A2_HZ;
        int frames = music_render(&mu, end, lr, RATE * 60);
        float peak = 0;
        for (int i = 0; i < 2 * frames; i++) { float a = lr[i] < 0 ? -lr[i] : lr[i]; if (a > peak) peak = a; }
        for (int i = 0; i < 2 * frames; i++) pcm[i] = clip(lr[i]);
        snprintf(path, sizeof path, "%s/%s_modern.wav", argv[1], name);
        wav(path, pcm, frames, 2);
        if (t == MUSIC_STAB) { printf("%s: %.1f s, peak %.2f\n", name, (double)mu.length[t] / A2_HZ, peak); continue; }
        // the original: the same notes through the speaker
        static Mach o;
        mach_init(&o);
        Speaker sp;
        speaker_init(&sp, RATE, 0);
        int got = 0;
        for (int k = 0; k < mu.nnote[t]; k++) {
            const MusicNote *no = &mu.note[t][k];
            int d = (int)((no->len - 258) / (256 * 23));
            tune_note(&o, no->pitch, d == 256 ? 0 : d);
            got += speaker_render(&sp, &o, o.cycles, pcm + got, RATE * 60 - got);
        }
        for (int i = got - 1; i >= 0; i--) pcm[i] = (int16_t)(pcm[i] / 2);     // quieter than full scale
        snprintf(path, sizeof path, "%s/%s_original.wav", argv[1], name);
        wav(path, pcm, got, 1);
        static const char *const PC[12] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
        const MusicHarmony *h = &mu.harm[t];
        printf("%s: %d notes, %.1f s (original %.1f s), peak %.2f; %s %s, %d/4, beat %d, pickup %d, %d bars:", name,
               mu.nnote[t], (double)mu.length[t] / A2_HZ, (double)got / RATE, peak, PC[h->key], h->minor ? "minor" : "major",
               h->beats, h->unit, h->pickup, h->nbar);
        for (int b = 0; b < h->nbar && b < 16; b++) printf(" %s%s", PC[h->chord[b]], h->chord_minor[b] ? "m" : "");
        printf("\n");
    }
    return 0;
}
