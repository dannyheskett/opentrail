#include "speaker.h"

#define AMP 9000.0

void speaker_init(Speaker *s, int rate, unsigned long long now) {
    s->rate = rate; s->at = now; s->frac = 0; s->level = -1;
    s->hp_in = s->hp_out = 0;
}

int speaker_render(Speaker *s, Mach *m, unsigned long long until, int16_t *out, int max) {
    const double per = (double)A2_HZ / s->rate;     // cycles per sample (~23.14)
    int n = 0;
    while (n < max) {
        double span = per + s->frac;
        unsigned long long end = s->at + (unsigned long long)span;
        if (end > until) break;
        // average the level over [at, end), toggling at each click inside it
        double sum = 0;
        unsigned long long t = s->at;
        while (m->spk_head != m->spk_tail && m->spk[m->spk_head] < end) {
            unsigned long long c = m->spk[m->spk_head];
            if (c > t) { sum += (double)s->level * (double)(c - t); t = c; }
            s->level = -s->level;
            m->spk_head = (m->spk_head + 1) % SPKQ;
        }
        sum += (double)s->level * (double)(end - t);
        double x = sum / (double)(end - s->at);
        s->frac = span - (double)(end - s->at);
        s->at = end;
        // DC blocker: a held level fades out instead of sitting at full scale
        double y = x - s->hp_in + 0.995 * s->hp_out;
        s->hp_in = x; s->hp_out = y;
        double v = y * AMP;
        out[n++] = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
    }
    return n;
}
