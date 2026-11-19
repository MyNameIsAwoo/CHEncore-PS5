# Five-lane drum view — Alpha v.37

The dedicated view is an Encore extension. Ghidra inspection of GameAssembly.dll
at 0x18216D6E0 confirms the PC engine converts yellow toms/blue cymbals for
five-lane kit compatibility, choosing collision-safe masks per chord. That
conversion, session judgement, input mapping and scoring are preserved.
DrumsNeckController at 0x1800F48D0 maps source pad/cymbal masks to drum effects.

The view places red, yellow, blue, orange and green left to right. Yellow and
orange retain the extracted cymbal body/cap/animation states; ordinary pads
retain the extracted tom states. Green source layers supply orange shapes with
an orange tint. No image assets are created. Five distinct receptor/effect
positions share the existing highway transform and player camera, including
lefty reflection, tilt, length and multiplayer scaling. Kick bars retain the
full highway width. The existing four-lane and pro-drum view stays separate.

Five-lane profiles use muted purple kicks by default. An explicit orange/purple
choice overrides that default and round-trips through the optional
kick_color_explicit profile field. Older profile files remain readable.

Validation records are kept with the release. Scripted bot screenshots show
layout and rendering; they do not certify physical five-lane hardware.
