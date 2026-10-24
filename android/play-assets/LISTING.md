# OpenTrail — Google Play store listing

`scripts/play_release.py listing` pushes the fields below and the images in this
folder (`store-release.yml`); the console-only settings are at the end. The
screenshots are made by `tools/store_shots.py` (the real game, by taps).

## Assets (this folder)

| File | Play field | Spec |
|------|-----------|------|
| `icon-512.png` | App icon | 512×512 PNG (`tools/art.py icon`) |
| `feature-graphic-1024x500.png` | Feature graphic | 1024×500 PNG (`tools/store_shots.py`) |
| `screenshots/phone/` | Phone screenshots | 1920×1080 PNG (16:9, landscape) |
| `screenshots/tablet/` | 7-inch and 10-inch tablet screenshots | 2560×1600 PNG |

## App name (≤30 chars)

```
opentrail: 1848
```

## Short description (≤80 chars)

```
Lead a wagon party 2,000 miles west in 1848. Free, open source, no ads.
```

## Full description (≤4000 chars)

```
It is 1848. Your wagon party of five sets out from Independence, Missouri, on the 2,000-mile journey west to the Willamette Valley. Buy oxen, food and spare parts at Matt's General Store, choose your pace and rations, hunt for food, ford rivers or float your wagon across, trade with the people you meet, and keep your party alive through storms, broken axles and disease.

The trail passes the Kansas River crossing, Fort Laramie, Independence Rock and the Blue Mountains, and ends with a raft run down the rapids of the Columbia River. Make it to the valley with your party, your supplies and your money, and earn your place on the Top Ten. Every choice is numbered and answered with a tap.

PIXEL ART AND MUSIC
• Every landmark, river crossing and event drawn as pixel art
• Folk tunes of the trail played by a small orchestra
• A hunting game: tap to walk the hunter there, double-tap to shoot

MADE FOR TOUCH
• Tap a choice; type names and epitaphs on the built-in keyboard
• A menu button for sound and quitting a game, any time
• Landscape, fills the screen, keeps clear of the camera and the gesture bar

NO ADS, NO TRACKING, NO ACCOUNTS
OpenTrail requests no permissions and never touches the network. Your Top Ten and the graves along the trail stay on your device.

FREE AND OPEN SOURCE
Read the code, report a bug, or build it yourself: https://github.com/dannyheskett/opentrail
```

## Categorization (Store settings)

- **App or game:** Game
- **Category:** Educational (or Adventure)
- **Tags:** educational, adventure, simulation, retro, history
- **Email:** the contact address the family's other listings use
- **Website:** https://danheskett.com/projects/opentrail/
- **Content rating (IARC questionnaire):** hunting animals with a rifle
  (pixel art, no blood) and members of the party dying of disease or
  accidents (told in a line of text, with a tombstone) — answer the violence
  questions accordingly; no sexual content, language, drugs, gambling or
  user interaction.

## Data safety (Policy → App content)

- Data collected: **None**. Data shared: **None**.
- The app has no `INTERNET` permission (android/AndroidManifest.xml has no
  permissions at all), so "no data transmitted off the device" is truthful.
- Privacy policy URL: **https://danheskett.com/app/privacy-policy/**
