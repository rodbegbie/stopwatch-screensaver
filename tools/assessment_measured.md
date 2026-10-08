## Measured on the device

Seven hacks have been run so far (default settings, 466×466 canvas pushed to
the display every frame, canvas held in PSRAM). The firmware times each frame
in three parts, averaged over 5 seconds: **step** is the hack's own draw call,
**push** is sending the canvas to the display, and **wait** is what is left of
the delay the hack asked for after step and push (the runner clamps the delay
to 1 second).

| Hack | fps | step | push | wait | Extra PSRAM |
| --- | --- | --- | --- | --- | --- |
| Pyro | 30.0 | 0.8-1.1 ms | 31.2 ms | 0 ms | about 80 KB |
| HyperCube | 28.3 | 2.7-2.9 ms | 31.5 ms | 0 ms | none measurable |
| Petri | 28.8-30.8 | 0.3-2.5 ms | 31.1 ms | 0 ms | about 1.3 MB |
| XSpirograph | 11.0-11.4 | 54-56 ms | 31.4 ms | 0-20 ms | none measurable |
| Helix | 16.6-23.6 | 0.9-2.1 ms | 31.3 ms | 9-26 ms | none measurable |
| Rorschach | 10.6 | 1.5 ms | 31.2 ms | 61 ms | none measurable |
| Pedal | 1.0 | 30-74 ms | 31.4 ms | 990-1250 ms | none measurable |

Free heap and free PSRAM return to exactly the same values every time a
hack is switched back to, so switching does not leak.

## What limits the frame rate

- Pushing the 434 KB canvas costs a steady 31 ms whatever the hack draws, so
  about 31 fps is the real ceiling today. The best hacks reach about 30.
- The firmware treats a hack's requested delay as the frame period and waits
  only for what is left after step and push. Before that change the delay was
  added on top, and Pyro, HyperCube and Petri ran at 22-23.5 fps, Helix at
  11.6-15, XSpirograph at 9.2 and Rorschach at 9.0.
- Helix and Rorschach draw in under 3 ms a frame. Their remaining low frame
  rates come from 20 ms delays plus a 1-second hold between pictures
  (Rorschach asks for 5 seconds, clamped to 1). Pedal likewise holds each
  picture for a second.
- XSpirograph is the only measured hack that is slow at drawing: 54-56 ms a
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
