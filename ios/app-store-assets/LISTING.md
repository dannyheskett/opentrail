# OpenTrail — App Store listing

`scripts/asc_release.py listing` pushes the fields below and the screenshots in
`screenshots/` (`release.yml`); `scripts/asc_setup.py app-info` sets the
category, age rating, URLs and price. Mirrors `android/play-assets/LISTING.md`
with Apple's differences. Never mention another platform or store in the copy:
Apple rejects listings that do.

## New App form (Apps → + → New App)

| Field | Value |
| --- | --- |
| Platform | iOS |
| Name | `OpenTrail 1848` (`OpenTrail` was taken; the home screen says opentrail, `CFBundleDisplayName`) |
| Primary Language | English (U.S.) |
| Bundle ID | `com.danheskett.opentrail` |
| SKU | `opentrail-free` |
| User Access | Full Access |

## Subtitle (≤30 chars)

```
A wagon journey west in 1848
```

## Promotional text (≤170 chars)

```
Lead your wagon party 2,000 miles west in 1848: rivers, hunting, trading and the rapids of the Columbia. No ads, no tracking, no accounts. Free and open source.
```

## Keywords (≤100 chars, comma separated, no spaces)

```
wagon,pioneer,1848,frontier,history,educational,retro,hunting,river,raft,oxen,pixel
```

## Description (≤4000 chars)

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
• Landscape, fills the screen, keeps clear of the notch and the home indicator

NO ADS, NO TRACKING, NO ACCOUNTS
OpenTrail never touches the network. Your Top Ten and the graves along the trail stay on your device.

FREE AND OPEN SOURCE
Read the code, report a bug, or build it yourself: https://github.com/dannyheskett/opentrail
```

## App Review notes

```
Thank you for reviewing OpenTrail. It needs no account, sign-in or network access. Every choice is answered by tapping it; the menu button at the top right turns the sound off or quits a game. In the hunt, tap the field to walk the hunter there and double-tap to shoot that way.
```

## App information

- **Category:** Games → Adventure, and Simulation (Apple has no Educational
  subcategory for games)
- **Content Rights:** contains third-party content, used with permission (NOTICE)
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
