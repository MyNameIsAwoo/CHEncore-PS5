# Menu opening audio

The owner-supplied `menu.wav` is an 8.986333-second, 48 kHz, stereo, 24-bit PCM clip. The asset manifest records its origin and distribution approval. It decodes once during startup with a bounded 30-second PCM budget, and plays once when the main menu first becomes interactive.

Preview loading and playback wait for the audio callback to finish the cue. The Now Playing widget and prompts then fade in over 0.6 seconds. Leaving the main menu cancels the cue without replaying it on return. Missing audio or an unavailable output skips the cue and keeps menus usable. Existing preview fade-in/fade-out behavior remains in place.

Ghidra MCP reference: PC `MenuMusicProgress` at `0x18013C6F0` reads the active audio position divided by its length; this progress behavior is preserved. The opening cue and delayed widget reveal are Encore additions.

Host ASan/UBSan validation mixes the supplied 24-bit clip through all 431,344 frames, checks exact playback completion, missing-file fallback, cancellation and existing confirmation cues. Console validation is recorded separately.

PS5 Pro verification: published build `c6b6a1b63b8829edeef65d542cc501db88762931`, executable SHA-256 `11a15d599daccb0bc8b8699b04cfc18f076eed5c59082a4d2f823b8ac64c14b0`. The 4K main-menu run returned status 0 at 59.9 FPS; the cue completed after 8.999 seconds, then preview decoding and playback began. Navigating away cancelled the cue after 0.147 seconds in a second successful native run. Normal launch restored. Kernel-log service refused connection, so validation uses application diagnostics, output block counters, result files and captures; audible balance needs user review. All 19 release attachments were downloaded and verified, including the 573-file application manifest.
