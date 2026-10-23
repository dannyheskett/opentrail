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
OpenTrail
```

## Short description (≤80 chars)

```
The 1985 classic of the wagon trail west, rebuilt. Free, open source, no ads.
```

## Full description (≤4000 chars)

```
It is 1848. Your wagon party of five sets out from Independence, Missouri, on the 2,000-mile trail to Oregon's Willamette Valley. Buy oxen, food and spare parts at Matt's General Store, choose your pace and rations, hunt for food, ford rivers or float your wagon across, trade with the people you meet, and keep your party alive through storms, broken axles and disease.

OpenTrail is the 1985 MECC classic, rebuilt rule for rule: the same trail, landmarks, rivers, events, hunting game, raft run down the Columbia and Top Ten, published with the permission of the original game's rights holder. Every choice is numbered and answered with a tap.

NEW PIXEL ART, THE ORIGINAL TUNES
• Every landmark, river crossing and event redrawn as new pixel art
• The original's tunes, note for note, played by a small orchestra
• The hunt is the original's game: tap the field to aim, then fire

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
- **Website:** https://danheskett.com
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
