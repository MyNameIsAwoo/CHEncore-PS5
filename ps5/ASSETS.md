# Assets

The assets the title ships, written from `ps5/assets.json` by
`ps5/tools/build-assets.py --notices`. Only assets with a clear licence are
shipped: in PS5_VulkanTemplate, the asset pack's other files (the `assets`
submodule) stay out, and the samples that use them get the replacements here.

| Path under `/app0/assets/` | Asset | Author | Licence | Samples |
| --- | --- | --- | --- | --- |
| `Roboto-Medium.ttf` | [Roboto Medium](https://fonts.google.com/specimen/Roboto) | Christian Robertson (Google Fonts) | Apache-2.0 | all |
| `certs/ca-bundle.crt` | [CA certificate bundle](https://curl.se/ca/cacert.pem) (Bundled certificate snapshot for HTTPS verification, dated 2026-08-23.) | Mozilla CA Certificate Program | MPL-2.0 | cloneheronative |
| `branding/menu-logo.png` | [CH: Encore typography]() (CH and ENCORE text using the licensed Roboto font. No PC artwork.) | HOWL | MIT; rendered with Apache-2.0 Roboto | cloneheronative |
| `controller_profiles` | [Portable controller profiles](local:assets/controller_profiles) (Verified controller report facts and portable JSON configuration; no artwork.) | HOWL project contributors | MIT | cloneheronative |
| `cloneheronative/sounds/menu_intro.wav` | [Encore main menu opening cue]() (Original 48 kHz stereo WAV; plays once on opening the main menu before song previews.) | Project owner supplied | Proprietary; supplied and approved by the project owner for inclusion in CH: Encore builds (2026-11-18). No standalone asset licence is granted. | cloneheronative |

## Launcher presentation artwork

| Packaged path | Asset | Author | Licence |
| --- | --- | --- | --- |
| `sce_sys/pic0.dds, sce_sys/pic1.dds` | CH: Encore typographic branding (Original text layout using the licensed Roboto font. No PC artwork.) | HOWL | MIT; rendered with Apache-2.0 Roboto |
| `sce_sys/icon0.png` | CH: Encore typographic branding (Original text layout using the licensed Roboto font. No PC artwork.) | HOWL | MIT; rendered with Apache-2.0 Roboto |
