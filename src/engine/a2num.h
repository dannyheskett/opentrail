// The BASIC's five-byte floating point, as the original computed it.
//
// The BASIC's running totals (miles, snow, rain) round differently from C
// doubles, and the difference shows: "Miles traveled" is INT(M), and M lands
// on a landmark's whole mileage give or take its rounding. These routines
// add, subtract, multiply and divide with the original's results bit for bit,
// including the rounding byte a result carries into the next operation of
// the same expression.
//
// Values are doubles holding exact 5-byte numbers (32-bit mantissas). Every
// value pushed while evaluating an expression is rounded, so in `A op B` the
// left operand is always a rounded number and the right one may be an
// unrounded result still being worked on (an A2Fac). So `V = BS * (1 - .1 * H0)`
// is
//     a2_round(a2_mul(g->bs, a2_sub(1, a2_mul(A2K(.1), a2_fac(h0)))))
// Constants are read as the original read them, which is not always the
// nearest number: .06 comes out one unit high.
#ifndef OT_A2NUM_H
#define OT_A2NUM_H

#include <stdint.h>

typedef struct { uint8_t e, m[4], s, x; } A2Fac;    // exponent, mantissa, sign (bit 7), rounding byte

A2Fac a2_fac(double v);                             // a variable or whole number, to work on
A2Fac a2_add(double arg, A2Fac fac);                // arg + fac
A2Fac a2_sub(double arg, A2Fac fac);                // arg - fac
A2Fac a2_mul(double arg, A2Fac fac);                // arg * fac
A2Fac a2_div(double arg, A2Fac fac);                // arg / fac; not by zero
double a2_round(A2Fac f);                           // to 32 bits: as stored or pushed
double a2_int(A2Fac f);                             // INT, of the unrounded value
double a2_num(double v);                            // v to the nearest 5-byte number
double a2_fin(const char *s);                       // a decimal constant as the original read it
#define A2K(lit) a2_fin(#lit)                       // A2K(.97): the program's constant .97

#endif
