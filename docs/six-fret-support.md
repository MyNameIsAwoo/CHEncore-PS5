# Six-fret guitars

Six-fret tracks use three columns with black and white rows, plus same-column
bar chords and subdued off-white open notes. Open sustains use the same color; active Star Power keeps its selected color. The controller profile vocabulary is W1/W2/W3 then
B1/B2/B3. Five-fret guitar/bass/rhythm and six-fret lead/bass/rhythm/co-op stay
separate: the player-selection menu only enables tracks for that player's
controller family. Known raw USB hardware determines that family; a native
controller can use the six-fret mapping selected in its player profile.

## USB presets

- PS4 Guitar Hero Live dongle: `1430:07BB`.
- PS3/Wii U Guitar Hero Live dongle: `12BA:074B`.

Both expose six independent fret bits and a separate left-stick-Y strum value.
The PS4 report uses bytes 5/6 for the frets and byte 2 for strum. The PS3/Wii U
report uses byte 0 for frets and byte 4 for strum. Dedicated D-pad and menu buttons
remain available. Whammy and tilt stay deferred.

The backend sends the documented HID keep-alive every four seconds using an
asynchronous endpoint-zero transfer per device. That keeps its acknowledgement
from blocking the other USB sources. The PS4 packet is
`30 02 08 0A 00 00 00 00 00`; PS3/Wii U is `02 08 20 00 00 00 00 00`.

## Chart and gameplay references

The PC six-fret matcher at `0x1820F4510` and input-mask construction at
`0x1820F4630` were decompiled through Ghidra. The matcher retains the distinct
black/white ghosting restrictions and exact bar/chord masks. It uses the existing
time-based session, scoring, Star Power, solos and sustain pipeline with a
six-fret chart kind. Five-fret charts still reject a sixth-fret bit.

`.chart` lanes 0/1/2 are white, 3/4/8 are black; 7 is open. Lanes 5/6 remain
HOPO-flip/tap modifiers. MIDI supports the corresponding `PART ... GHL` tracks,
including pitch 100 for Expert Black 3 and pitch 94 for Expert Open. Six-fret
MIDI solos do not use the five-fret legacy Star Power fallback. MIDI cache
version 5 refreshes older conversions.

Tap note 104 and PS SysEx tap ranges include their start and exclude their
endpoint. They annotate note heads rather than shortening authored sustains.
Independent MIDI fixtures verify overlapping ranges, difficulty scope and a
tail continuing past the tap endpoint. The same checks cover five-fret tracks.

`ps5/tools/export-pc-six-fret.py` reads the original PC serialized prefabs and
packs their sprites, pivot geometry, note scale and receptor positions into
`assets/pc_six_fret`. The manifest retains source and pixel hashes. It does not
create replacement artwork.

## Validation scope

Host sanitizer tests cover report decoding with simultaneous frets/strumming,
sixth-fret judgement, row/chord matching and controller-specific track gating.
Independent MIDI fixtures cover all six lanes, opens, solos and tap boundaries.
Physical six-fret hardware is unavailable; dongle initialization, wireless
connection behavior and playing feel require tester confirmation. Do not
interpret synthetic input or bot tests as hardware certification.

## Primary references

- [PlasticBand PS4 report](https://github.com/TheNathannator/PlasticBand/blob/a7b5ca304969717a6486fa21f1a42fb044d1150f/Docs/Instruments/6-Fret%20Guitar/PS4.md)
- [PlasticBand PS3/Wii U report](https://github.com/TheNathannator/PlasticBand/blob/a7b5ca304969717a6486fa21f1a42fb044d1150f/Docs/Instruments/6-Fret%20Guitar/PS3%20and%20Wii%20U.md)
- [Linux HID Sony implementation](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-sony.c)
- [Six-fret chart format](https://github.com/TheNathannator/GuitarGame_ChartFormats/blob/main/docs/Chart-File-Formats/chart-format/Tracks/6-Fret-Guitar.md)
- [Six-fret MIDI format](https://github.com/TheNathannator/GuitarGame_ChartFormats/blob/main/docs/Chart-File-Formats/mid-format/Tracks/6-Fret-Guitar.md)
