# Getting OpenTrail into the App Store and Google Play

What the repository does by itself, and the one-time console work it cannot do.
The family's other games (openbounty, openblocks) went through the same steps;
their certificate, API key, upload key and service account are reusable here.

## What runs by itself

| When | Workflow | What |
|---|---|---|
| Every pull request | `ci.yml` | Builds every platform; launches the Android build on an emulator and the iOS build on a Simulator (screenshots kept as artifacts); signs the `.ipa` when the secrets are set |
| Every push to `main` | `release.yml` | The next `release-N`: every platform built and published as a GitHub Release (Linux, Windows, macOS, both web games, the APK, the App Bundle, the `.ipa`); then the AAB to Play's **internal** track and the `.ipa` to **TestFlight**, with "What to Test" notes. Testers only. |
| By hand | `store-release.yml` | Pushes the listings and screenshots from the repo, promotes a build (Play: internal, alpha, beta or production, a staged rollout; Apple: a version with the build attached, submitted for review when ticked). Dry run by default. |
| By hand | `asc-setup.yml` | One-time App Store Connect setup through the API: `bundle-id`, `profile`, `app-info`. Dry run by default. |
| By hand | `devicefarm.yml` | AWS Device Farm fuzz test on real phones (needs `AWS_ROLE_ARN`, `AWS_DEVICEFARM_ARN`). |

The listings are `android/play-assets/LISTING.md` and
`ios/app-store-assets/LISTING.md` (`scripts/store_listing.py --check`
validates them: the stores' length limits, no "Oregon Trail" in the copy, no
other store named on Apple's). The screenshots are made by
`tools/store_shots.py` from the real game; the icon by `tools/art.py icon`.

## The secrets (Settings → Secrets and variables → Actions)

| Secret | What | Used by |
|---|---|---|
| `IOS_CERT_P12` | the Apple Distribution certificate, `.p12`, base64 | signed `.ipa` |
| `IOS_CERT_PASSWORD` | its password | signed `.ipa` |
| `IOS_TEAM_ID` | the team id (10 characters) | signed `.ipa` |
| `IOS_PROVISIONING_PROFILE` | the App Store profile **for com.danheskett.opentrail**, base64 (from `asc-setup profile`) | signed `.ipa` |
| `ASC_KEY_P8` | the App Store Connect API key, the `.p8` text (not base64) | TestFlight, store-release, asc-setup |
| `ASC_KEY_ID`, `ASC_ISSUER_ID` | its id and issuer | the same |
| `PLAY_UPLOAD_KEYSTORE` | the upload keystore, base64 | the AAB |
| `PLAY_KEY_ALIAS`, `PLAY_KEYSTORE_PASSWORD`, `PLAY_KEY_PASSWORD` | its alias and passwords | the AAB |
| `PLAY_SERVICE_ACCOUNT_JSON` | the Play service account's JSON | the internal track, store-release |
| `AWS_ROLE_ARN`, `AWS_DEVICEFARM_ARN` | optional: Device Farm | devicefarm |

```
base64 -w0 distribution.p12        | gh secret set IOS_CERT_P12
gh secret set IOS_CERT_PASSWORD    -b '...'
gh secret set IOS_TEAM_ID          -b '...'
gh secret set ASC_KEY_P8           < AuthKey_XXXXXXXXXX.p8
gh secret set ASC_KEY_ID           -b '...'
gh secret set ASC_ISSUER_ID        -b '...'
base64 -w0 upload.jks              | gh secret set PLAY_UPLOAD_KEYSTORE
gh secret set PLAY_KEY_ALIAS       -b '...'
gh secret set PLAY_KEYSTORE_PASSWORD -b '...'
gh secret set PLAY_KEY_PASSWORD    -b '...'
gh secret set PLAY_SERVICE_ACCOUNT_JSON < play-service-account.json
```

Without a secret the step that needs it says so and is skipped; the release
still ships everything else (an unsigned `.ipa`, no AAB).

## Apple, once

1. With the `ASC_*` secrets set: run `asc-setup.yml` with `bundle-id` (registers
   `com.danheskett.opentrail`), then `profile` (creates the App Store profile
   for the team's distribution certificate and prints it base64: store it as
   `IOS_PROVISIONING_PROFILE`). Dry run first, then for real.
2. App Store Connect → Apps → + → New App, with the values in
   `ios/app-store-assets/LISTING.md` ("New App form"). Console only. (Done:
   "OpenTrail 1848", SKU `opentrail-free`.)
3. Run `asc-setup.yml` with `app-info` (category, age rating, URLs, free, every
   territory but mainland China).
4. App Privacy → "No, we do not collect data from this app". Console only.
5. App Review Information (the reviewer contact) once on any version; later
   releases copy it.
6. Push to `main`: the release uploads the signed `.ipa` to TestFlight. Add
   yourself to the internal testing group to install it.
7. When ready: `store-release.yml` with platform `ios`, `submit_for_review`
   ticked (a dry run first).

## Google Play, once

1. Play Console → Create app: name `OpenTrail`, English (US), Game, Free,
   both declarations. Package name `com.danheskett.opentrail` (it is fixed by
   the first upload).
2. The first App Bundle goes up by hand (Play's API cannot create the first
   release): take `opentrail-build-N-android.aab` from the GitHub Release,
   Testing → Internal testing → Create release, upload. From then on
   `release.yml` uploads every release to the internal track.
3. Grant the service account (the one in `PLAY_SERVICE_ACCOUNT_JSON`) access to
   the app (Users and permissions), with release and listing rights.
4. App content: Data safety (no data collected or shared), the content rating
   (IARC questionnaire: see "Categorization" in the listing), target audience,
   ads (none), the privacy policy URL.
5. Store listing: `store-release.yml` with platform `android` and
   `push_listing` pushes the text, icon, feature graphic and screenshots.
6. Production access for a new personal account needs a closed test with 12
   testers for 14 days first (Testing → Closed testing).
7. When ready: `store-release.yml` with `android_track: production` (and a
   `rollout` fraction for a staged release).

## Before the public listing

The original game's content is published with the permission of its rights
holder (NOTICE). Confirm that the permission covers distribution in the app
stores before the first public release on either store.
