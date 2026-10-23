// Hi-res page -> colour pixels, as a clean colour monitor showed them.
//
// The Apple II draws each hi-res pixel as two dots of the 14.318 MHz clock, so a
// line is 560 dots. A byte with bit 7 set is delayed by one dot (half a pixel),
// and the last dot of the byte before it is held through the gap.
#include "ntsc.h"

static int row_offset(int y) { return (y & 7) * 0x400 + ((y >> 3) & 7) * 0x80 + (y >> 6) * 0x28; }

#define RGB(r, g, b) (0xFF000000u | (uint32_t)(b) << 16 | (uint32_t)(g) << 8 | (uint32_t)(r))

// The clean colour monitor ("RGB" in most emulators): each of the 280 pixels is
// one solid colour. Two lit pixels side by side are white, a lone lit pixel
// takes its column's colour (violet/green, or blue/orange with bit 7 set), and
// a dark pixel between two lit ones fills with their colour. Pixels are two
// dots wide and a bit-7 byte starts one dot later, the last dot of the byte
// before it held through the gap.
static const uint32_t CLEAN[6] = {
    RGB(0x00, 0x00, 0x00), RGB(0xFF, 0xFF, 0xFF), RGB(0xFF, 0x44, 0xFD),
    RGB(0x14, 0xF5, 0x3C), RGB(0x14, 0xCF, 0xFD), RGB(0xFF, 0x6A, 0x3C),
};

void ntsc_render(const uint8_t *page, uint32_t *out) {
    uint8_t bit[282], pal[282];
    for (int y = 0; y < 192; y++) {
        const uint8_t *row = page + row_offset(y);
        bit[0] = bit[281] = 0; pal[0] = pal[281] = 0;
        for (int c = 0; c < 40; c++)
            for (int p = 0; p < 7; p++) {
                bit[1 + c * 7 + p] = (uint8_t)((row[c] >> p) & 1);
                pal[1 + c * 7 + p] = (uint8_t)(row[c] >> 7);
            }
        uint32_t *o = out + y * NTSC_W;
        uint32_t held = CLEAN[0];
        for (int x = 0; x < 280; x++) {
            int on = bit[x + 1], l = bit[x], r = bit[x + 2], h = pal[x + 1];
            int k;
            if (on && (l || r)) k = 1;
            else if (on) k = h ? (x % 2 ? 5 : 4) : (x % 2 ? 3 : 2);
            else if (l && r) k = h ? (x % 2 ? 4 : 5) : (x % 2 ? 2 : 3);
            else k = 0;
            uint32_t c = CLEAN[k];
            int d = x * 2 + h;
            if (h && x % 7 == 0) o[x * 2] = held;           // the held dot
            o[d] = c;
            if (d + 1 < NTSC_W) o[d + 1] = c;
            held = c;
        }
    }
}
