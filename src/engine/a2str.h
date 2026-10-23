// Applesoft text conventions the game's messages depend on.
#ifndef OT_A2STR_H
#define OT_A2STR_H

#include <stddef.h>

// STR$(v): Applesoft number formatting. Up to 9 significant digits, no leading
// zero before the point (".5"), no trailing zeros, E notation outside
// .01 <= |v| < 1E9. buf needs 16 bytes.
char *a2_str(double v, char *buf);

// RIGHT$(s, n): the last n characters, or all of a shorter s.
const char *a2_right(const char *s, size_t n);

// OREGON TRAIL:200 money format ("12.05") from whole cents.
char *a2_money(long cents, char *buf);

// Loss-list item names: I$(item) made singular when qty = 1, as each overlay's
// line 50250 does it. The TRADE.LIB version also singularises oxen ("ox").
void a2_item_name(int item, double qty, int trade_rules, char *out, size_t n);

// DOS random-access text file helpers (TREED / TWRITE over a raw file image).
// Fields end with CR; high bits are masked on read.
int  seq_read(const unsigned char *file, int size, int *pos, char *out, int n);
void seq_write(unsigned char *file, int size, int *pos, const char *s);

#endif
