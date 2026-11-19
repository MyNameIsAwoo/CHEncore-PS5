# Performance work before a Switch backend

Alpha v.34 keeps the native PS5 Vulkan/RADV setup intact. Its measured PS5
results are not evidence of Switch performance.

The shared gameplay code uses timestamped input and scheduled judgment
boundaries. Host tests cover 30/60/120/240 fps, long frames and complete 2 ms
input pulses. Each physical edge consumes at most one matching guitar head;
drum edges are independent per pad/cymbal/kick. Fresh recovery settles skipped
heads without manufacturing strikes or weakening same-direction debounce.

Menus no longer build/upload hidden highway geometry or run inactive player
judgment. The library worker caches populated difficulty sections, avoiding a
synchronous chart reread when opening song options. Rapid cursor movement
coalesces album decoding/uploads with a 100 ms settle interval; old cover art
is hidden while selection changes. Song filtering and audio preparation retain
their existing caches/workers. PS5 artwork quality and menu layout are retained.

Before a Switch release, implement and measure these against that platform:

- Tune and measure streaming ring sizes, decoder scratch, SD/USB read latency
  and refill watermarks on actual Switch hardware. The shared engine now uses
  fixed rings rather than full-song gameplay PCM; physical underrun measurements
  remain necessary before choosing smaller platform budgets.
- Choose texture/atlas residency and framebuffer budgets from measured GPU/RAM
  limits. Avoid globally reducing the PS5/Pro asset quality for another target.
- Preserve acquisition timestamps and press/release queues in the Switch input
  backend. Reuse the existing shared judgment, remapping and player-seat tests.
- Profile CPU update, GPU frame time, audio underruns and loading separately on
  physical hardware, including four players and large external song libraries.

Only the platform backends should depend on console APIs. Portable timing,
chart parsing, mapping and scoring stay independent of scePad and Vulkan setup.


Alpha v.33 preview preparation now decodes only 30/60-second excerpts. All 30
bundled songs pass the host preview sweep; the 20-minute, two-stem case uses
23 MB PCM rather than 461 MB. Alpha v.34 also streams full gameplay using 524,288 PCM bytes per loaded stem.
Switch still requires its own memory/latency profiling and budget selection.
See `docs/ps5-audio-playback.md` and `klog/major-preview-audio/` for evidence.
