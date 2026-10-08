## Measured on the device

Thirteen hacks have been run so far (default settings, 466×466 canvas pushed to
the display every frame, canvas held in PSRAM). The firmware times each frame
in three parts, averaged over 5 seconds: **step** is the hack's own draw call,
**push** is sending the canvas to the display, and **wait** is what is left of
the delay the hack asked for once the push is credited against it (the runner
caps any delay at 10 seconds). Rows other than Rorschach and Pedal were
measured before the cap was raised from 1 second; Helix also asks for 5-second
holds, so its frame rate will now be lower than shown.

| Hack | fps | step | push | wait | Extra PSRAM |
| --- | --- | --- | --- | --- | --- |
| Pyro | 30.0 | 0.8-1.1 ms | 31.2 ms | 0 ms | about 80 KB |
| HyperCube | 28.3 | 2.7-2.9 ms | 31.5 ms | 0 ms | none measurable |
| Petri | 28.8-30.8 | 0.3-2.5 ms | 31.1 ms | 0 ms | about 1.3 MB |
| XSpirograph | 11.0-11.4 | 54-56 ms | 31.4 ms | 0-20 ms | none measurable |
| Helix | 16.6-23.6 | 0.9-2.1 ms | 31.3 ms | 9-26 ms | none measurable |
| Rorschach | 3.0 | 0.6-1.7 ms | 31.1-31.2 ms | 367 ms | none measurable |
| Pedal | 0.2-0.4 | 126-519 ms | 31.4-31.5 ms | 3290-5507 ms | none measurable |
| Coral | 15.0-21.2 | 15-25 ms | 31.2 ms | 0-10 ms | about 240 KB |
| Squiral | 30.6-31.0 | 0.1-0.3 ms | 31.1 ms | 0 ms | about 850 KB |
| Critical | 30.4 | 0.7 ms | 31.1 ms | 0 ms | about 15 KB |
| CloudLife | 24.2 | 8.8-8.9 ms | 31.4 ms | 0 ms | about 260 KB |
| WhirlWindWarp | 17.4-25.8 | 6.2-25.2 ms | 31.3 ms | 0 ms | about 410 KB |
| Flame | 3.4-7.4 | 78-271 ms | 31.4 ms | 0-70 ms | none measurable |

Free heap and free PSRAM return to exactly the same values every time a
hack is switched back to, so switching does not leak.

## What limits the frame rate

- Pushing the 434 KB canvas costs a steady 31 ms whatever the hack draws, so
  about 31 fps is the real ceiling today. The best hacks reach about 30.
- A hack's delay is the pause after it draws, and the firmware credits only
  the 31 ms push against it. Hacks that ask for 10-20 ms therefore run at the
  push ceiling. Before the push was credited the delay was added on top, and
  Pyro, HyperCube and Petri ran at 22-23.5 fps. Crediting the whole step as
  well was tried and dropped: a hack asking for a hold got none once its own
  draw took that long, so slow Pedal pictures were erased as soon as they
  appeared.
- Rorschach and Pedal draw a picture and then hold it for the 5 seconds they
  ask for, so their frame rates (3.0 and 0.2-0.4) measure the holds, not the
  speed. Rorschach draws a picture in about 15 frames over half a second.
- XSpirograph is the slowest at drawing: 54-56 ms a frame for 1000 lines.
  Whether that is the PSRAM pixel writes or its double-precision maths has
  not been separated. Coral (15-25 ms), CloudLife (about 9 ms) and
  WhirlWindWarp (about 6 ms) are the next most expensive; Squiral and
  Critical draw in under a millisecond and run at the push ceiling.
- Coral's step time falls as the picture fills in (15.0 fps, then 21.2).
- Flame is the slowest hack so far, at 78-271 ms a frame, and it varies from
  picture to picture. Its source is all `double`, with `sin`, `cos` and
  `sqrt` in a recursive per-point loop, and the ESP32-S3 has only a
  single-precision FPU. Double-precision maths in software is the likely
  cause, but it has not been profiled. The scorer's `float-heavy` flag missed
  it, because it counts call sites in the source, not how often loops run it.
- WhirlWindWarp ran at 6.2-6.4 ms a frame in one session and a steady 25.2 ms
  in another. It switches around 16 forcefields on and off at random, so cost
  may depend on which are active, but that has not been confirmed.
- Hacks that draw many primitives per frame are the ones to profile first.

## Other notes

Rorschach keeps a 9.6 KB array on the stack, which overflowed the Arduino
loop task's default 8 KB stack and rebooted the device on the first frame. The
firmware now sets a 16 KB loop stack (about 8 KB less free heap). Any hack with
large local arrays can hit the same limit.

Pedal picks up to 1000 points per picture. The shim used to skip polygons
over 256 points, and the canvas dropped scanline crossings past 64, so half of
Pedal's pictures drew nothing. Both limits are gone. Sorting the crossings
with insertion sort then cost 2.6 million steps per picture on average and
21.6 million on the worst, which took about 4-5 seconds on the device; with
`qsort` above 16 crossings the step means are 126-519 ms. Pedal remains the
second most expensive hack to draw after Flame.

Pyro's `init` builds two 6284-entry sine and cosine tables in double
precision, so restarting it dips to about 17 fps for a few seconds.
