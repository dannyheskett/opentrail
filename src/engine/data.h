// Tables preloaded by VAR.BIN (spec Appendix A), plus the DATA statements the
// engine needs. Values are transcribed from legacy/extracted/sideA/VAR.txt;
// tests/test_engine.c (test_data) checks them against the spec's decoded tables.
#ifndef OT_DATA_H
#define OT_DATA_H

extern const char *const LM_NAME[18];     // LM$(i,0)
extern const int LM_TYPE[18];             // LM$(i,1): 1 store, 2 river, 0 other
extern const int LM_LEG[18];              // LM$(i,2): default leg (99 = end)
extern const int LM_ALT[18];              // LM$(i,3): alternate leg, 0 none
extern const int LEG_MILES[19];           // LM(k,0)
extern const int LEG_MPD[19];             // LM(k,1)
extern const int LEG_DEST[19];            // LM(k,2)
extern const int MAP_XY[18][2];           // MP%(i,0..1)
extern const int SCENERY[17][2];          // L%(k,0..1)
extern const int STATUS_COL[6];           // B(0..5)
extern const char *const WC[6];           // WC$(zone), two characters per month
extern const double RIVER[4][6];          // RC(r,0..5)

extern const char *const STORE_NAME[7];   // S$(i,0)
extern const char *const STORE_BASE[7];   // S$(i,1), as text ("2.00", "0.20")
extern const char *const STORE_UNIT[7];   // S$(i,2)
extern const char *const STORE_PLURAL[7]; // S$(i,3)

extern const char *const MONTH[12];       // M$
extern const char *const ITEM[9];         // I$(0..8), I$(0) unused
extern const char *const PACE[3];         // P$
extern const char *const RATION[3];       // R$
extern const char *const WEATHER[10];     // W$
extern const char *const HEALTH[4];       // H$
extern const char *const ILLNESS[9];      // IL$
extern const char *const ACTION[10];      // AQ$
extern const char *const DEFAULT_NAMES[10];   // MENU:29000
extern const char *const RATING[3];       // WIN:29000

#endif
