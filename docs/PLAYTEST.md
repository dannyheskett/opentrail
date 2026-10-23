# Play-testing

`tools/playtest.py` plays whole games through the harness (`build/harness`,
the real game stepped by commands; see BUILD.md) in either mode, along chosen
paths, and keeps a screenshot of every new screen. Screens that animate
without waiting for a key (the river crossing) pass between its stops.

    python3 tools/playtest.py OUT [--mode modern|legacy|both] [--variations V,...] [--jobs J]
    python3 tools/playtest.py OUT --list            # the variations

Each run goes to `OUT/<mode>/<variation>/`: `NNNN_<request>.png` for each new
screen (legacy screens are told apart by their pixels, modern ones by their
scene), samples of travel, the hunt, the crossings and the raft, `log.json`
(each screen's request, game state and, in modern mode, scene) and
`sheet_NN.png` contact sheets (the first screen of each kind). `OUT/summary.md`
says how each run ended; `OUT/coverage.md` lists the requests no run reached.
Runs need a display (the harness's hidden window) and take a few minutes to
half an hour each; `--jobs` runs several at once.

## The player

It decides from the engine's state alone (the harness's `state`: the request
and its fields, the game: date, landmark, whether on the trail or at a fort,
supplies, health, the river's depth, the raft), so the same decisions drive
both modes; only the keys differ (`docs/INPUT.md`: legacy fields end with
Return, and a legacy answer goes by the order of the stops within a request,
since legacy screens carry no scene).

The careful default: a banker leaving in April with 4 yoke, 1200 lb of food,
10 sets of clothes, 30 boxes of bullets, 2 of each spare part; steady pace,
filling rations; rest (3 days, twice at most) when health is poor; hunt
(aiming at the animals from the hunt's field) under 400 lb of food; at a fort,
buy food to 1000 lb, bullets under 300, clothes from August; ford rivers under
2.5 ft, else the ferry or the guide, else float; trade only for what is needed;
talk once at every landmark; look at the supplies once and the map once; the
raft steered by trying every key sequence over the next passes against the
rocks it can see, and making for the landing at the end.

## The variations

| Variation | What changes | The path it covers |
|---|---|---|
| `v01_banker_april_raft` | nothing | the careful default; the raft |
| `v02_carpenter_may_barlow` | carpenter, May, a smaller outfit, the Barlow road | $800; the toll road |
| `v03_farmer_march_raft` | farmer, March, a small outfit | $400; an early, cold start |
| `v04_june_always_ford` | June, ford every river, the Barlow road | fords at any depth (losses, tipped wagons) |
| `v05_always_float` | float every river | caulking at any depth |
| `v06_always_ferry` | the ferry where there is one, the Barlow road | ferry offers and waits |
| `v07_wait_at_rivers` | wait a day at each river first | the camp days, the river's change |
| `v08_other_splits_barlow` | the second way at both splits, the Barlow road | Fort Bridger, the other Blue Mountains leg |
| `v09_grueling_meager` | grueling pace, meager rations | fast and hungry: illness |
| `v10_strenuous_bare_bones` | strenuous pace, bare bones, the Barlow road | starvation at speed |
| `v11_no_hunting` | never hunts; more food | forts only |
| `v12_menu_tour` | first Learn about the trail, the Top Ten, sound off and on | the main menu's other choices |
| `v13_advice_screens` | first the occupations' differences, the month advice, the paces explained, the river information | every "find out" screen |
| `v14_poor_party_starves` | farmer, 150 lb of food, nothing else, no hunting, buying or rest | death: the tombstone, all dead |
| `v15_abandon_esc_esc` | Esc Esc at the third landmark | leaving a game (`GOTO 31000`) |
| `v16_raft_unsteered` | no keys on the raft | hits, losses, a missed landing |
| `v17_barlow_without_cash` | spends all its money at the forts, the Barlow road | "You do not have enough cash", then the raft |
| `v18_july_into_winter` | July, no clothes, no buying | snow, blizzards, cold deaths |
| `v19_trade_and_talk` | accepts every trade, the Barlow road | trading, every talk |
| `v20_quit_dialog` | opens the window's quit dialog at the second landmark and says no | the quit dialog mid-game |

A variation is a few settings over the default (`DEFAULT` and `VARIATIONS`
in the tool): occupation, month, pace, rations, the outfit, the river policy
(`depth`, `ford`, `float`, `ferry`, `wait`), the two splits, The Dalles
(1 the river, 2 the Barlow road), hunting, resting, buying, trading
(`need`, `always`), the raft (`steer`, `none`), a menu tour, the advice
screens, Esc Esc at landmark n, the quit dialog, spending everything. Each
run's seed comes from its variation, so a run repeats.
