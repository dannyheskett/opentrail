// Modern mode's music (music.h).
//
// The tunes are the extension's format (tune.c): a count of tunes, a table of
// offsets, then (pitch, length) pairs ending at a byte with bit 7 set. Pitch p
// is MIDI note p + 41 (pitch $1C is 429.9 Hz, A4; the original's speaker was
// some 40 cents flat of concert pitch, which an orchestra cannot be); a length
// of d lasts 256 * d passes of 23 cycles (0 means 256), plus the player's
// set-up per note, so the tune keeps the original's tempo to the cycle.
//
// Each tune is a single line. To score it for an orchestra, music_load works
// out its key (the melody's notes against the Krumhansl key profiles), its
// metre (three or four beats, and a pickup, where the long notes fall) and a
// chord per bar (the diatonic triad its notes fit best), then writes the
// parts as timed events: the melody (flute, horn or violins), a string pad
// holding the chord, cellos on the root, a harp's arpeggios and timpani on
// some tunes. The voices are synthesised (band-limited saws, additive horn and
// flute, a plucked string) and pass through a small stereo hall.
#include "music.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PASS 23
#define NOTE_SETUP 258
#define UNIT (256ULL * PASS)                         // cycles per length unit
#define TWO_PI 6.283185307f

enum { P_FLUTE, P_HORN, P_VIOLINS, P_STRINGS, P_CELLO, P_HARP, P_TIMPANI };

static float midi_hz(int n) { return 440.0f * powf(2.0f, (n - 69) / 12.0f); }

// ---------------------------------------------------------------------------
// The tunes
// ---------------------------------------------------------------------------
static int parse(MusicSynth *mu, int t, const uint8_t *b, int n) {
    if (n < 4 || b[0] < 1) return 0;
    int off = b[2] | b[3] << 8;                      // tune 1's offset
    unsigned long long at = 0;
    int k = 0;
    for (int i = off; i + 1 < n && !(b[i] & 0x80) && k < MUSIC_NOTES; i += 2) {
        int d = b[i + 1] ? b[i + 1] : 256;
        unsigned long long len = (unsigned long long)d * UNIT + NOTE_SETUP;
        mu->note[t][k++] = (MusicNote){ b[i], at, len };
        at += len;
    }
    mu->nnote[t] = k;
    mu->length[t] = at;
    return k > 0;
}

static int units(const MusicNote *no) { return (int)((no->len - NOTE_SETUP) / UNIT); }

// ---------------------------------------------------------------------------
// Harmony
// ---------------------------------------------------------------------------
static const float MAJOR[12] = { 6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f, 2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f };
static const float MINOR[12] = { 6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f, 2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f };

static float corr(const float *a, const float *b) {
    float ma = 0, mb = 0;
    for (int i = 0; i < 12; i++) { ma += a[i]; mb += b[i]; }
    ma /= 12; mb /= 12;
    float num = 0, da = 0, db = 0;
    for (int i = 0; i < 12; i++) { num += (a[i] - ma) * (b[i] - mb); da += (a[i] - ma) * (a[i] - ma); db += (b[i] - mb) * (b[i] - mb); }
    return da > 0 && db > 0 ? num / sqrtf(da * db) : 0;
}

static void analyse(MusicSynth *mu, int t) {
    MusicHarmony *h = &mu->harm[t];
    const MusicNote *no = mu->note[t];
    int n = mu->nnote[t];
    int on[MUSIC_NOTES], d[MUSIC_NOTES], total = 0;
    float hist[12] = { 0 };
    for (int i = 0; i < n; i++) {
        on[i] = total; d[i] = units(&no[i]); total += d[i];
        if (no[i].pitch) hist[(no[i].pitch + 41) % 12] += (float)d[i];
    }
    // the key
    float best = -2;
    for (int k = 0; k < 24; k++) {
        float prof[12];
        for (int i = 0; i < 12; i++) prof[i] = (k < 12 ? MAJOR : MINOR)[(i - k % 12 + 12) % 12];
        float c = corr(hist, prof);
        if (c > best) { best = c; h->key = k % 12; h->minor = k >= 12; }
    }
    // the beat: the commonest length, brought to between 0.3 and 0.8 s
    int count[257] = { 0 }, beat = 1, most = 0;
    for (int i = 0; i < n; i++) if (d[i] <= 256 && ++count[d[i]] > most) { most = count[d[i]]; beat = d[i]; }
    while (beat * 5.77 < 300 && beat < 128) beat *= 2;
    while (beat * 5.77 > 800 && beat % 2 == 0) beat /= 2;
    // the metre and the pickup: where the long notes fall
    float score_best = -1;
    h->beats = 4; h->pickup = 0;
    for (int beats = 3; beats <= 4; beats++) {
        int bar = beats * beat;
        for (int i = 0; i < n && on[i] < bar; i++) {
            int off = on[i] % bar;
            float s = 0;
            for (int j = 0; j < n; j++) {
                int rel = on[j] - off;
                if (rel < 0) continue;
                if (rel % bar == 0) s += 2.0f * d[j];
                else if (rel % beat == 0) s += 0.5f * d[j];
            }
            s *= beats == 4 ? 1.05f : 1.0f;          // a small preference for four
            if (s > score_best) { score_best = s; h->beats = beats; h->pickup = off; }
        }
    }
    h->unit = beat;
    int bar = h->beats * beat;
    h->nbar = (h->pickup > 0) + (total - h->pickup + bar - 1) / bar;
    if (h->nbar > MUSIC_BARS) h->nbar = MUSIC_BARS;
    // a chord per bar: the diatonic triads
    static const int MAJ_DEG[5][2] = { { 0, 0 }, { 5, 0 }, { 7, 0 }, { 9, 1 }, { 2, 1 } };   // I IV V vi ii
    static const int MIN_DEG[5][2] = { { 0, 1 }, { 5, 1 }, { 7, 0 }, { 8, 0 }, { 3, 0 } };   // i iv V VI III
    static const float PRIOR[5] = { .35f, .2f, .25f, .05f, .02f };
    const int (*deg)[2] = h->minor ? MIN_DEG : MAJ_DEG;
    int prev = 0;
    for (int b = 0; b < h->nbar; b++) {
        int s0 = b == 0 && h->pickup ? 0 : h->pickup + (b - (h->pickup > 0)) * bar;
        int s1 = b == 0 && h->pickup ? h->pickup : s0 + bar;
        int pick = 0; float top = -1e9f;
        for (int c = 0; c < 5; c++) {
            int root = (h->key + deg[c][0]) % 12, third = (root + (deg[c][1] ? 3 : 4)) % 12, fifth = (root + 7) % 12;
            float s = PRIOR[c];
            for (int i = 0; i < n; i++) {
                if (!no[i].pitch) continue;
                int a = on[i] > s0 ? on[i] : s0, z = on[i] + d[i] < s1 ? on[i] + d[i] : s1;
                if (z <= a) continue;
                int pc = (no[i].pitch + 41) % 12;
                float w = pc == root ? 1.0f : pc == third ? .85f : pc == fifth ? .75f : -.6f;
                if (on[i] == s0) w *= 1.4f;
                s += w * (float)(z - a) / bar;
            }
            if (c == prev) s += .05f;
            if (b == h->nbar - 1 && c == 0) s += .6f;    // end at home
            if (s > top) { top = s; pick = c; }
        }
        prev = pick;
        h->chord[b] = (uint8_t)((h->key + deg[pick][0]) % 12);
        h->chord_minor[b] = (uint8_t)deg[pick][1];
    }
}

// ---------------------------------------------------------------------------
// Orchestration: the events
// ---------------------------------------------------------------------------
typedef struct { int lead, lead2, harp, timp; float pad, bass; } Preset;

static Preset preset(int t) {
    if (t == MUSIC_TAPS) return (Preset){ P_HORN, -1, 0, 0, .18f, 0 };
    if (t == MUSIC_ARRIVAL) return (Preset){ P_VIOLINS, P_HORN, 1, 1, .3f, .55f };
    if (t == 0) return (Preset){ P_HORN, -1, 0, 1, .28f, .5f };
    return (Preset){ t % 2 ? P_FLUTE : P_VIOLINS, -1, t % 3 == 0, t % 4 == 1, .26f, .45f };
}

static void add(MusicSynth *mu, int t, unsigned long long at, unsigned long long len, int part, int midi, float amp) {
    if (mu->nev[t] >= MUSIC_EVENTS || midi < 24 || midi > 108) return;
    mu->ev[t][mu->nev[t]++] = (MusicEvent){ at, len, (uint8_t)part, (uint8_t)midi, amp };
}

static int near(int pc, int lo) { int m = lo + ((pc - lo) % 12 + 12) % 12; return m; }   // pitch class pc at or above lo

static int cmp_ev(const void *a, const void *b) {
    const MusicEvent *x = a, *y = b;
    return x->at < y->at ? -1 : x->at > y->at;
}

static void orchestrate(MusicSynth *mu, int t) {
    const MusicHarmony *h = &mu->harm[t];
    Preset p = preset(t);
    mu->nev[t] = 0;
    for (int i = 0; i < mu->nnote[t]; i++) {                      // the melody
        const MusicNote *no = &mu->note[t][i];
        if (!no->pitch) continue;
        int m = no->pitch + 41 + (p.lead == P_FLUTE ? 12 : 0);
        add(mu, t, no->start, no->len, p.lead, m, .75f);
        if (p.lead2 >= 0) add(mu, t, no->start, no->len, p.lead2, m - 12, .45f);
    }
    unsigned long long beat = (unsigned long long)h->unit * UNIT, bar = beat * h->beats;
    unsigned long long pick = (unsigned long long)h->pickup * UNIT;
    for (int b = 0; b < h->nbar; b++) {
        unsigned long long s0 = b == 0 && h->pickup ? 0 : pick + (b - (h->pickup > 0)) * bar;
        unsigned long long len = b == 0 && h->pickup ? pick : bar;
        if (s0 >= mu->length[t]) break;
        int root = h->chord[b], third = (root + (h->chord_minor[b] ? 3 : 4)) % 12, fifth = (root + 7) % 12;
        if (b == 0 && h->pickup) continue;                           // the pickup: the melody alone
        if (p.pad > 0) {                                             // the strings: a close chord around G3
            add(mu, t, s0, len, P_STRINGS, near(root, 52), p.pad);
            add(mu, t, s0, len, P_STRINGS, near(third, near(root, 52)), p.pad);
            add(mu, t, s0, len, P_STRINGS, near(fifth, near(root, 52)), p.pad);
        }
        if (p.bass > 0) {                                            // cellos and basses: root, and the fifth halfway
            int r = near(root, 36);
            if (h->beats == 4) {
                add(mu, t, s0, beat * 2, P_CELLO, r, p.bass);
                add(mu, t, s0 + beat * 2, beat * 2, P_CELLO, near(fifth, r), p.bass * .8f);
            } else add(mu, t, s0, len, P_CELLO, r, p.bass);
        }
        if (p.harp) {                                                // the harp: the chord rising, two notes a beat
            int tones[3] = { root, third, fifth }, base = near(root, 55);
            for (int k = 0; k < h->beats * 2; k++) {
                int m = near(tones[k % 3], base) + 12 * (k / 3 % 2);
                add(mu, t, s0 + k * beat / 2, beat * 2, P_HARP, m, .3f);
            }
        }
        if (p.timp && (b % 4 == (h->pickup ? 1 : 0) || b == h->nbar - 1))   // timpani on the phrases
            add(mu, t, s0, beat * 3, P_TIMPANI, near(root, 40), .5f);
    }
    qsort(mu->ev[t], (size_t)mu->nev[t], sizeof mu->ev[t][0], cmp_ev);
}

int music_load(MusicSynth *mu, Mach *m) {
    static uint8_t buf[4096];
    int got = 0;
    for (int t = 0; t < MUSIC_STAB; t++) {
        char name[24];
        if (t == MUSIC_TAPS) snprintf(name, sizeof name, "TS.BIN");
        else snprintf(name, sizeof name, "MS%d.BIN", t);
        int n = m->load ? m->load(m->load_ctx, name, buf, sizeof buf) : -1;
        if (n > 0 && parse(mu, t, buf, n)) { analyse(mu, t); orchestrate(mu, t); got++; }
    }
    // the raft's collision (the original chirps four times, FLOAT:740): four
    // stabs of horn and strings on a low D minor chord, a timpani under the first
    int t = MUSIC_STAB;
    unsigned long long step = A2_HZ * 18 / 100;
    mu->nev[t] = 0; mu->nnote[t] = 1;
    for (int k = 0; k < 4; k++) {
        static const int CH[3] = { 50, 53, 57 };
        for (int c = 0; c < 3; c++) {
            add(mu, t, k * step, step / 2, P_HORN, CH[c], .45f);
            add(mu, t, k * step, step / 2, P_STRINGS, CH[c] + 12, .3f);
        }
    }
    add(mu, t, 0, step * 3, P_TIMPANI, 38, .7f);
    qsort(mu->ev[t], (size_t)mu->nev[t], sizeof mu->ev[t][0], cmp_ev);
    mu->length[t] = 4 * step + A2_HZ / 2;
    mu->loaded = got;
    return got;
}

// ---------------------------------------------------------------------------
// The hall: a small Freeverb (four combs and two all-passes a side)
// ---------------------------------------------------------------------------
static const int COMB[4] = { 1116, 1188, 1277, 1356 }, ALLP[2] = { 556, 441 };

static void hall_init(MusicSynth *mu) {
    float k = mu->rate / 44100.0f;
    for (int s = 0; s < 2; s++) {
        for (int i = 0; i < 4; i++) mu->rv_len[s * 6 + i] = (int)((COMB[i] + 23 * s) * k);
        for (int i = 0; i < 2; i++) mu->rv_len[s * 6 + 4 + i] = (int)((ALLP[i] + 23 * s) * k);
    }
    for (int i = 0; i < 12; i++) { if (mu->rv_len[i] > 4096) mu->rv_len[i] = 4096; mu->rv_pos[i] = 0; mu->rv_lp[i] = 0; }
    memset(mu->rv, 0, sizeof mu->rv);
}

static float hall(MusicSynth *mu, int side, float in) {
    float out = 0;
    for (int i = 0; i < 4; i++) {
        int j = side * 6 + i;
        float *b = mu->rv[j]; int p = mu->rv_pos[j];
        float y = b[p];
        mu->rv_lp[j] = y * .75f + mu->rv_lp[j] * .25f;             // damping
        b[p] = in + mu->rv_lp[j] * .83f;
        mu->rv_pos[j] = (p + 1) % mu->rv_len[j];
        out += y;
    }
    for (int i = 0; i < 2; i++) {
        int j = side * 6 + 4 + i;
        float *b = mu->rv[j]; int p = mu->rv_pos[j];
        float y = b[p];
        b[p] = out + y * .5f;
        out = y - out;
        mu->rv_pos[j] = (p + 1) % mu->rv_len[j];
    }
    return out * .25f;
}

void music_init(MusicSynth *mu, int rate, unsigned long long now) {
    mu->rate = rate;
    mu->at = now; mu->frac = 0;
    mu->tune = -1; mu->playing = 0;
    memset(mu->v, 0, sizeof mu->v);
    hall_init(mu);
}

void music_play(MusicSynth *mu, int t, int loop, unsigned long long now) {
    if (t < 0 || t >= MUSIC_TUNES || !mu->nnote[t]) return;
    mu->tune = t; mu->loop = loop; mu->playing = 1;
    mu->t0 = now; mu->next = 0;
    mu->gain = 1; mu->fade = 0;
    if (mu->at < now) mu->at = now;
}

void music_stop(MusicSynth *mu) {
    if (mu->playing && !mu->fade) mu->fade = 1.0f / (mu->rate * 0.5f);
}

int music_playing(const MusicSynth *mu) { return mu->playing ? mu->tune : -1; }

// ---------------------------------------------------------------------------
// The instruments
// ---------------------------------------------------------------------------
static float rnd(uint32_t *s) {
    *s ^= *s << 13; *s ^= *s >> 17; *s ^= *s << 5;
    return (float)(*s & 0xFFFF) / 32768.0f - 1.0f;
}

// A band-limited saw (PolyBLEP).
static float saw(float *ph, float inc) {
    float t = *ph, s = 2 * t - 1;
    if (t < inc) { float x = t / inc; s -= x + x - x * x - 1; }
    else if (t > 1 - inc) { float x = (t - 1) / inc; s -= x * x + x + x + 1; }
    *ph += inc; if (*ph >= 1) *ph -= 1;
    return s;
}

// attack, release (seconds) and the stereo place of each part
static const float ATTACK[7] = { .04f, .08f, .07f, .35f, .12f, .002f, .002f };
static const float RELEASE[7] = { .15f, .22f, .25f, .7f, .3f, 2.5f, 1.4f };
static const float PAN[7] = { .15f, .25f, -.3f, 0, -.2f, .4f, 0 };

static void note_on(MusicSynth *mu, const MusicEvent *e, unsigned long long now) {
    MusicVoice *v = NULL, *quiet = &mu->v[0];
    for (int i = 0; i < MUSIC_VOICES && !v; i++) {
        if (!mu->v[i].on) v = &mu->v[i];
        else if (mu->v[i].env * mu->v[i].amp < quiet->env * quiet->amp) quiet = &mu->v[i];
    }
    if (!v) v = quiet;                                          // steal the quietest
    int idx = (int)(v - mu->v);
    memset(v, 0, sizeof *v);
    v->on = 1; v->part = e->part; v->amp = e->amp; v->pan = PAN[e->part];
    v->freq = midi_hz(e->midi);
    v->end = now + e->len;
    v->noise = 0x9E3779B9u ^ (uint32_t)(e->midi * 2654435761u) ^ (uint32_t)now;
    for (int k = 0; k < 4; k++) v->ph[k] = (float)k * .23f;
    if (e->part == P_HARP) {                                    // the string, plucked: filtered noise
        v->ks = mu->ks_buf[idx];
        v->ks_len = (int)(mu->rate / v->freq);
        if (v->ks_len < 2) v->ks_len = 2;
        if (v->ks_len > 2048) v->ks_len = 2048;
        float lp = 0;
        for (int i = 0; i < v->ks_len; i++) { lp = lp * .5f + rnd(&v->noise) * .5f; v->ks[i] = lp; }
    }
}

static float voice(MusicSynth *mu, MusicVoice *v, unsigned long long now, float dt) {
    (void)mu;
    int p = v->part;
    if (!v->rel && now >= v->end) v->rel = dt;
    float a = v->age < ATTACK[p] ? v->age / ATTACK[p] : 1;
    if (v->rel) {
        float r = 1 - v->rel / RELEASE[p];
        if (r <= 0) { v->on = 0; return 0; }
        a *= r * r;
        v->rel += dt;
    }
    float s = 0, f = v->freq;
    switch (p) {
    case P_FLUTE: {
        float vib = v->age > .2f ? fminf((v->age - .2f) * 2, 1) * .004f * sinf(TWO_PI * 5.3f * v->age) : 0;
        v->ph[0] += f * (1 + vib) * dt; if (v->ph[0] >= 1) v->ph[0] -= 1;
        float ph = TWO_PI * v->ph[0];
        s = sinf(ph) + .22f * sinf(2 * ph) + .06f * sinf(3 * ph) + .05f * rnd(&v->noise) * (v->age < .08f ? 2 : 1);
        s *= .55f;
        break;
    }
    case P_HORN: {
        float vib = v->age > .3f ? .0025f * sinf(TWO_PI * 4.8f * v->age) : 0;
        v->ph[0] += f * (1 + vib) * dt; if (v->ph[0] >= 1) v->ph[0] -= 1;
        float ph = TWO_PI * v->ph[0], bright = .3f + .45f * a, g = 1;
        for (int k = 1; k <= 7 && k * f < 8000; k++) { s += sinf(k * ph) * g / powf((float)k, 1.3f); g *= bright; }
        s *= .5f;
        break;
    }
    case P_VIOLINS: case P_STRINGS: case P_CELLO: {
        static const float DET[3] = { 0, .0042f, -.0047f };
        int nosc = p == P_CELLO ? 2 : 3;
        float vib = p == P_VIOLINS && v->age > .25f ? .005f * sinf(TWO_PI * 5.8f * v->age) : 0;
        for (int k = 0; k < nosc; k++) s += saw(&v->ph[k], f * (1 + DET[k] * (p == P_STRINGS ? 1.6f : 1) + vib) * dt);
        s /= nosc;
        float cut = p == P_CELLO ? 700 : p == P_STRINGS ? 1500 + 900 * a : 3200;   // two one-pole low-passes
        float c = 1 - expf(-TWO_PI * cut * dt);
        v->lp += c * (s - v->lp); v->lp2 += c * (v->lp - v->lp2);
        s = v->lp2 * (p == P_VIOLINS ? .7f : .8f);
        break;
    }
    case P_HARP: {
        int i = v->ks_pos, j = (i + 1) % v->ks_len;
        s = v->ks[i];
        v->ks[i] = (v->ks[i] + v->ks[j]) * .4985f;
        v->ks_pos = j;
        s *= 1.4f;
        if (!v->rel && v->age > 2.5f) v->rel = dt;
        break;
    }
    case P_TIMPANI: {
        float glide = 1 + .04f * expf(-v->age * 18);
        v->ph[0] += f * glide * dt; if (v->ph[0] >= 1) v->ph[0] -= 1;
        float body = sinf(TWO_PI * v->ph[0]) + .35f * sinf(TWO_PI * 1.5f * v->ph[0]);
        float thud = rnd(&v->noise) * expf(-v->age * 40);
        v->lp += .2f * (thud - v->lp);
        s = (body * expf(-v->age * 2.2f) + v->lp * .8f) * .8f;
        break;
    }
    }
    v->env = a;
    v->age += dt;
    return s * a * v->amp;
}

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------
int music_render(MusicSynth *mu, unsigned long long until, float *lr, int max) {
    double per = (double)A2_HZ / mu->rate;           // cycles per sample
    float dt = 1.0f / mu->rate;
    int n = 0;
    while (n < max) {
        if ((double)mu->at + mu->frac + per > (double)until) break;
        unsigned long long now = mu->at;
        if (mu->playing) {
            int t = mu->tune;
            while (mu->next < mu->nev[t] && mu->t0 + mu->ev[t][mu->next].at <= now)
                note_on(mu, &mu->ev[t][mu->next++], now);
            if (mu->next >= mu->nev[t] && now >= mu->t0 + mu->length[t]) {
                if (mu->loop) { mu->t0 += mu->length[t]; mu->next = 0; }
                else mu->playing = 0;
            }
        }
        float l = 0, r = 0;
        for (int i = 0; i < MUSIC_VOICES; i++) {
            MusicVoice *v = &mu->v[i];
            if (!v->on) continue;
            float s = voice(mu, v, now, dt);
            l += s * (1 - v->pan) * .5f; r += s * (1 + v->pan) * .5f;
        }
        if (mu->fade) {
            mu->gain -= mu->fade;
            if (mu->gain <= 0) {
                mu->gain = 0; mu->fade = 0; mu->playing = 0;
                for (int i = 0; i < MUSIC_VOICES; i++) mu->v[i].on = 0;
            }
        }
        l *= mu->gain; r *= mu->gain;
        float wl = hall(mu, 0, (l + r) * .5f), wr = hall(mu, 1, (l + r) * .5f);
        l = l * .85f + wl * .9f; r = r * .85f + wr * .9f;
        lr[2 * n] = tanhf(l * .9f); lr[2 * n + 1] = tanhf(r * .9f);     // a soft limit
        n++;
        mu->frac += per;
        unsigned long long whole = (unsigned long long)mu->frac;
        mu->at += whole; mu->frac -= whole;
    }
    return n;
}
