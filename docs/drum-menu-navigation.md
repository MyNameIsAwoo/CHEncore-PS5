# Drum menu controls — Alpha v.38

Rock Band PS3/Wii and PS4 drum documentation specifies normal D-pad and face-button input. Guitar Hero five-lane kits also expose those controls. Menus use those hardware controls alongside the existing colored pad/cymbal shortcuts; drum gameplay bindings remain separate.

## Controls

| Menu function | Hardware control / existing fallback |
| --- | --- |
| Move through menus | Kit D-pad; left/right also work in the keyboard and lists that support them |
| Confirm | Green / Cross; green pad or cymbal fallback |
| Back | Red / Circle; red pad fallback |
| Yellow action | Yellow / Triangle; yellow pad or cymbal fallback |
| Blue action | Blue / Square; blue pad or cymbal fallback |
| Orange action | Five-lane orange cymbal/button; existing kick shortcut on four-lane kits |
| Join / player menu / pause | Start / Options |
| Search / Select action | Select / Share |

The same canonical menu action is combined before edge detection, so a mirrored face/pad input does not create two confirmations. Existing menu bounce filtering remains in use. Context hints identify the D-pad when a drum profile controls the menu.

## Report details

Rock Band 4 face buttons share the upper nibble of report byte 5 with the D-pad's lower nibble, while pad/cymbal velocities are in bytes 43–49. Face-only presses previously produced no mapped pad and could be ignored by both the UI and USB transition history. Schema 3 adds separate `menu.buttons` digital locators. `applyProfile()` and the gameplay action vocabulary are unchanged.

Older Rock Band kits multiplex yellow/blue cymbal identification onto D-pad Up/Down while the byte-1 cymbal flag is set. Their `menu.hat_ignore_when` locator suppresses those tags as directional menu input. Physical D-pad input works normally when the flag is clear. Gameplay still reads the original reports.

Schema 1/2 files remain readable. Legacy device overrides inherit missing menu controls from a cached, matching bundled hardware profile; gameplay remaps are retained. Explicit schema-3 menu definitions are preserved. No additional per-report file reads are introduced.

## References and validation

- [Rock Band PS4 report](https://github.com/TheNathannator/PlasticBand/blob/a7b5ca304969717a6486fa21f1a42fb044d1150f/Docs/Instruments/4-Lane%20Drums/PS4.md)
- [Rock Band PS3/Wii report](https://github.com/TheNathannator/PlasticBand/blob/a7b5ca304969717a6486fa21f1a42fb044d1150f/Docs/Instruments/4-Lane%20Drums/PS3%20and%20Wii.md)
- [Guitar Hero five-lane report](https://github.com/TheNathannator/PlasticBand/blob/a7b5ca304969717a6486fa21f1a42fb044d1150f/Docs/Instruments/5-Lane%20Drums/PS3.md)
- PC `PlayerProfileMenu` open entry `0x180178310` inspected through Ghidra; the existing join/profile menu behavior is retained.

Host sanitizer fixtures cover face-only input with zero velocity, all D-pad directions with simultaneous face input, wrong report IDs/lengths, counter-only packets, schema round trips, malformed menu locators, saved-remap inheritance and older cymbal hat tags. Physical kit validation remains separate.

## Multiple USB controllers

Each active USB source retains its own reports, transition queue, previous input and menu filter. Source slots remain stable when another device joins; player numbering follows Start/join order. Hardware menu metadata is cached per device, including when identical models are attached.

`tests/multi_usb_input_isolation_tests.cpp` exercises four interleaved streams, two identical model IDs, counter-only traffic, overflow confined to one queue, stable source assignment and independent menu filters. AddressSanitizer/UndefinedBehaviorSanitizer checks passed. A bounded native PS5 run joined four simulated USB-source slots and independently closed one player menu while the other three remained open. These checks do not substitute for four physical controllers or a physical drum kit.
