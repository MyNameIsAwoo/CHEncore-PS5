# Input transport and filtering checks

Gameplay input and menu filtering are separate. Menu selection/action edges use
a 220-ms guard; directional menu edges retain 75 ms and PC held-repeat timing.
The guitar engine's separate configurable debounce defaults to 5 ms and checks
repeated same-direction strums only. Opposite-direction strums remain eligible.
Those timers use input/audio time rather than frame counts.

Raw USB acquisition preserves both press and release reports between frames.
Known-device history now compares decoded instrument controls and menu hats;
packet counters and checksums alone do not create history entries. Latest raw
reports remain available for joystick navigation and the remap wizard. The
bounded history was expanded from 64 to 256 actual transitions per USB source.
Overflow is counted and written to the beta diagnostic log rather than hidden.

Host tests generated 100 alternating strums with two-millisecond pulses, while
render consumers ran at 4, 30, 60 and 120 FPS. All four cases judged exactly 100
hits, zero misses and no queue overflow. Counter-only packet changes were
ignored; real fret, strum and release transitions were retained. Additional
checks cover report order, overflow, flush/reset and audio-time clamping.

This establishes those synthetic software cases, not universal hardware latency
or freedom from switch chatter. USB polling and PS5 scePadReadState remain
platform input paths; an input not reported by the hardware/OS cannot be
reconstructed. Physical Xbox 360 receiver/controller testing remains unavailable.
