// VAR.BIN tables (legacy/extracted/sideA/VAR.txt) and DATA statements.
#include "data.h"

const char *const LM_NAME[18] = {
    "Independence", "the Kansas River crossing", "the Big Blue River crossing",
    "Fort Kearney", "Chimney Rock", "Fort Laramie", "Independence Rock",
    "South Pass", "Fort Bridger", "Green River crossing", "Soda Springs",
    "Fort Hall", "the Snake River crossing", "Fort Boise", "the Blue Mountains",
    "Fort Walla Walla", "The Dalles", "the Willamette Valley",
};
const int LM_TYPE[18] = { 1, 2, 2, 1, 0, 1, 0, 0, 1, 2, 0, 1, 2, 1, 0, 1, 0, 0 };
const int LM_LEG[18]  = { 0, 1, 2, 3, 4, 5, 6, 7, 9, 10, 11, 12, 13, 14, 15, 17, 18, 99 };
const int LM_ALT[18]  = { 0, 0, 0, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 16, 0, 0, 0 };

const int LEG_MILES[19] = { 102, 83, 119, 250, 86, 190, 102, 57, 125, 162, 144, 57,
                            182, 114, 160, 55, 125, 120, 100 };
const int LEG_MPD[19]   = { 20, 20, 20, 20, 20, 12, 12, 12, 12, 12, 12, 12, 12, 12,
                            12, 12, 12, 12, 12 };
const int LEG_DEST[19]  = { 1, 2, 3, 4, 5, 6, 7, 9, 8, 10, 10, 11, 12, 13, 14, 15,
                            16, 16, 17 };

const int MAP_XY[18][2] = {
    {245,139}, {235,136}, {226,126}, {207,122}, {185,120}, {170,114},
    {155,100}, {143,108}, {132,117}, {134,108}, {124,104}, {110,101},
    { 93,102}, { 80, 87}, { 70, 76}, { 69, 66}, { 55, 71}, { 45, 70},
};

const int SCENERY[17][2] = {
    {8,78}, {8,78}, {5,50}, {6,36}, {7,50}, {6,36}, {7,56}, {8,56}, {14,78},
    {9,64}, {10,70}, {14,78}, {11,56}, {15,56}, {12,56}, {13,112}, {16,50},
};

const int STATUS_COL[6] = { 95, 70, 81, 95, 23, 20 };

const char *const WC[6] = {
    ";+?,I8V?_NiNnElFdHY<K1@-",
    "5#:#B(O3Y<c?i9g4].Q(C#9#",
    "5#9#>'H.R3\\+e(c$X'M'?%8#",
    "1#6%>*I5R7[+c&a#W)L,=&3$",
    "<-B+H+P*X*_'h!f!]$R(F+>,",
    "DWIGLBQ5W3[.`#`(]/T?LTG^",
};

// depth, width, swiftness, bottom, unused, extra option
const double RIVER[4][6] = {
    {  1,  600, 3, 0,  3, 2 },     // Kansas
    {  1,  220, 2, 1,  2, 0 },     // Big Blue
    { 20,  400, 5, 2, 12, 2 },     // Green
    {  6, 1000, 7, 2,  9, 1 },     // Snake
};

const char *const STORE_NAME[7] = { "Oxen", "Clothing", "Ammunition", "Wagon wheels",
                                    "Wagon axles", "Wagon tongues", "Food" };
const char *const STORE_BASE[7] = { "20", "10", "2.00", "10", "10", "10", "0.20" };
const char *const STORE_UNIT[7] = { "ox", "set", "box", "wheel", "axle", "tongue", "pound" };
const char *const STORE_PLURAL[7] = { "en", "s", "es", "s", "s", "s", "s" };

const char *const MONTH[12] = { "January", "February", "March", "April", "May", "June",
                                "July", "August", "September", "October", "November",
                                "December" };
const char *const ITEM[9] = { "", "Wagon", "oxen", "sets of clothing", "bullets",
                              "wagon wheels", "wagon axles", "wagon tongues",
                              "pounds of food" };
const char *const PACE[3] = { "steady", "strenuous", "grueling" };
const char *const RATION[3] = { "filling", "meager", "bare bones" };
const char *const WEATHER[10] = { "very cold", "cold", "cool", "warm", "hot", "very hot",
                                  "rainy", "snowy", "very rainy", "very snowy" };
const char *const HEALTH[4] = { "good", "fair", "poor", "very poor" };
const char *const ILLNESS[9] = { "a broken arm", "a broken leg", "a snakebite",
                                 "exhaustion", "typhoid", "cholera", "measles",
                                 "dysentery", "a fever" };
const char *const ACTION[10] = { "Continue on trail", "Check supplies", "Look at map",
                                 "Change pace", "Change food rations", "Stop to rest",
                                 "Attempt to trade", "Talk to people", "Buy supplies",
                                 "Hunt for food" };
const char *const DEFAULT_NAMES[10] = { "Zeke", "Jed", "Anna", "Mary", "Joey", "Beth",
                                        "John", "Sara", "Henry", "Emily" };
const char *const RATING[3] = { "Trail guide", "Adventurer", "Greenhorn" };
