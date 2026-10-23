# Modern-mode art plan

Made by `tools/art.py plan`: one row per job in art/jobs/, the images it makes, its one run (art/runs/<job>/runNN/, the approved one) and the price of running it again (Retro Diffusion's free check). The recipe is in docs/ARCHITECTURE.md; each image's source file is in art/manifest.json.

| job | style | size | images | run | cost |
|---|---|---|---|---|---|
| cross_ferry | rd_pro__default | 112x56 | cross_ferry | run01 | $0.18 |
| cross_float | rd_pro__default | 112x56 | cross_float | run02 | $0.18 |
| cross_ford | rd_pro__default | 112x56 | cross_ford | run04 | $0.18 |
| cross_tipped | rd_pro__default | 112x56 | cross_tipped | run01 | $0.18 |
| event_blizzard | rd_pro__default | 72x24 | event_blizzard | run03 | $0.18 |
| event_broken_part | rd_pro__default | 168x66 | event_broken_part | run03 | $0.18 |
| event_fire | rd_pro__default | 216x60 | event_fire | run07 | $0.18 |
| event_fruit | rd_pro__default | 48x20 | event_fruit | run02 | $0.18 |
| event_hail | rd_pro__default | 64x24 | event_hail | run09 | $0.18 |
| event_snowdrift | rd_pro__default | 80x20 | event_snowdrift | run03 | $0.18 |
| event_thief | rd_pro__default | 64x72 | event_thief | run17 | $0.18 |
| event_thunderstorm | rd_pro__default | 80x24 | event_thunderstorm | run07 | $0.18 |
| hunt_bear_anim | rd_advanced_animation__custom_action | 48x32 | hunt_bear_1, hunt_bear_2, hunt_bear_3, hunt_bear_4 | run01 | $0.25 |
| hunt_bear_dead | rd_advanced_animation__custom_action | 48x32 | hunt_bear_dead | run02 | $0.25 |
| hunt_buffalo_anim | rd_advanced_animation__custom_action | 56x40 | hunt_buffalo_1, hunt_buffalo_2, hunt_buffalo_3, hunt_buffalo_4 | run01 | $0.25 |
| hunt_buffalo_dead | rd_advanced_animation__custom_action | 56x40 | hunt_buffalo_dead | run03 | $0.25 |
| hunt_cactus_1 | rd_pro__default | 60x44 | hunt_cactus_1 | run02 | $0.18 |
| hunt_cactus_2 | rd_pro__default | 60x44 | hunt_cactus_2 | run01 | $0.18 |
| hunt_cactus_3 | rd_pro__default | 60x44 | hunt_cactus_3 | run01 | $0.18 |
| hunt_deer_anim | rd_advanced_animation__custom_action | 48x32 | hunt_deer_1, hunt_deer_2, hunt_deer_3, hunt_deer_4 | run01 | $0.25 |
| hunt_deer_buck_anim | rd_advanced_animation__custom_action | 48x36 | hunt_deer_buck_1, hunt_deer_buck_2, hunt_deer_buck_3, hunt_deer_buck_4 | run01 | $0.25 |
| hunt_deer_buck_dead | rd_advanced_animation__custom_action | 48x36 | hunt_deer_buck_dead | run02 | $0.25 |
| hunt_deer_dead | rd_advanced_animation__custom_action | 48x32 | hunt_deer_dead | run02 | $0.25 |
| hunt_grass_1 | rd_pro__default | 60x20 | hunt_grass_1 | run02 | $0.18 |
| hunt_grass_2 | rd_pro__default | 60x20 | hunt_grass_2 | run01 | $0.18 |
| hunt_grass_3 | rd_pro__default | 60x20 | hunt_grass_3 | run01 | $0.18 |
| hunt_hunter_dirs | rd_advanced_animation__rotate | 48x64 |  | run04 | $0.10 |
| hunt_hunter_down_anim | rd_advanced_animation__custom_action | 48x64 | hunt_hunter_down_1, hunt_hunter_down_2, hunt_hunter_down_3, hunt_hunter_down_4 | run04 | $0.25 |
| hunt_hunter_downright_anim | rd_advanced_animation__walking | 48x64 | hunt_hunter_downright_1, hunt_hunter_downright_2, hunt_hunter_downright_3, hunt_hunter_downright_4 | run04 | $0.14 |
| hunt_hunter_downright_diag | rd_advanced_animation__custom_action | 48x64 |  | run02 | $0.25 |
| hunt_hunter_right_anim | rd_advanced_animation__walking | 48x64 | hunt_hunter_right_1, hunt_hunter_right_2, hunt_hunter_right_3, hunt_hunter_right_4 | run02 | $0.14 |
| hunt_hunter_up_anim | rd_advanced_animation__walking | 48x64 | hunt_hunter_up_1, hunt_hunter_up_2, hunt_hunter_up_3, hunt_hunter_up_4 | run02 | $0.14 |
| hunt_hunter_upright_anim | rd_advanced_animation__custom_action | 48x64 | hunt_hunter_upright_1, hunt_hunter_upright_2, hunt_hunter_upright_3, hunt_hunter_upright_4 | run05 | $0.25 |
| hunt_hunter_upright_diag | rd_advanced_animation__custom_action | 48x64 |  | run01 | $0.25 |
| hunt_pine_1 | rd_pro__default | 60x56 | hunt_pine_1 | run02 | $0.18 |
| hunt_pine_2 | rd_pro__default | 60x56 | hunt_pine_2 | run01 | $0.18 |
| hunt_pine_3 | rd_pro__default | 60x56 | hunt_pine_3 | run01 | $0.18 |
| hunt_rabbit_anim | rd_advanced_animation__custom_action | 40x32 | hunt_rabbit_1, hunt_rabbit_2, hunt_rabbit_3, hunt_rabbit_4 | run01 | $0.25 |
| hunt_rabbit_dead | rd_advanced_animation__custom_action | 40x32 | hunt_rabbit_dead | run02 | $0.25 |
| hunt_rock_1 | rd_pro__default | 60x36 | hunt_rock_1 | run02 | $0.18 |
| hunt_rock_2 | rd_pro__default | 60x36 | hunt_rock_2 | run01 | $0.18 |
| hunt_rock_3 | rd_pro__default | 60x36 | hunt_rock_3 | run02 | $0.18 |
| hunt_squirrel_anim | rd_advanced_animation__custom_action | 40x32 | hunt_squirrel_1, hunt_squirrel_2, hunt_squirrel_3, hunt_squirrel_4 | run01 | $0.25 |
| hunt_squirrel_dead | rd_advanced_animation__custom_action | 40x32 | hunt_squirrel_dead | run02 | $0.25 |
| hunt_tree_1 | rd_pro__default | 60x56 | hunt_tree_1 | run01 | $0.18 |
| hunt_tree_2 | rd_pro__default | 60x56 | hunt_tree_2 | run01 | $0.18 |
| hunt_tree_3 | rd_pro__default | 60x56 | hunt_tree_3 | run01 | $0.18 |
| portrait_matt | rd_pro__default | 64x160 | portrait_matt | run01 | $0.18 |
| raft_landing | rd_pro__default | 60x48 | raft_landing | run03 | $0.18 |
| raft_raft | rd_pro__default | 64x40 | raft_raft | run03 | $0.18 |
| raft_rock | rd_pro__default | 52x20 | raft_rock | run03 | $0.18 |
| raft_sign | rd_pro__default | 60x36 | raft_sign | run02 | $0.18 |
| scene_hunt_zone0 | rd_tile__single_tile | 64x64 | scene_hunt_zone0 | run03 | $0.06 |
| scene_hunt_zone1 | rd_tile__single_tile | 64x64 | scene_hunt_zone1 | run03 | $0.06 |
| scene_hunt_zone2 | rd_tile__single_tile | 64x64 | scene_hunt_zone2 | run04 | $0.06 |
| scene_hunt_zone3 | rd_tile__single_tile | 64x64 | scene_hunt_zone3 | run03 | $0.06 |
| scene_hunt_zone4 | rd_tile__single_tile | 64x64 | scene_hunt_zone4 | run03 | $0.06 |
| scene_landmark_00 | rd_plus__environment | 384x216 | scene_landmark_00 | run17 | $0.06 |
| scene_landmark_01 | rd_plus__environment | 384x216 | scene_landmark_01 | run07 | $0.06 |
| scene_landmark_02 | rd_plus__environment | 384x216 | scene_landmark_02 | run08 | $0.06 |
| scene_landmark_03 | rd_plus__environment | 384x216 | scene_landmark_03 | run05 | $0.06 |
| scene_landmark_04 | rd_plus__environment | 384x216 | scene_landmark_04 | run05 | $0.06 |
| scene_landmark_05 | rd_plus__environment | 384x216 | scene_landmark_05 | run07 | $0.06 |
| scene_landmark_06 | rd_plus__environment | 384x216 | scene_landmark_06 | run02 | $0.06 |
| scene_landmark_07 | rd_plus__environment | 384x216 | scene_landmark_07 | run04 | $0.06 |
| scene_landmark_08 | rd_plus__environment | 384x216 | scene_landmark_08 | run02 | $0.06 |
| scene_landmark_09 | rd_plus__environment | 384x216 | scene_landmark_09 | run04 | $0.06 |
| scene_landmark_10 | rd_plus__environment | 384x216 | scene_landmark_10 | run03 | $0.06 |
| scene_landmark_11 | rd_plus__environment | 384x216 | scene_landmark_11 | run03 | $0.06 |
| scene_landmark_12 | rd_plus__environment | 384x216 | scene_landmark_12 | run09 | $0.06 |
| scene_landmark_13 | rd_plus__environment | 384x216 | scene_landmark_13 | run14 | $0.06 |
| scene_landmark_14 | rd_plus__environment | 384x216 | scene_landmark_14 | run06 | $0.06 |
| scene_landmark_15 | rd_plus__environment | 384x216 | scene_landmark_15 | run08 | $0.06 |
| scene_landmark_16 | rd_plus__environment | 384x216 | scene_landmark_16 | run04 | $0.06 |
| scene_landmark_17 | rd_plus__environment | 384x216 | scene_landmark_17 | run03 | $0.06 |
| scene_matt_store | rd_plus__environment | 384x216 | scene_matt_store | run01 | $0.06 |
| scene_title | rd_plus__environment | 384x216 | scene_title | run01 | $0.06 |
| travel_icon_01 | rd_pro__default | 97x33 | travel_icon_01, travel_icon_02, travel_icon_09, travel_icon_12 | run04 | $0.18 |
| travel_icon_03 | rd_pro__default | 120x60 | travel_icon_03 | run02 | $0.18 |
| travel_icon_04 | rd_pro__default | 40x26 | travel_icon_04 | run01 | $0.18 |
| travel_icon_05 | rd_pro__default | 170x72 | travel_icon_05 | run04 | $0.18 |
| travel_icon_06 | rd_pro__default | 150x44 | travel_icon_06 | run03 | $0.18 |
| travel_icon_07 | rd_pro__default | 150x44 | travel_icon_07 | none | $0.18 |
| travel_icon_08 | rd_pro__default | 170x72 | travel_icon_08 | run01 | $0.18 |
| travel_icon_10 | rd_pro__default | 140x48 | travel_icon_10 | run04 | $0.18 |
| travel_icon_11 | rd_pro__default | 150x64 | travel_icon_11 | run01 | $0.18 |
| travel_icon_13 | rd_pro__default | 170x72 | travel_icon_13 | run03 | $0.18 |
| travel_icon_14 | rd_pro__default | 130x26 | travel_icon_14 | run03 | $0.18 |
| travel_icon_15 | rd_pro__default | 170x72 | travel_icon_15 | run02 | $0.18 |
| travel_icon_16 | rd_pro__default | 97x33 | travel_icon_16 | run01 | $0.18 |
| travel_icon_17 | rd_pro__default | 160x36 | travel_icon_17 | run07 | $0.18 |
| travel_wagon_anim | rd_advanced_animation__walking | 156x60 | travel_wagon_1, travel_wagon_2, travel_wagon_3, travel_wagon_4, travel_wagon_5, travel_wagon_6, travel_wagon_7, travel_wagon_8 | run01 | $0.14 |
| ui_banner_arrival | rd_pro__default | 256x32 | ui_banner_arrival | run02 | $0.18 |
| ui_banner_topten | rd_pro__default | 256x32 | ui_banner_topten | run03 | $0.18 |
| ui_button | rd_pro__default | 48x16 | ui_button | run01 | $0.18 |
| ui_icon_axle | rd_pro__default | 32x32 | ui_icon_axle | run01 | $0.18 |
| ui_icon_bullets | rd_pro__default | 32x32 | ui_icon_bullets | run01 | $0.18 |
| ui_icon_clothing | rd_pro__default | 32x32 | ui_icon_clothing | run01 | $0.18 |
| ui_icon_food | rd_pro__default | 32x32 | ui_icon_food | run01 | $0.18 |
| ui_icon_money | rd_pro__default | 32x32 | ui_icon_money | run01 | $0.18 |
| ui_icon_oxen | rd_pro__default | 32x32 | ui_icon_oxen | run01 | $0.18 |
| ui_icon_tongue | rd_pro__default | 32x32 | ui_icon_tongue | run01 | $0.18 |
| ui_icon_wheel | rd_pro__default | 32x32 | ui_icon_wheel | run01 | $0.18 |
| ui_logo | rd_pro__typography | 256x64 | ui_logo | run02 | $0.18 |
| ui_panel | rd_pro__default | 48x48 | ui_panel | run01 | $0.18 |

**Total: $16.78** to run all 105 jobs again, one attempt each.
