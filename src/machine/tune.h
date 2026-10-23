// The extension's tune player: & PT and & PN (tune.c).
#ifndef OT_TUNE_H
#define OT_TUNE_H

#include <stdint.h>

typedef struct {
    int tune;           // $8BC9: selected tune number
    uint16_t ptr;       // $8BCA: next note
} TuneState;

#endif
