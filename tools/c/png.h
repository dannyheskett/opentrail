// write_png: an RGBA PNG with stored (uncompressed) deflate blocks, so the
// tools need no library.
#ifndef OT_PNG_H
#define OT_PNG_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t crc_tab[256];
static uint32_t crc(uint32_t c, const uint8_t *p, size_t n) {
    if (!crc_tab[1])
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t r = i;
            for (int k = 0; k < 8; k++) r = r & 1 ? 0xEDB88320u ^ r >> 1 : r >> 1;
            crc_tab[i] = r;
        }
    c = ~c;
    while (n--) c = crc_tab[(c ^ *p++) & 0xFF] ^ c >> 8;
    return ~c;
}

static void be32(uint8_t *p, uint32_t v) { p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v; }

static void chunk(FILE *f, const char *type, const uint8_t *data, size_t n) {
    uint8_t h[8];
    be32(h, (uint32_t)n); memcpy(h + 4, type, 4);
    fwrite(h, 1, 8, f); fwrite(data, 1, n, f);
    be32(h, crc(crc(0, (const uint8_t *)type, 4), data, n));
    fwrite(h, 1, 4, f);
}

static int write_png(const char *path, const uint32_t *px, int w, int h) {
    size_t row = (size_t)w * 4 + 1, raw = row * (size_t)h;
    size_t blocks = (raw + 65534) / 65535;
    uint8_t *z = malloc(2 + raw + blocks * 5 + 4), *q = z;
    uint8_t *img = malloc(raw);
    for (int y = 0; y < h; y++) {
        img[y * row] = 0;                                       // filter: none
        memcpy(img + y * row + 1, px + (size_t)y * w, (size_t)w * 4);   // R, G, B, A in memory order
    }
    *q++ = 0x78; *q++ = 0x01;
    uint32_t a = 1, b = 0;
    for (size_t off = 0; off < raw; off += 65535) {
        size_t n = raw - off < 65535 ? raw - off : 65535;
        *q++ = off + n == raw;
        *q++ = n & 0xFF; *q++ = n >> 8; *q++ = ~n & 0xFF; *q++ = (~n >> 8) & 0xFF;
        memcpy(q, img + off, n); q += n;
        for (size_t i = 0; i < n; i++) { a = (a + img[off + i]) % 65521; b = (b + a) % 65521; }
    }
    be32(q, b << 16 | a); q += 4;
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); return 1; }
    uint8_t ihdr[13] = { 0 };
    be32(ihdr, (uint32_t)w); be32(ihdr + 4, (uint32_t)h);
    ihdr[8] = 8; ihdr[9] = 6;                                   // 8-bit RGBA
    fwrite("\x89PNG\r\n\x1a\n", 1, 8, f);
    chunk(f, "IHDR", ihdr, 13);
    chunk(f, "IDAT", z, (size_t)(q - z));
    chunk(f, "IEND", NULL, 0);
    fclose(f);
    free(z); free(img);
    return 0;
}

#endif
