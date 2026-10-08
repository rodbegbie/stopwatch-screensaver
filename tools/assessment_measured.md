## Measured on the device

Seven hacks have been run so far (default settings, 466×466 canvas pushed to
the display every frame, canvas held in PSRAM). The firmware times each frame
in three parts, averaged over 5 seconds: **step** is the hack's own draw call,
**push** is sending the canvas to the display, and **wait** is the delay the
hack asked for (the runner clamps it to 1 second).

| Hack | fps | step | push | wait | Extra PSRAM |
| --- | --- | --- | --- | --- | --- |
| Pyro | 23.5 | 0.4-1.5 ms | 31 ms | 11 ms | about 80 KB |
| HyperCube | 22.0 | 2.8 ms | 31.5 ms | 11 ms | none measurable |
| Petri | 23.5 | 0.1-0.4 ms | 31 ms | 11-12 ms | about 1.3 MB |
| XSpirograph | 9.2 | 56.5 ms | 31.4 ms | 22 ms | none measurable |
| Helix | 11.6-15 | 0.8-2.8 ms | 31.3 ms | 37-59 ms | none measurable |
| Rorschach | 9.0 | 1.3-1.7 ms | 31.1 ms | 95 ms | none measurable |
| Pedal | 1.0 | 17-56 ms | 31.3 ms | 1108-1389 ms | none measurable |

Free heap and free PSRAM return to exactly the same values every time a
hack is switched back to, so switching does not leak.

## What limits the frame rate

- Pushing the 434 KB canvas costs a steady 31 ms whatever the hack draws, so
  about 32 fps is the real ceiling today.
- Most hacks sit below it because of the delay they request. Pyro, HyperCube
  and Petri ask for 10 ms and the wait loop adds about 1.2 ms, which gives
  roughly 23 fps. This is their own pacing, not a hardware limit.
- Helix and Rorschach draw in under 3 ms a frame. Their low frame rates come
  from 20 ms delays plus a 1-second hold between pictures (Rorschach asks for
  5 seconds, clamped to 1). Pedal likewise holds each picture for a second.
- XSpirograph is the only measured hack that is slow at drawing: 56.5 ms a
  frame for 1000 lines. Whether that is the PSRAM pixel writes or its
  double-precision maths has not been separated.
- Hacks that draw many primitives per frame are the ones to profile first.

## Other notes

Rorschach keeps a 9.6 KB array on the stack, which overflowed the Arduino
loop task's default 8 KB stack and rebooted the device on the first frame. The
firmware now sets a 16 KB loop stack (about 8 KB less free heap). Any hack with
large local arrays can hit the same limit.

Pyro's `init` builds two 6284-entry sine and cosine tables in double
precision, so restarting it dips to about 17 fps for a few seconds.
