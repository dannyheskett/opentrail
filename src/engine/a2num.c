// Five-byte floating point, as the original's sums came out. See a2num.h.
//
// A value being worked on is a 40-bit mantissa (the 32 bits a stored number
// keeps, then a rounding byte under them) with its top bit set, an exponent
// biased by 128 (0: the value is zero) and a sign. The operations below are
// plain integer arithmetic on that, with the original's particular results
// kept where they differ from exact rounding:
// - addition with equal exponents adds one to the rounding byte;
// - an operand shifted right for addition keeps only its own 40 bits;
// - in multiplication a zero multiplier byte moves the partial product a whole
//   byte, and one bit further unless the step before it added (see mul());
// - division rounds the divisor to 32 bits first and keeps 34 quotient bits.
#include "a2num.h"
#include <assert.h>
#include <math.h>
#include <stdint.h>

#define M40 0xFFFFFFFFFFull     // 40 bits

typedef struct { int e; uint64_t m; int neg; } Num;    // m: 40 bits

static Num from_fac(A2Fac f) {
    Num n = { f.e, 0, (f.s & 0x80) != 0 };
    for (int i = 0; i < 4; i++) n.m = n.m << 8 | f.m[i];
    n.m = n.m << 8 | f.x;
    return n;
}

static A2Fac to_fac(Num n) {
    A2Fac f = { 0 };
    if (!n.e) return f;
    f.e = (uint8_t)n.e;
    for (int i = 0; i < 4; i++) f.m[i] = (uint8_t)(n.m >> (32 - 8 * i));
    f.x = (uint8_t)n.m;
    f.s = n.neg ? 0xFF : 0;
    return f;
}

static const Num ZERO = { 0, 0, 0 };

// Shift left until the top bit is set. A value whose top 32 bits are clear
// is zero, as is one whose exponent would fall to 0 or below.
static Num normalize(Num n) {
    if (!(n.m >> 8)) return ZERO;
    int s = 0;
    while (!(n.m >> 39 & 1)) { n.m <<= 1; s++; }
    n.m &= M40;
    if (s >= n.e) return ZERO;
    n.e -= s;
    return n;
}

// a + f, a without a rounding byte
static Num add(Num a, Num f) {
    if (!f.e) return a;
    if (!a.e) return f;
    // the operand with the smaller exponent is shifted; with equal ones, a
    Num keep = f, shift = a;
    if (a.e > f.e) { keep = a; shift = f; }
    int d = keep.e - shift.e;
    uint64_t s = d >= 40 ? 0 : shift.m >> d;
    Num r = keep;
    if (keep.neg == shift.neg) {
        uint64_t t = keep.m + s + (d == 0);
        if (t >> 40) {                              // carried out: one place right
            assert(r.e < 255);                      // ?OVERFLOW ERROR
            r.e++;
            t >>= 1;
        }
        r.m = t;
        return r;
    }
    if (keep.m >= s) r.m = keep.m - s;
    else { r.m = s - keep.m; r.neg = !r.neg; }
    return normalize(r);
}

// The result's exponent from the operands' biased ones, or 0 when it falls
// to 0 or below.
static int product_exponent(int ea, int ef) {
    int e = ea + ef - 128;
    assert(e < 256);                                // ?OVERFLOW ERROR
    return e > 0 ? e : 0;
}

// a * f. The multiplier is f's 40 bits, taken a byte at a time from the
// bottom; each set bit adds a's mantissa at the top of a 40-bit partial
// product, which moves right one place per bit. A zero byte (other than the
// top one) moves it a whole byte instead, and then one more place unless the
// step before it was a byte with bits set (for the first byte: unless the
// exponents add to less than 256); that extra place drops a bit out of the
// mantissa without passing it to the rounding byte.
static Num mul(Num a, Num f) {
    if (!f.e) return f;
    if (!a.e) return ZERO;
    int e = product_exponent(a.e, f.e);
    if (!e) return ZERO;
    uint64_t am = a.m >> 8, p = 0;
    int added = a.e + f.e < 256;
    for (int i = 0; i < 5; i++) {
        unsigned b = (unsigned)(f.m >> (8 * i)) & 0xFF;
        if (!b && i < 4) {
            p >>= 8;
            if (!added) p = (p >> 8 >> 1) << 8 | (p & 0xFF);
            added = 0;
            continue;
        }
        for (int k = 0; k < 8; k++) {
            uint64_t hi = (p >> 8) + (b >> k & 1 ? am : 0);   // up to 33 bits
            p = (hi << 8 | (p & 0xFF)) >> 1;
        }
        added = 1;
    }
    Num r = { e, p, a.neg != f.neg };
    return normalize(r);
}

// a / f
static Num divide(Num a, Num f) {
    assert(f.e);                                    // ?DIVISION BY ZERO ERROR
    uint64_t d = f.m >> 8;
    int ef = f.e;
    if (f.m & 0x80) {                               // the divisor rounded
        if (++d >> 32) { d = 0x80000000u; assert(ef < 255); ef++; }
    }
    if (!a.e) return ZERO;
    int sum = a.e + 256 - ef;
    if (sum < 128) return ZERO;
    assert(sum < 384);                              // ?OVERFLOW ERROR
    int e = sum - 128;
    int neg = e ? a.neg != f.neg : 0;               // at the very bottom, positive
    assert(e < 255);                                // ?OVERFLOW ERROR
    e++;
    uint64_t rem = a.m >> 8, q = 0;                 // 34 quotient bits: 1 whole, 33 fractional
    for (int i = 0; i < 34; i++) {
        q <<= 1;
        if (rem >= d) { rem -= d; q |= 1; }
        rem <<= 1;
    }
    Num r = { e, q << 6, neg };
    return normalize(r);
}

// ---------------------------------------------------------------------------
// Doubles in and out
// ---------------------------------------------------------------------------
double a2_num(double v) {
    if (v == 0) return 0;
    int e;
    double f = frexp(fabs(v), &e);
    double m = floor(ldexp(f, 32) + .5);
    double r = ldexp(m, e - 32);
    return v < 0 ? -r : r;
}

static Num from_double(double v) {
    v = a2_num(v);
    if (v == 0) return ZERO;
    int e;
    double f = frexp(fabs(v), &e);
    assert(e + 0x80 > 0 && e + 0x80 < 256);
    Num n = { e + 0x80, (uint64_t)ldexp(f, 32) << 8, v < 0 };
    return n;
}

A2Fac a2_fac(double v) { return to_fac(from_double(v)); }

A2Fac a2_add(double arg, A2Fac fac) { return to_fac(add(from_double(arg), from_fac(fac))); }
A2Fac a2_mul(double arg, A2Fac fac) { return to_fac(mul(from_double(arg), from_fac(fac))); }
A2Fac a2_div(double arg, A2Fac fac) { return to_fac(divide(from_double(arg), from_fac(fac))); }

A2Fac a2_sub(double arg, A2Fac fac) {
    Num f = from_fac(fac);
    f.neg = !f.neg;
    return to_fac(add(from_double(arg), f));
}

// A constant gathers its digits as a whole number (exact: it fits in 32
// bits), then is divided by 10 once per decimal place, the running value
// rounded before each division.
double a2_fin(const char *s) {
    int neg = *s == '-', places = -1;
    if (neg) s++;
    double n = 0;
    for (; *s; s++) {
        if (*s == '.') { places = 0; continue; }
        assert(*s >= '0' && *s <= '9');
        n = n * 10 + (*s - '0');
        if (places >= 0) places++;
    }
    assert(n < 4294967296.0);
    A2Fac f = a2_fac(n);
    for (int i = 0; i < places; i++) f = a2_div(a2_round(f), a2_fac(10));
    double v = a2_round(f);
    return neg ? -v : v;
}

// INT takes the 40-bit value down to a whole number without rounding it
// first: its floor.
double a2_int(A2Fac f) {
    if (!f.e) return 0;
    double m = (double)((uint32_t)f.m[0] << 24 | (uint32_t)f.m[1] << 16 | (uint32_t)f.m[2] << 8 | f.m[3]) * 256 + f.x;
    double v = ldexp(m, f.e - 0x80 - 40);
    return floor(f.s & 0x80 ? -v : v);
}

// To 32 bits: up when the rounding byte's top bit is set.
double a2_round(A2Fac f) {
    if (!f.e) return 0;
    uint64_t m = (uint64_t)((uint32_t)f.m[0] << 24 | (uint32_t)f.m[1] << 16 | (uint32_t)f.m[2] << 8 | f.m[3]);
    int e = f.e;
    if (f.x & 0x80 && ++m >> 32) { m = 0x80000000u; assert(e < 255); e++; }
    double v = ldexp((double)m, e - 0x80 - 32);
    return f.s & 0x80 ? -v : v;
}
