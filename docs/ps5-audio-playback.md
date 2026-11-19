# PS5 song audio

`Song` chart sections identify the primary stream with `MusicStream`. The PC `BassAudioManager` static constructor and loader were recovered in Ghidra: it enumerates 14 stem names (`guitar`, `bass`, `rhythm`, `vocals`, `vocals_1`, `vocals_2`, `drums`, `drums_1` through `drums_4`, `keys`, `song`, and `crowd`) against `.opus`, `.ogg`, `.mp3`, and `.wav`. The PS5 loader now uses that same stem-name set, decodes Opus/Vorbis/MP3/WAV files, and sums their synchronized PCM in the audio callback. A nonstandard relative `MusicStream` uses an unused stem channel without replacing named stems. Gameplay now uses fixed-size synchronized streaming rings (Alpha v.34). The legacy full-file reference loader retains its 768 MiB aggregate cap for tests; it is no longer the normal gameplay path.

`gameplay/audio_player.cpp` reads the Ogg page/lacing structure, decodes standard mono or stereo Opus packets with the pinned libopus 1.5.2 port, trims Opus pre-skip and end padding using the stream granule position, and stores bounded 48 kHz stereo PCM. The per-source duration limit is 60 minutes of decoded audio and 256 MiB of encoded Ogg input. The callback only copies/mixes PCM; it does not access files or allocate.

The existing platform audio thread feeds 256 frame, 48 kHz stereo blocks to `sceAudioOut`. The first three seconds of the chart are a silent lead-in. The loader reads `.chart` `Offset` (seconds) and `song.ini` `[song] delay` (milliseconds), seeks or delays the stream as needed, and maps the audio cursor back onto chart time. Pausing gameplay pauses the audio cursor, and the gameplay clock is corrected against that cursor. The PC `song` level affects the named `song` stem; guitar, bass, rhythm, vocals, drums, keys, and crowd levels affect their corresponding stem channels. Master volume is applied above those channel levels. Menu music and selected-song previews use the PC `menu_volume` and `preview_volume` settings respectively.

The title builds and links libopus statically. Its BSD license is staged at `licenses/title/opus-COPYING.txt` in the title package. Original Vorbis/MP3/WAV packages use the vendored stb_vorbis/dr_mp3/dr_wav decoders with bounded stereo/mono resampling to 48 kHz. The decoder accepts standard Ogg Opus mono/stereo mapping family 0. A successful build confirms compilation, link resolution and packaging; it does not establish decoding or audible output on PS5 hardware.


## Menu preview playback

Rechecked PC references through Ghidra MCP: `BassAudioManager..cctor` at `0x18037A6F0` initializes the 14-stem matrix; `MenuMusicProgress` at `0x18013C6F0` derives its fill from playback position divided by the active audio length. Native preview selection, the native excerpt policy, and orange-fret mute are user-requested adaptations.

A song folder's `preview.opus`, `preview.ogg`, `preview.mp3`, or `preview.wav` takes priority; `song_preview` with these extensions is also accepted. Filename capitalization is accepted for dedicated previews. Unreadable preview files fall back to the regular chart stem mixer. Dedicated previews begin at zero and use their authored duration up to 60 seconds; shorter files end naturally. Without one, every supported named stem plus any nonstandard chart `MusicStream` is decoded by the existing mixer, synchronized, and mixed through the audio callback.

Main-menu full-mix excerpts use 60 seconds (or the available source length), with a random start that fits the complete excerpt inside the source. When space allows, the first five seconds and last two seconds are avoided. Short sources play their available length. Playback windows stop at the PCM sample boundary, independently of render-frame timing. Ordinary seek/reload clears that limit for gameplay. Selected-song list previews retain their 30-second cap and use the chart's configured `preview_start_time`, clamped to leave room for the excerpt, or zero for a dedicated preview.

History tracks chart paths rather than list indices. Automatic selection excludes the last song for libraries of two through ten playable songs, and the last ten songs for larger libraries. A sole playable song may repeat. A fresh launch uses the runtime RNG seed. Unreadable audio is skipped until a library rescan. Decoding runs on the worker for both main-menu and song-list previews; the UI/audio callback does not decode files. Preview PCM is limited to its 30/60-second slice and the existing aggregate budget. Gameplay source duration is capped at 60 minutes, with a constant-size PCM ring per stem.

The Now Playing bar is white and measures elapsed excerpt time. Orange/L1 toggles main-menu preview gain without pausing or resetting its cursor, changing the menu selection, muting UI effects, or changing saved volume levels. Its extracted orange fret icon shows MUTE/UNMUTE on the right below the widget; Select to Play occupies the left side of that same row. Muting stays in effect across automatic song changes during that app session. Strum handles main-menu movement. Select starts the chart named by the widget, fixing the old unrelated song-list-index shortcut.

`tests/menu_preview_tests.cpp` checks synchronized sums, dedicated/capitalized/corrupt-file fallback, bounded 45–60-second policy choices, dedicated-file duration caps, and exact integer/fractional-start playback windows, mute/cursor behavior, normal-seek reset, and 60,000 shuffled selections including reordered lists and ten-song exclusion. It uses generated test PCM and ASan/UBSan, with exceptions/RTTI disabled. `tools/test-menu-preview-console.py` registers temporary synthetic fixtures outside the song library only under a frame budget, checks native stem/source/window/mute/rotation traces, captures the UI, and removes its fixture tree. Normal launches cannot register those fixtures. Evidence is stored in `klog/menu-preview/`.

Earlier 30-second implementation verification (2026-11-16): the three-stem fixture used a random start within the complete source interval and ended at 30.000000 seconds; the next song was different and retained mute. Orange toggled mute/unmute/mute while main selection stayed at zero. The dedicated 12-second WAV took priority over the full backing file and ended at 12.000000 seconds. All three bounded runs returned status 0 at 59.9 FPS. Captured screenshots show the white excerpt bar and original orange-fret MUTE/UNMUTE prompt. Fixture tree, trace and test-run files were verified removed; the normal title was relaunched. Eboot SHA-256: `dad24e56b3a4b6c4e754b3960e1bf7456c2f18d12e6ea3117f30193d71b9d951`. These runs exercise synthetic pad input and hardware audio playback; they do not certify listening quality for every downloaded chart.


Preview-length/widget follow-up: a read-only FTP audit of the installed and bundled song roots inspected eight folders and found **zero** dedicated `preview.*` or `song_preview.*` files, so no installed-file average could be calculated. The fallback now chooses 45–60 seconds; a future dedicated file uses its actual duration up to 60 seconds instead of inheriting the fallback length. Sources shorter than the requested excerpt finish naturally. Artwork grew from 70×70 to 90×90 canvas units, the title uses the extracted Lato Semibold font, and the white progress fill is 8 units high with an opaque slate track. The bar's width is unchanged. Play/mute pointer targets occupy separate left/right areas beneath the widget.

Follow-up verification: native build plus ASan/UBSan preview tests passed. The PS5 mixed-preview run selected 49.940199 seconds and stopped at 49.940208 seconds (within one 48-kHz sample), then selected a different real song for a 46.876308-second excerpt while retaining mute. Dedicated-file preference/natural short duration also passed. All three runs returned status 0 at 59.9 FPS. Captures in `klog/menu-preview/` show the revised art/font/bar/prompt layout. Temporary fixtures and scripts were verified removed. Eboot SHA-256: `ad47d81ea1c90067270508391878598654c7931e8ed21fc2f2577418e48127b5`.

Gameplay audio/effect follow-up: see `pc-guitar-feedback.md` for the missing-cue packaging fix, instrument-only miss attenuation, case-insensitive stem lookup, preview-PCM reuse, original guitar effects and callback/console evidence.


## Alpha v.33 bounded preview preparation

Preview workers now probe source duration and decode only the selected excerpt:
60 seconds on the main menu and 30 seconds in song selection. Dedicated previews
start at zero and end naturally if shorter. Named stems and chart MusicStream
retain the existing PC stem matrix. Absolute source time is retained separately
from the buffered slice, so seeking, progress and short stems stay aligned.
Preview slices are never promoted to full gameplay audio: Ready holds until the
full stem mixer is prepared, before advancing the countdown.

WAV, Vorbis and MP3 ranges seek to the required source frame. Opus skips prefix
packets, then decodes one second of preroll before the excerpt. This exceeds the
[Opus Ogg seeking minimum](https://www.rfc-editor.org/rfc/rfc7845.html#section-4.6)
and matched the tested full-decode samples. It still scans compressed prefix
pages; this is bounded PCM preparation rather than a full streaming backend.
The callback continues to perform no allocation or file access. Complete-file
loads release geometric allocation slack after decoding.

All 30 original bundled song previews passed host preparation checks. For the
20-minute, two-stem Coalescence & Segmentation chart, the host sanitizer build
used 23,040,000 bytes of preview PCM versus 460,800,000 for full gameplay,
and loaded in 296 ms versus 8,726 ms in that run. These are host results,
not Switch performance measurements. Evidence and repeatable probes live in
`klog/major-preview-audio/`, `tests/audio_range_tests.cpp`,
`tests/preview_memory_probe.cpp` and `tools/test-bounded-preview-console.py`.
Malformed Opus final pages/granules are rejected; complete missing-EOS sources
retain their established compatibility behavior.


Native Alpha v.33 verification (eboot `f176705f19ac44a2e4d7fe018d456ae7235087303b9072714d225cd4534cd48b`):
the 20-minute chart prepared its two-stem, 60-second excerpt in 293.918 ms,
using 23,040,000 bytes of PCM. Ready then prepared the complete two-stem mix
(460,800,000 bytes) before entering gameplay. A dedicated 12-second preview
took precedence and stopped at the expected sample boundary. An original
four-stem Biology playback/pause/resume check recorded zero audio cursor drift
while paused and at most 22 ms between chart and audio trace samples after
resuming. The visual rewind was observed and chart/audio synchronization was
restored. Evidence: `klog/major-preview-audio/native/verdict.json` and
`klog/major-preview-audio/ready-sync/verdict.json`. Scripted checks do not
measure speaker/display latency or replace a listening test.


## Alpha v.34 streaming gameplay

`loadGameplayStream` discovers the unchanged PC stem/container matrix and opens
persistent Opus, Vorbis, MP3 or WAV decoders. One worker owns all source files.
Each loaded stem has 131,072 stereo 48-kHz frames (524,288 PCM bytes), independent
of song length. The worker prepares at least 24,000 source frames before Ready
can complete, then keeps up to roughly 2.73 seconds ahead. Decoder state and
compressed-packet scratch are additional bounded memory; PCM byte telemetry is
not whole-process RAM usage. Startup metadata probes can still scan compressed
MP3 input, so loading cost is format-dependent.

All stems consume a common fixed-point source cursor, including playback-rate
changes. The callback consumes an interval only when every loaded stem has that
interval available. Short stems contribute silence after their authored end.
Starvation leaves the shared cursor unchanged and is counted; the chart clock
holds while buffering. The callback performs no allocation, file access, locking
or waiting. UI effects remain independently available while the song is paused.

Seeks are generation-tagged requests to the worker. Old buffers cannot become
visible under a new generation. The worker waits for callback readers before
resetting rings; the main thread does not wait for seek completion. Opus seeks
skip compressed prefix packets and decode one second of preroll; archive formats
seek their existing decoder and preserve the 48-kHz resampling phase. This avoids
opening/decoding the whole source at every refill. Playback windows, natural EOF,
pause, practice rates, restart and resume rewind preserve the previous API.

Loading cancellation is cooperative between reads/pages and stems; it cannot
interrupt an OS read already in progress. Job arguments remain immutable until
join. Changing song/context cancels the outstanding job, and canceled or stale
results are discarded. Leaving Ready clears its pending launch before reaping
workers, preventing an old request from being rescheduled. Storage/decoder read
failure stops the shared stream and pauses gameplay with a recoverable message.

Checks cover six source format/rate fixtures, exact/toleranced comparisons to the
full decoder, gain/rate changes, rapid seeks, playback-window EOF, move/teardown,
truncated input and cancellation. All 30 original bundled songs pass streaming
preparation on the host sanitizer build. The original 20-minute two-stem chart
now reserves 1,048,576 PCM bytes rather than 460,800,000 bytes for a full decode.
Evidence is in `klog/engine-streaming/`; native tests and engine comparisons are
summarized in `docs/alpha34-engine-verification.md`.

The PS5 source-loss check injects the same mixer notification used by the mount
observer and verifies the normal pause UI and stationary cursor. This is separate
from the host's actual truncated-file decoder test. Replacing a file through FTP
can leave the console's already-open source readable, so that experiment is not
reported as a physical unplug or native I/O-failure certification. Mount removal
now explicitly stops streaming even when open file data remains cached.


Main-menu fallback preview mixes now fade over their last five seconds, using
PCM sample position rather than render frames. Dedicated preview files keep
their authored audio, and list previews do not receive this envelope. The mixed
song bus is scaled before UI effects are added, so menu cues remain audible.
Full gameplay loads clear the preview envelope. Short fallback sources use the
available length if shorter than five seconds. Host PCM/move/mute/exclusion
checks and native policy/transport checks pass; see `klog/preview-fade/`.
