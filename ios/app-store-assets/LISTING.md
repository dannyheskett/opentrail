# OpenTrail — App Store listing

`scripts/asc_release.py listing` pushes the fields below and the screenshots in
`screenshots/` (`store-release.yml`); `scripts/asc_setup.py app-info` sets the
category, age rating, URLs and price. Mirrors `android/play-assets/LISTING.md`
with Apple's differences. Never mention another platform or store in the copy:
Apple rejects listings that do.

## New App form (Apps → + → New App)

| Field | Value |
| --- | --- |
| Platform | iOS |
| Name | `OpenTrail 1848` (`OpenTrail` was taken; the home screen still says OpenTrail, `CFBundleDisplayName`) |
| Primary Language | English (U.S.) |
| Bundle ID | `com.danheskett.opentrail` |
| SKU | `opentrail-free` |
| User Access | Full Access |

## Subtitle (≤30 chars)

```
The 1985 wagon trail classic
```

## Promotional text (≤170 chars)

```
The classic journey west in 1848, rebuilt rule for rule with new pixel art and the original tunes. No ads, no tracking, no accounts. Free and open source.
```

## Keywords (≤100 chars, comma separated, no spaces)

```
wagon,pioneer,1848,frontier,history,educational,retro,classic,hunting,river,MECC
```

## Description (≤4000 chars)

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
• Landscape, fills the screen, keeps clear of the notch and the home indicator

NO ADS, NO TRACKING, NO ACCOUNTS
OpenTrail never touches the network. Your Top Ten and the graves along the trail stay on your device.

FREE AND OPEN SOURCE
Read the code, report a bug, or build it yourself: https://github.com/dannyheskett/opentrail
```

## App Review notes

```
Thank you for reviewing OpenTrail, a rebuild of the 1985 MECC classic, published with the permission of the original game's rights holder. It needs no account, sign-in or network access. Every choice is answered by tapping it; the menu button at the top right turns the sound off or quits a game. In the hunt, tap the field to aim and use Walk and Fire under it.
```

## App information

- **Category:** Games → Adventure, and Simulation (Apple has no Educational
  subcategory for games)
- **Content Rights:** contains third-party content (the original game's, used with its rights holder's permission)
- **Age Rating:** infrequent/mild cartoon violence and guns (hunting animals, pixel art), infrequent/mild mature themes (members of the party die of disease and accidents); everything else None
- **Copyright:** `2026 Dan Heskett`
- **Support URL:** https://github.com/dannyheskett/opentrail/issues
- **Marketing URL:** https://danheskett.com/projects/opentrail/
- **Privacy Policy URL:** https://danheskett.com/app/privacy-policy/

## App Privacy (App Store Connect → App Privacy)

Answer **"No, we do not collect data from this app."** — no network code, no
analytics, no permissions. This yields a "Data Not Collected" label. Console
only: Apple has no API for it.

## Screenshots

`screenshots/iphone-6.9/` (2796×1290, landscape) and `screenshots/ipad-13/`
(2732×2048), made by `tools/store_shots.py` from the real game.

## Pricing

Free. No in-app purchases.

## Export compliance

No encryption: `ITSAppUsesNonExemptEncryption = false` is in `ios/Info.plist`.
