// Speaker clicks -> PCM. The Apple II speaker is a one-bit output toggled by
// every access to $C030; the machine timestamps each click on its emulated
// clock (mach_click) and this turns them into samples, box-filtered per
// sample and DC-blocked the way the speaker cone relaxes.
#ifndef OT_SPEAKER_H
#define OT_SPEAKER_H

#include "mach.h"
#include <stdint.h>

typedef struct {
    int rate;                       // output sample rate
    unsigned long long at;          // emulated cycle of the next sample's start
    double frac;                    // fractional cycles carried between samples
    int level;                      // +1 / -1
    double hp_in, hp_out;           // DC blocker state
} Speaker;

void speaker_init(Speaker *s, int rate, unsigned long long now);
// Render every whole sample that ends at or before emulated cycle `until`,
// consuming m's clicks; returns the number written (at most max).
int speaker_render(Speaker *s, Mach *m, unsigned long long until, int16_t *out, int max);

#endif
