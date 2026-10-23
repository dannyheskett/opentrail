// The host's raylib-free half. See core.h.
#include "core.h"
#include "embed.h"
#include "speaker.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Assets are the raw DOS file bodies, compiled in (embed.h): the binary needs
// no files beside it.
int host_load(void *ctx, const char *name, uint8_t *buf, int max) {
    (void)ctx;
    for (int i = 0; i < EMBEDDED_COUNT; i++) {
        if (strcmp(EMBEDDED[i].name, name)) continue;
        int n = EMBEDDED[i].size < max ? EMBEDDED[i].size : max;
        memcpy(buf, EMBEDDED[i].data, (size_t)n);
        return n;
    }
    return -1;
}

// ---------------------------------------------------------------------------
// The sound
// ---------------------------------------------------------------------------
#define PCM_RING 16384
static Speaker s_spk;
static MusicSynth *s_music;
static int16_t s_ring[PCM_RING * 2];
static unsigned s_wr, s_rd;                  // atomic: the audio thread reads s_wr, writes s_rd

void core_audio_init(Mach *m, int rate) { speaker_init(&s_spk, rate, m->cycles); }
void core_set_music(MusicSynth *mu) { s_music = mu; }

int core_audio_read(int16_t *out, int frames) {
    unsigned rd = __atomic_load_n(&s_rd, __ATOMIC_RELAXED), wr = __atomic_load_n(&s_wr, __ATOMIC_ACQUIRE);
    for (int i = 0; i < frames; i++) {
        if (rd != wr) { out[2 * i] = s_ring[2 * (rd % PCM_RING)]; out[2 * i + 1] = s_ring[2 * (rd % PCM_RING) + 1]; rd++; }
        else out[2 * i] = out[2 * i + 1] = 0;
    }
    __atomic_store_n(&s_rd, rd, __ATOMIC_RELEASE);
    return frames;
}

void core_pump_audio(Mach *m) {
    static int16_t tmp[4096];
    static float lr[2 * 4096];
    int n = speaker_render(&s_spk, m, m->cycles, tmp, 4096);
    int nm = s_music ? music_render(s_music, m->cycles, lr, 4096) : 0;   // the same clock, so (nearly) the same count
    int frames = n > nm ? n : nm;
    unsigned wr = __atomic_load_n(&s_wr, __ATOMIC_RELAXED), rd = __atomic_load_n(&s_rd, __ATOMIC_ACQUIRE);
    for (int i = 0; i < frames; i++) {
        if (wr - rd >= PCM_RING) break;          // overrun: drop rather than block
        float spk = i < n ? tmp[i] / 32768.0f : 0;
        float l = spk + (i < nm ? lr[2 * i] * .8f : 0), r = spk + (i < nm ? lr[2 * i + 1] * .8f : 0);
        l = l > 1 ? 1 : l < -1 ? -1 : l; r = r > 1 ? 1 : r < -1 ? -1 : r;
        s_ring[2 * (wr % PCM_RING)] = (int16_t)(l * 32767);
        s_ring[2 * (wr % PCM_RING) + 1] = (int16_t)(r * 32767);
        wr++;
    }
    __atomic_store_n(&s_wr, wr, __ATOMIC_RELEASE);
}

