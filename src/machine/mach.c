// Core of the emulated screen machine: RAM, the keyboard latch, row addressing.
#include "mach.h"
#include <string.h>

void mach_init(Mach *m) {
    memset(m, 0, sizeof *m);
    m->hpag = 0x40;
    m->invflg = 0xFF;
    m->page2 = 1;
    m->sound = 1;
}

// Hi-res rows are interleaved: y = 64a + 8b + c lives at $400c + $80b + $28a.
uint16_t hires_row(int y) {
    return (uint16_t)((y & 7) * 0x400 + ((y >> 3) & 7) * 0x80 + (y >> 6) * 0x28);
}

void mach_key_push(Mach *m, uint8_t key) {
    int next = (m->kq_tail + 1) % KEYQ;
    if (next == m->kq_head) return;                 // full: drop, like a missed keypress
    m->keyq[m->kq_tail] = key | 0x80;
    m->kq_tail = next;
}

// $C000: a queued key becomes the latch once the previous one was cleared.
int mach_key_peek(Mach *m) {
    if (!(m->kbd & 0x80) && m->kq_head != m->kq_tail) {
        m->kbd = m->keyq[m->kq_head];
        m->kq_head = (m->kq_head + 1) % KEYQ;
    }
    return m->kbd;
}

void mach_key_clear(Mach *m) { m->kbd &= 0x7F; }

int mach_key_wait(Mach *m) {
    while (!(mach_key_peek(m) & 0x80)) {
        if (!m->idle) return 0;                     // no host: nothing will ever arrive
        m->waiting = 1;
        mach_advance(m, FRAME_CYCLES);              // waiting spends time too
    }
    m->waiting = 0;
    int k = m->kbd;
    mach_key_clear(m);
    return k;
}

void mach_advance(Mach *m, unsigned long long cycles) {
    m->cycles += cycles;
    while (m->cycles - m->frame_at >= FRAME_CYCLES) {
        m->frame_at += FRAME_CYCLES;
        if (m->idle) {
            // Time spent with a host means a player waiting at a key prompt:
            // the monitor's key wait counts at $4E/$4F (RNDL/RNDH) about once
            // per 15 cycles, and the hunt seeds its generator from it.
            unsigned r = (unsigned)(m->ram[0x4E] | m->ram[0x4F] << 8) + FRAME_CYCLES / 15;
            m->ram[0x4E] = (uint8_t)r; m->ram[0x4F] = (uint8_t)(r >> 8);
            m->idle(m->idle_ctx);
        }
    }
}

void mach_click(Mach *m) {
    m->clicks++;
    int next = (m->spk_tail + 1) % SPKQ;
    if (next == m->spk_head) return;                // host fell behind: drop the click
    m->spk[m->spk_tail] = m->cycles;
    m->spk_tail = next;
}
