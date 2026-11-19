# USB MIDI drums: experimental integration

Enabled in the Beta 1.0.1 maintenance build. Release classification: **experimental/intermittent**. Native setup and injected MIDI-to-gameplay tests pass; a physical MIDI kit is not available for validation. Only class-compliant USB MIDI 1.0 bulk-IN interfaces are supported; proprietary USB protocols, DIN without a compatible USB adapter, and MIDI 2.0 are outside this backend.

- Reference PC MIDI drum input, velocity handling and mapping behavior through Ghidra.
- Add **Drums (MIDI)** to new-controller setup, followed by **4-Lane Pro** or **5-Lane**.
- Show this warning whenever a MIDI option is selected: **A DualSense controller is required for menu navigation when using MIDI drums. MIDI inputs can cause unintended menu selections.**
- Keep all menu navigation on DualSense. Use MIDI hits for the pad-mapping wizard and gameplay only.
- Guide the player through striking each pad/cymbal and kick; persist assignments by device and player.
- Implement USB MIDI transport, including separate cables/devices, Note On velocity, Note Off and velocity-zero handling. Avoid duplicate hits and device cross-talk.
- Check hardware and protocol coverage before advertising support; no MIDI drum hardware has been identified for validation yet.

## PC reference inspected through Ghidra

Open program: `GameAssembly.dll`, project `CLONEHERO_PORT`.

- `MIDIInputManager.Start` at `0x180227FA0` creates the RtMidi input source.
- `0x180228400` assigns a MIDI port to a drum player and adds a separate custom controller to the player's existing controller collection. This supports retaining a gamepad alongside the kit.
- `0x18022B150` drains messages, accepts Note On with positive velocity, and ignores Note Off and zero-velocity Note On.
- `0x18022C080` associates strikes with the selected port/device and timestamps them with a monotonic runtime clock.
- `0x18022B760` matches pitch and configured velocity threshold and enforces the binding's retrigger interval before accepting a strike.
- `0x18022A050` processes all queued strikes at the input-source update, encodes velocity as `0.5 + 0.5 * velocity / 127`, then clears the queue. `0x18022C640` clears drum axes first, so a held MIDI note is not a sustained menu button.
- `MIDISetupMenu` at `0x1801106C0` captures the struck pitch and creates a binding with a default minimum velocity of 10. The PS5 guided flow will use its existing controller-popup styling and the requested explicit DualSense navigation notice.

Raw decompiler observations are retained locally outside the release sources. The PS5 transport is implemented below. These PC findings do not prove physical kit compatibility.

## Optional velocity calibration requested

After selecting the kit layout and mapping pads, offer **Calibrate Velocity**.
Capture a soft hit, then a hard hit for each mapped pad/cymbal. Store the raw
endpoints per pad and normalize incoming velocity through that range before the
existing normal/ghost/accent rules. Reject equal or reversed captures, provide
retry/skip, and retain standard MIDI behavior for skipped or fixed-velocity pads.
Keep DualSense navigation active throughout; MIDI strikes only supply samples.
Calibrating must not turn MIDI Note Off, velocity-zero messages or menu navigation
into drum hits. Repeated strikes and different pads remain independent.

## Kick setup requested

Offer **One kick pad — single or double pedal** or **Two kick pads**. Capture each
kick input and map both to the same gameplay Kick action. Matching MIDI notes are
valid and should be deduplicated as bindings, while every received strike remains
an independent event. A double pedal on one pad uses the one-pad option. Do not
require a Note Off between rapid kicks or apply a shared debounce that suppresses
alternating pads. **Do not offer velocity calibration for kick inputs.** Treat
every positive-velocity kick Note On as a full-strength kick; do not apply pad
soft/hard calibration, accent/ghost classification, or a calibrated velocity
threshold to either kick binding. Continue ignoring velocity-zero Note On messages
as Note Off messages. The optional calibration wizard visits pads and cymbals only.

## Implemented portable core

`gameplay/midi_drums.hpp` now provides USB MIDI 1.0 descriptor and event decoding,
per-source bounded strike queues, explicit pad/cymbal actions, mapping and optional
velocity normalization. Two kick bindings may use the same MIDI note without
creating duplicate hits. Every positive-velocity kick is accepted without pad
velocity calibration; Note Off remains ignored. Mapping and calibration prompt
orders are separate so kicks never enter soft/hard calibration.

`tests/midi_drums_tests.cpp` passes ASan/UBSan cases for descriptor selection,
malformed/release filtering, fragmented transfers, cable/channel separation,
queue overflow accounting, repeated Note On, fast alternating kick inputs,
layouts and calibration bounds. There is no menu-navigation output in this core.
`gameplay/ps5_usb_midi.cpp` implements a separate PS5 USB MIDI worker for up to
four endpoints. It requires approval before claiming an interface, isolates
sources with connection-generation checks, and cross-compiles with the vendored
toolchain. `gameplay/midi_profile.cpp` adds bounded portable JSON persistence,
atomic saves and separate kick/pad binding rules; sanitizer tests cover round-trip
persistence, calibration, malformed data and symlink rejection.

`gameplay/midi_setup.hpp` implements the guided setup state: layout choice,
one/two kick pads, strike mapping, an optional velocity offer, soft/hard samples,
retry/skip and review. A short transition guard affects setup captures only;
gameplay kicks remain independent. Regression checks walk the complete Pro setup
and confirm that calibration visits seven pads/cymbals and no kicks.

The transport, profiles and guided setup are linked into the game. A detected MIDI endpoint opens the translucent new-controller popup; choose Drums (MIDI), then 4-Lane Pro or 5-Lane. The native companion used to confirm the first page owns that kit. MIDI does not create a virtual DualSense, consume a pad slot or emit menu inputs. Up to four MIDI endpoints can feed separate companion players. Remap Controller reopens setup for that player's kit.

Mappings live under userdata/controllers as midi-VID-PID-interface-endpoint JSON files, with separate named-player overrides. Positive Note On messages are converted directly into canonical drum strike events, preserving repeated strikes and timestamps through the existing PC-derived DrumSession. Menu debounce and held-button edge detection are bypassed for MIDI strikes. Disconnecting a linked kit pauses play; reconnect/setup remains available from the main menu. The companion's face buttons do not become drum strikes.

Validation: ASan/UBSan tests exercise 128 alternating kicks at 30/60/120 FPS, simultaneous pad/cymbal chords, malformed/release filtering and four isolated mappings. A native PS5 injected-event run completes the full two-kick Pro wizard, seven pad/cymbal calibrations, JSON save/reload and six independently judged notes with zero misses. The injection is available only in explicit bounded QA runs; normal launches read the USB worker. The native five-lane/one-kick setup also judges all six mapped inputs with zero misses, and the actual USB discovery worker completes beside the connected non-MIDI devices without claiming them. No physical MIDI kit has been tested.

USB framing reference: [USB-IF MIDI 1.0 specification](https://www.usb.org/sites/default/files/midi10.pdf).
