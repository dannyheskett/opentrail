#include "a2str.h"
#include "data.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

char *a2_str(double v, char *buf) {
    if (v == 0) { strcpy(buf, "0"); return buf; }
    char tmp[32];
    // 9 significant digits, then rebuild the Applesoft layout from them.
    snprintf(tmp, sizeof tmp, "%.8e", fabs(v));
    char digits[16]; int nd = 0;
    for (const char *s = tmp; *s && *s != 'e'; s++)
        if (*s >= '0' && *s <= '9') digits[nd++] = *s;
    int exp10 = atoi(strchr(tmp, 'e') + 1);
    while (nd > 1 && digits[nd - 1] == '0') nd--;
    digits[nd] = 0;

    char *o = buf;
    if (v < 0) *o++ = '-';
    if (exp10 < -2 || exp10 >= 9) {
        *o++ = digits[0];
        if (nd > 1) { *o++ = '.'; memcpy(o, digits + 1, nd - 1); o += nd - 1; }
        o += sprintf(o, "E%c%02d", exp10 < 0 ? '-' : '+', abs(exp10));
    } else if (exp10 < 0) {
        *o++ = '.';
        for (int i = -1; i > exp10; i--) *o++ = '0';
        memcpy(o, digits, nd); o += nd;
    } else {
        for (int i = 0; i <= exp10; i++) *o++ = i < nd ? digits[i] : '0';
        if (nd > exp10 + 1) {
            *o++ = '.';
            memcpy(o, digits + exp10 + 1, nd - exp10 - 1);
            o += nd - exp10 - 1;
        }
    }
    *o = 0;
    return buf;
}

const char *a2_right(const char *s, size_t n) {
    size_t l = strlen(s);
    return l > n ? s + l - n : s;
}

// V = INT(V*100+.5): K$ = "0" + STR$(V): K$ = STR$(INT(V/100)) + "." + RIGHT$(K$,2)
char *a2_money(long cents, char *buf) {
    char k[24], whole[16];
    a2_str((double)cents, k + 1);
    k[0] = '0';
    a2_str(floor(cents / 100.0), whole);
    sprintf(buf, "%s.%s", whole, a2_right(k, 2));
    return buf;
}

// RIVER.LIB / LF.LIB / FLOAT 50250 and TRADE.LIB 50250.
void a2_item_name(int item, double qty, int trade_rules, char *out, size_t n) {
    const char *name = ITEM[item];
    size_t len = strlen(name);
    int ends_s = len && name[len - 1] == 's';
    int singular = (qty == 1) && (item == 8 || item == 3 || ends_s ||
                                  (trade_rules && item == 2));
    if (!singular) { snprintf(out, n, "%s", name); return; }
    if (item != 8 && item != 3) {
        // LEFT$(Z$, LEN(Z$) - 1 - (L = 2)): "wagon wheels" -> "wagon wheel", "oxen" -> "ox"
        size_t keep = len - 1 - (trade_rules && item == 2);
        snprintf(out, n, "%.*s", (int)keep, name);
        return;
    }
    // X = 6 - 2*(L = 3): Z$ = LEFT$(Z$, X-1) + RIGHT$(Z$, LEN(Z$) - X)
    int x = 6 - 2 * (item == 3);
    snprintf(out, n, "%.*s%s", x - 1, name, name + x);
}

int seq_read(const unsigned char *file, int size, int *pos, char *out, int n) {
    int k = 0, found = 0;
    while (*pos < size) {
        int c = file[(*pos)++] & 0x7F;
        if (c == '\r') { found = 1; break; }
        if (k < n - 1) out[k++] = (char)c;
    }
    out[k] = 0;
    return found;
}

void seq_write(unsigned char *file, int size, int *pos, const char *s) {
    for (; *s && *pos < size; s++) file[(*pos)++] = (unsigned char)*s;
}
