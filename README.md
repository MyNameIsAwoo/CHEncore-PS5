# CH: Encore — PS5

Huge thanks to my cousin, [Eddie Rubio](https://music.youtube.com/channel/UCmQMUczMFC3UQH1eaTNe1Bw), for the awesome main menu riff.

<p align="center"><img src="ps5/assets/ch-branding/icon0.png" alt="CH: Encore" width="180"></p>

A native PS5 rhythm game with guitar/bass, drums and local multiplayer.

**v1.5.0 — pursuaded** changes to user-supplied PC resources. No original PC artwork, fonts, sound effects or songs are included in the game, source tree or release packages. The old main-menu screenshots have been removed.

## Getting started

1. Download a game-folder ZIP, exFAT image or native homebrew PKG from [Releases](https://github.com/MyNameIsAwoo/CHEncore-PS5/releases).
2. Read [Asset setup](docs/ASSET_SETUP.md). Use the desktop importer only with PC resources you are authorized to extract and use. It does not decrypt protected songs or bypass access controls.
3. Copy `EncoreAssets-v1.encore-assets` to the root of an exFAT USB drive. Open the game and select **Import** on the setup screen.
4. Add your own charts through Encore Browser, `/data/homebrew/SONGS`, or a root-level `SONGS` folder on an exFAT drive. Nested collections are supported.

I wish I could supply a fully built game, but have been asked not to include any assets in the project. Good news, after initial install updates are automatic in-game!

The initial setup screen uses the included open font and needs no PC resources. Imported assets, songs, profiles and scores are kept in persistent storage, outside the application and update runtime. Package/image installs use the same persistent location as folder installs.

## Features

- Five-fret guitar/bass, six-fret, drums, Pro Drums and five-lane layouts.
- Up to four local players, profiles, per-instrument remapping and controller navigation.
- Encore Browser for chart search/downloads; USB/internal song scanning, copying and confirmed removal.
- Local records, lyrics, solos, pause/restart, previews and highway/color options.
- Display-aware resolution and supported 60/120 FPS modes.
- USB MIDI drums with **experimental/intermittent support**; keep a DualSense for menus and pause.
- Verified updates with persistent user resources and startup recovery.

## Updating

Current updater-enabled installs can receive a migration update from the retired repository address. It contains the same asset-free executable as this release. After the one-time asset import, future update checks use this repository. Original PC assets are never downloaded by the updater.

The game-folder, exFAT and native homebrew/debug PKG formats are supported by compatible homebrew setups. These packages are not retail-signed. Preserve the complete title folder and your userdata when manually installing.

## Status and rights

This remains a work in progress. Physical four-instrument, five-lane, six-fret and MIDI testing needs broader reports. The reported RB4 Stratocaster problem still needs a tester log/retest. Online play, whammy and tilt are unfinished. PS4/Switch ports have no release date.

This project is not affiliated with, approved by or endorsed by the original PC developers. Supplying your own files does not itself grant permission to extract, modify or reuse them. The project does not claim that changing its name or removing its assets resolves all copyright, contractual or anti-circumvention questions. See [rights and distribution notes](docs/RIGHTS_AND_DISTRIBUTION.md).

## Building and reports

Native builds use the vendored prospero toolchain and the existing Vulkan 1.3/RADV renderer. See `ps5/README.md`, the corresponding-source archives, and `BUILDING.txt` in each release. The user asset pack is never a build input or release attachment.

Read [known issues](KNOWN_ISSUES.md) and [diagnostics](docs/beta-diagnostics.md) before reporting a problem. Include console/firmware, launcher/version, controller/mode and reproduction steps. Discord: **mynameisawooo**.

## Credits and licences

Native code and third-party components retain their individual licence notices. The linked executable and corresponding source follow their applicable combined-work obligations. The asset importer contains metadata and code, not the original PC assets. Roboto, dependency certificates and user-supplied main-menu audio retain their stated licences/rights.

PlayStation and PS5 are trademarks of Sony Interactive Entertainment Inc. This is an independent homebrew project.
