// Colour rendering of a hi-res page at the Apple II's native 560 dots.
#ifndef OT_NTSC_H
#define OT_NTSC_H

#include <stdint.h>

#define NTSC_W 560
#define NTSC_H 192

// page: 8K of hi-res memory ($2000 or $4000 layout). out: NTSC_W x NTSC_H
// pixels, RGBA8 in memory order (raylib PIXELFORMAT_UNCOMPRESSED_R8G8B8A8),
// in solid pixel colours (a clean colour monitor, without composite fringes).
void ntsc_render(const uint8_t *page, uint32_t *out);

#endif
