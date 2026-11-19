# Controller calibration

Open your player’s Press Start card and choose **Calibrate Controller**, immediately below Remap Controller. Each player and instrument keeps a separate timing offset; named profiles save it, while Guest changes last for the session.

## Timing

Choose **Timing Test**, release the controls and follow the clicks after the countdown. Strum or press Green on a guitar, use the kick on a non-MIDI drum controller, or strike a mapped pad/kick on MIDI drums. Keep a DualSense available for MIDI menu navigation.

The test takes 16 responses, rejects isolated mis-taps, and shows the suggested offset and timing variation before applying anything. Select Save Player (or Apply to Guest), Retry, or Cancel. Inconsistent responses require another attempt. Cancelling, disconnecting or losing audio keeps the previous setting. Other players can keep using their own cards.

**Input Offset** also supports one-millisecond adjustments with Yellow/Blue or left/right. Positive values compensate for late input; negative values compensate for early input. Reset Timing affects only the selected player and instrument. Audio Offset remains the shared setting for the sound system; changing that later can require recalibration.

Offsets change the player’s hit and automatic-miss timeline, without changing hit-window width, song playback, highway visuals or other players. Bots and recorded engine tests bypass player offsets. Frame rates do not change the offset duration; practice speed scales it into chart time.

## MIDI velocity

Once a MIDI kit is connected and mapped, choose **MIDI Velocity** to repeat soft/hard calibration without remapping the kit. Strike each displayed pad softly, then firmly. Yellow skips a pad. Review and save when finished. One- and two-kick assignments stay intact; kicks never request velocity calibration.

Velocity and timing solve different problems: velocity sets the soft/hard response range, while timing compensates for consistent input delay. Hardware, speaker latency and the player’s consistency affect the timing result. USB MIDI hardware support remains experimental/intermittent.

## Verification

`bash ps5/tools/test-player-calibration.sh` runs exception-free C++20 sanitizer checks for four-player/instrument persistence, legacy profiles, timing samples and actual guitar/drum judgments at 30/60/120 FPS, duplicate responses, outliers, audio-clock resets, and MIDI velocity with preserved double kicks.

`bash ps5/tools/test-midi-drums.sh` covers MIDI decoding and drum judgments. Host tests use synthetic timestamped events and do not establish physical kit latency.
