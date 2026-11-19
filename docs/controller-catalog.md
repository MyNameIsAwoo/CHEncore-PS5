# Automatic controller catalog and transports

Controller report specifications were checked against PlasticBand documentation
commit `a7b5ca304969717a6486fa21f1a42fb044d1150f` and PlasticBand-Unity commit
`14a30ec79c2d41a617ff146985a1c9a082ed9ba1`. The latter's MIT notice accompanies
the catalog. `assets/controller_profiles/catalog.json` records individual sources.

The catalog includes both PDP Riffmaster modes (`0e6f:024a` PS4 and
`0e6f:0249` PS5), Mad Catz/PDP Rock Band 4 guitars and drums, CRKD PS4/PS5
Gibson SG modes, PS3/Wii Rock Band receivers, GH World Tour PC guitar and PS3
five-lane drums. Existing player/device custom profiles have priority.
The original `12ba:0100` profile also matches the standard PS3 Guitar Hero fret
and menu report and remains compatible with the existing DIY guitar.

Santroller/RetroCultMods HID profiles use `1209:2882` with revision-specific
selectors: `0300` GH guitar, `0400` RB guitar, `0500` GH five-lane drums and
`0600` RB pro drums. Firmware using console emulation instead presents that
emulated controller's ID and layout. These presets do not assert compatibility
with every RCM firmware release or arbitrary reconfigured VID/PID. Unknown
layouts retain instrument selection and remapping. Whammy/tilt remain omitted.
RCM's own programming-tool page identifies Santroller as its underlying platform:
https://retrocultmods.com/programming-tool/

Schema 3 adds separate hardware menu-button bindings and cymbal/hat guards. Legacy schema 1/2 files remain readable; gameplay remaps are preserved. See [drum menu controls](drum-menu-navigation.md).

Schema 2 adds masked hats, alternate physical inputs, optional report IDs and
menu navigation axes. Schema 1 profiles remain readable and preserve their
existing interpretation. Masked capture ignores mirrored face-button flags and
packet counters when remapping a known Riffmaster. Both upper and lower frets
map to their corresponding lane. No virtual DualSense or external bridge is
required for the USB transport.

Xbox 360 transport is experimental until physical hardware testing:

- Receivers `045e:0719`, `045e:0291`, `045e:02a9`: select only controller
  interfaces `ff/5d/81`, never the neighboring audio interfaces. Send presence
  and capabilities requests, parse presence/link/input messages, and expose
  independently connected channels as individual input sources. Player seats
  retain Start-based join order. Empty channels do not reserve game slots.
- Wired GH X-plorer `1430:4748` and RB Stratocaster `1bad:0002` use
  `ff/5d/01` controller interfaces and their documented 20-byte USB reports.
  Wired RB drums `1bad:0003` and ION Drum Rocker `1bad:0130` default to four lanes.
- Wireless guitar subtype 6 is Rock Band; subtype 7 is Guitar Hero. They have
  different yellow/blue face-button assignments. Ambiguous drum subtypes ask
  for the instrument instead of inferring a kit. Receiver-channel mappings
  use separate `_chN.json` files. Multiplexed older Xbox pro-cymbal behavior is
  not certified; standard drum mapping remains available.

Wire references:
https://github.com/torvalds/linux/blob/master/drivers/input/joystick/xpad.c
https://github.com/InvoxiPlayGames/x360-research/blob/master/hardware/usb-protocols/argon.md
https://github.com/TheNathannator/PlasticBand/tree/main/Docs

PS5 evidence so far: the attached Riffmaster reports its PS5 ID and streams
64-byte reports with the automatic profile, alongside the existing 27-byte
guitar. The owner also confirmed Options/frets/strum navigation. No Xbox 360 receiver/controller was available. Decoder and slot-routing
host tests do not certify physical USB initialization or kit compatibility.
