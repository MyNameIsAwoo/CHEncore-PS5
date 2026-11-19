# Alpha v.34 engine verification

The native PS5 build uses the vendored prospero-clang++ toolchain. Vulkan 1.3,
volk/RADV, dynamic rendering and the existing direct-display swapchain are
preserved. The in-game version and history are updated. No artwork was replaced.

Engine-verification eboot SHA256: `c19d79cb6b68e11b95d7677ce021c7b6d8f7196ba61ae415ec0a60b9cf116bcc`.
Gameplay-object SHA256 at engine verification: `53aa17fa307118001b093bec7d17fec39a86fafa92acc27c535327d1e21a7694`.

| Area | Result |
| --- | --- |
| Gameplay streaming | Persistent Opus/Vorbis/MP3/WAV decoders, fixed synchronized rings, shared cursor, no callback allocation/file access/locks/waits |
| Loading/cancellation | Canceled/stale jobs cannot launch a song or replace a later preview; pending Ready cancellation and retry pass |
| Storage loss | Decoder failures and mount loss stop the shared stream and open pause; source-loss notification tested on PS5 |
| Gameplay comparisons | 19,216 recorded four-player samples match the PC at equivalent timing; six additional MIDI/chart cases match exactly |
| Rendering | In-place stable draw ordering plus contiguous batching and reduced redundant state binds; PC layer keys retained |

Gameplay PCM rings reserve 524,288 bytes per loaded stem. The original 20-minute,
two-stem Coalescence & Segmentation chart uses 1,048,576 PCM bytes rather than
460,800,000 bytes for its legacy full decode. Decoder and packet scratch are
additional bounded memory. All 30 original bundled songs pass host streaming
preparation; this does not measure whole-process RAM or Switch performance.

The callback advances all stems through the same interval or holds the common
cursor when a buffer is short. Short stems are padded with silence and clamp
interpolation at their own EOF, including practice rates. Seeks use generation
handoff, preserve source phase and cannot publish stale ring data. The decoder
revalidates source rate/channels after probing and bounds every PCM read for
stereo even if MP3 metadata changes. A host stress test replaces a source between
mono/stereo files 100 times. Actual read truncation, cancellation and teardown
are covered under ASan/UBSan; format tests compare against full-decoder PCM.

The native original-song check loads four stems, shows zero paused cursor drift,
resumes through the existing rewind, and records at most
22 ms between chart and audio trace samples. TV/speaker
latency and physical controller calibration remain separate measurements.
A native test injects the same source-loss notification used by the mount
observer and verifies visible pause and a stationary audio cursor. Replacing a
file through FTP could leave an old open source readable, so that experiment
is not used as evidence of a physical USB pull. The observer explicitly stops
a removed source instead of waiting for an eventual read error. Reconnect and
return to song selection to open fresh streams.

Four independent reference cases cover the original mixed-instrument replay,
non-grid tempo/time-signature changes, overlapping sustain/solo boundaries, and
roll/cymbal sections. Each compares 1,201 frames across guitar, bass, drums and
pro drums, including score subtotals, streak, health, Star Power and shared band
rank/progress. The six format cases replay loose charts and ZIP-contained MIDI
for guitar/drums/pro drums (2,862 total frames), checking note flags,
solo metadata and exact scores. The native overstrum path now retires unfinished
scoring tails while preserving completed tails' recent-fret allowance. A plain
miss can still retain other held tails, as observed in the PC replay.

Encore's previously requested packed-note recovery remains intentionally
different from the PC chronological gate. The 219BPM overlapping-miss fixture
records that difference in `klog/release-engine-suite/tempo-boundaries/`; it is
not presented as passing. The calculation suite isolates that input-policy
adaptation. Recorded comparisons do not establish universal parity.

The last changes after the reference runs affect only audio decoding/lifecycle.
The gameplay/render object is byte-identical to the one used by those replays
and the profile. Final-binary audio, cancellation and source-loss checks passed. The console
rewrites upload metadata, so the deployed ELF was verified by comparing all
five executable load segments; each matches the local linked ELF.

| Four-player mean | Previous submission | Optimized submission |
| --- | ---: | ---: |
| Draw calls | 288.3 | 213.9 |
| State binds | 864.9 | 96.2 |
| Command recording CPU | 0.162 ms | 0.095 ms |
| Geometry CPU | 0.065 ms | 0.065 ms |
| GPU, including UI | 0.540 ms | 0.538 ms |

Command-recording CPU time fell 41.4% in this run. Vertex counts are
identical; GPU time is essentially unchanged. The in-place sorter matched
`stable_sort` for 2,000 host draw lists without sort allocations. Native GPU timestamp
queries and profile logging run only in the explicit QA profile mode.

Evidence:

- `klog/engine-streaming/verification.json`
- `klog/engine-streaming/all-bundled-streams.json`
- `klog/engine-streaming/ready-sync/verdict.json`
- `klog/engine-streaming/cancellation/verdict.json`
- `klog/engine-streaming/source-loss/verdict.json`
- `klog/release-engine-suite/render-profile/verdict.json`
- `klog/multiplayer-release-audit/verdict.json`
- `klog/release-engine-suite/pc-compatible-*/verdict.json`
- `klog/release-engine-suite/midi/*/verdict.json`

Physical USB unplug/reinsert, listening and four physical instrument checks
remain tester work. Switch still needs its backend, hardware profiling and
platform-specific buffer/texture budgets. Existing Alpha v.32 PKG/exFAT/folder
artifacts remain frozen and have not been repackaged with this code. No public
upload has been made.


## Preview fade follow-up

Current follow-up eboot SHA256: `0f2191da055e1bed4aef6694b15b2fcf85eeec8b7465c6d0f2007e197ade1f43`.

Full-song mixes used by the main-menu widget fade linearly over their final five
seconds. Sources shorter than five seconds fade over their available duration.
Dedicated preview files and song-list previews retain their existing audio.
The envelope follows 48-kHz sample position, preserving all stems' relative
levels and keeping UI effects independent. Muting still advances the preview;
seeking/moving/resetting mixers preserves or clears the envelope as appropriate.

ASan/UBSan preview checks validate the complete envelope, a short source,
dedicated/list exclusions, unfaded UI cues, mute and transport/move behavior.
The native three-case run confirms a five-second mixed-preview envelope, zero
fade for a dedicated file, complete gameplay streaming after the preview and
natural dedicated-file completion. Evidence is in `klog/preview-fade/`.
This follow-up recompiles the title for the mixer-field addition and trace;
the gameplay comparison and renderer measurements above describe the preceding
engine build. Scoring/judgment and renderer submission logic are unchanged.
Distribution packages remain Alpha v.32 until refreshed separately.
