# RB4 Bluetooth and RetroCultMods guitar input

This work adds a PS5 native instrument path for the PlayStation 4 Mad Catz
RB4 Stratocaster (0738:8261). Physical Stratocaster validation is still pending;
a successful mock test or build is not confirmation that a paired guitar works.
Beta 1.0.1 includes this experimental path. A tester reported no input from a
paired PS4 Stratocaster in the original 1.0.1 build. The same-version refresh
adds discovery revision 2; a physical retest is still required.

## PS4 RB4 Stratocaster on PS5

Pair the guitar in the console's Bluetooth Accessories settings and assign it
to a signed-in user. Encore checks both standard and special port handles for the guitar device
class, including handles already opened by the service. If necessary it probes
the known Mad Catz identity with `scePadOpenExt` on special port 2, using the
zero and repeated second-product-field forms of the public interface. Standard
and extended information queries identify the class; accepting a vendor ID
alone is insufficient. A connected-class device whose first input sample is
delayed has a three-second grace period. Disconnected placeholders never claim
a source. The standard controller remains available while a source is free.
Only handles opened by this backend are closed by it.

The class decoder merges the upper and solo fret banks into Encore's existing
five-fret input. Native strum, Options and system-interception information are
preserved. It does not add menu-repeat filtering to gameplay or synthesize
strum edges. Guitar text entry uses the controller keyboard.

Disconnect and sign-out release only that source. A disconnected standard-port
handle can be replaced by an instrument; a connected controller cannot. The
native backend has four source slots total, including standard controllers.
This does not add direct Bluetooth pairing or a replacement Bluetooth driver.

Discovery results and class-decoding failures go to the existing asynchronous
`encore-test.log`. No raw input packets are recorded. If the guitar still does
not appear, send the build identity and this log. Look for `pad: native guitar discovery
revision=2`, `pad: RB4`, and `pad: guitar` lines. Discovery/open failures,
extended class availability and system interception are logged only on
transitions; no synchronous file writes are added to input polling. Test Options, all five frets,
both strum directions, solo frets and reconnect before reporting support as
confirmed. Special-port access/parameter acceptance remains a hardware check.

## RetroCultMods Wii adapters

A Wii guitar connected through an adapter is decoded according to the adapter's
USB output mode. Encore already supports the 27-byte PS3 Guitar Hero report
(12ba:0100) and Santroller guitar reports (1209:2882, revisions 0300 and 0400).
The PS3 preset now supplies fixed hardware menu buttons alongside gameplay
bindings, including Start and Select. Existing saved gameplay remaps retain
their bindings when this menu metadata is inherited.

PS3 GH frets are Green 0x02, Red 0x04, Yellow 0x01, Blue 0x08, Orange 0x10
in byte 0; Start/Select are byte 1 bits 0x02/0x01; byte 2 carries the hat.
Both neutral hat values 8 and 31 are accepted. Santroller mode has a distinct
layout and must not be decoded with these PS3 offsets. Wrong report lengths
and IDs remain rejected. Keyboard/Fortnite mode is a different protocol and
is not covered by this PS3 mapping. Individual adapter firmware still needs
physical testing; the product name alone is not proof of its output format.

## Verification

Run `bash ps5/tools/test-native-instruments.sh`. The suite exercises actual
platform code with a mock ScePad service: independent ports, all frets and solo
frets, strum transitions, interception, invalid class data, duplicate handles,
reconnection, sign-out and four native instruments. Revision 2 also covers
standard-port guitars, borrowed handles, extended-only class information,
second-product-field variants, delayed first samples, timeout cleanup and
disconnected placeholders beside four ordinary controllers. Profile tests exercise the
shipped PS3/Santroller maps and preserve independent USB queues under load.
AddressSanitizer and UndefinedBehaviorSanitizer are enabled.

## References

- [Harmonix PS4 instrument compatibility on PS5](https://www.harmonixmusic.com/blog/rock-band-4-and-next-gen-consoles)
- [OpenOrbis ScePad interface declarations](https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/blob/master/include/orbis/_types/pad.h)
- [AnyPS5 native interface structures](https://github.com/boykopovar/AnyPS5/blob/main/core/libs/SceTypes.hpp)
- [RetroCultMods V3 documentation](https://wiki.retrocultmods.com/adapters/v3-adapter/)
- [PlasticBand PS3 Guitar Hero reports](https://github.com/TheNathannator/PlasticBand/blob/main/Docs/Instruments/5-Fret%20Guitar/Guitar%20Hero/PS3.md)

Native interface declarations guide platform integration. The PC game's input
engine does not provide Sony controller transport code. No emulator runtime
implementation is incorporated into Encore.
