// Render a hi-res page (8K, as build/play writes) or a 16K dump of both pages
// through the game's renderer to a 560x384 PNG.
// usage: scr2png IN OUT.png [page 1|2]
#include "ntsc.h"
#include "png.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: scr2png IN OUT.png [1|2]\n"); return 2; }
    static uint8_t buf[16384];
    FILE *f = fopen(argv[1], "rb");
    if (!f) { perror(argv[1]); return 1; }
    size_t n = fread(buf, 1, sizeof buf, f);
    fclose(f);
    int page = argc > 3 ? atoi(argv[3]) : 2;
    const uint8_t *p = n <= 8192 ? buf : buf + (page == 2 ? 8192 : 0);
    static uint32_t px[NTSC_W * NTSC_H], out[NTSC_W * NTSC_H * 2];
    ntsc_render(p, px);
    for (int y = 0; y < NTSC_H; y++) {
        memcpy(out + (2 * y) * NTSC_W, px + y * NTSC_W, NTSC_W * 4);
        memcpy(out + (2 * y + 1) * NTSC_W, px + y * NTSC_W, NTSC_W * 4);
    }
    return write_png(argv[2], out, NTSC_W, NTSC_H * 2);
}
