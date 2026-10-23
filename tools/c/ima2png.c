// Cut every image of an original image set (.IMA) out to PNG, drawn by the
// game's own & IMAGE and rendered in clean colour, one Apple pixel per pixel.
// usage: ima2png SET OUTPREFIX [raw]
//   SET: the asset name ("EVENTS.IMA"), read from assets/SET.bin.
//   raw: the hunt's sets, whose images are plain rows of bytes ($F1FC).
// Writes OUTPREFIX_NN.png for each image NN of the index.
#include "screen.h"
#include "ntsc.h"
#include "png.h"

#define SET 0x9000

void persist_save(const Persist *p, int which) { (void)p; (void)which; }

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: ima2png SET OUTPREFIX [raw]\n"); return 2; }
    int raw = argc > 3 && !strcmp(argv[3], "raw");
    static Mach m;
    sh_power_on(&m);
    char path[512];
    snprintf(path, sizeof path, "assets/%s.bin", argv[1]);
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); return 1; }
    int len = (int)fread(m.ram + SET, 1, 0xB000 - SET, f);
    fclose(f);
    m.hpag = 0x20;
    amp_cst(&m, 14848);
    amp_dft(&m, 7, SET);

    // byte 0: the number of images; entry n (1..count) is 2 bytes at SET + byte 1 + 2n
    int ix = m.ram[SET + 1], total = m.ram[SET];
    static uint32_t px[NTSC_W * NTSC_H], out[280 * 192];
    int count = 0;
    for (int n = 1; n <= total; n++) {
        int lo = m.ram[SET + ix + 2 * n], hi = m.ram[SET + ix + 2 * n + 1];
        int off = lo | (hi & 0x3F) << 8;
        if (off + 2 > len) continue;
        int w = m.ram[SET + off], h = m.ram[SET + off + 1];
        if (w < 1 || w > 40 || h < 1 || h > 192) continue;
        memset(m.ram + 0x2000, 0, 0x2000);
        if (raw) {
            const uint8_t *d = m.ram + SET + off + 2;
            for (int y = 0; y < h; y++)
                for (int x = 0; x < w; x++)
                    m.ram[0x2000 + hires_row(y) + x] = d[y * w + x];
        } else {
            amp_image(&m, n, 0, 0);
        }
        ntsc_render(m.ram + 0x2000, px);
        int pw = w * 7;
        for (int y = 0; y < h; y++)
            for (int x = 0; x < pw; x++) out[y * pw + x] = px[y * NTSC_W + 2 * x + 1];
        snprintf(path, sizeof path, "%s_%02d.png", argv[2], n);
        if (write_png(path, out, pw, h)) return 1;
        count++;
    }
    printf("%s: %d images\n", argv[1], count);
    return 0;
}
