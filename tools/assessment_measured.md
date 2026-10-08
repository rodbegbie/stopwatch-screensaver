## Measured on the device

Twenty-seven hacks have been run so far (default settings, 466×466 canvas
pushed to the display every frame, canvas held in PSRAM). The firmware times
each frame in three parts, averaged over 5 seconds: **step** is the hack's own
draw call, **push** is sending the canvas to the display, and **wait** is what
is left of the delay the hack asked for once the push is credited against it
(the runner caps any delay at 10 seconds). Rows other than Rorschach and Pedal
were measured before the cap was raised from 1 second; Helix also asks for
5-second holds, so its frame rate will now be lower than shown.

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
| WhirlWindWarp | 17.0-24.8 | 6.2-26.4 ms | 31.3 ms | 0 ms | about 410 KB |
| Flame | 4.4-6.8 | 26-107 ms | 31.3-31.4 ms | 85-187 ms | none measurable |
| Hopalong | 19.2-25.2 | 7.3-20.1 ms | 31.2-31.3 ms | 0 ms | about 8 KB |
| Vines | 2.8-3.0 | 133-160 ms | 31.2 ms | 175-187 ms | none measurable |
| Sierpinski | 2.4-2.6 | 1.8-4.0 ms | 31.3-31.4 ms | 377-409 ms | about 32 KB |
| FadePlot | 28.0-28.2 | 2.9-3.2 ms | 31.3-31.4 ms | 0 ms | about 16 KB |
| Thornbird | 29.0-29.2 | 2.0-2.2 ms | 31.1-31.2 ms | 0 ms | about 11 KB |
| Spiral | 18.8 | 0.9-1.2 ms | 31.1 ms | 21 ms | about 5 KB |
| Sphere | 30.0-30.4 | 0.6-1.1 ms | 31.1-31.2 ms | 0 ms | none measurable |
| Discrete | 5.4-5.6 | 149-160 ms | 31.5 ms | 0 ms | about 16 KB |
| Galaxy | 9.4-11.2 | 58-75 ms | 31.1-31.3 ms | 0 ms | about 225-266 KB |
| Drift | 22.0-23.0 | 11-13 ms | 31.5 ms | 0 ms | about 16 KB |
| Lightning | 29.4 | 1.8-1.9 ms | 31.2 ms | 0 ms | none measurable |
| Maze | 6.0-28.8 | 0.1-1.3 ms | 31.2-33.4 ms | 0-256 ms | not measured |
| Blaster | 20.4-20.8 | 3.6-4.0 ms | 43.4-44.4 ms | 0 ms | not measured |
| Substrate | 12.4-20.6 | 2.9-43.9 ms | 41.1-44.4 ms | 0 ms | about 1.74 MB |

Maze's row is 26 five-second readings over 160 seconds, taken on a build that
includes the overlay stamping. Its steps are cheap, and its frame rate is set
by the delays it asks for between phases (a 10 ms solving step, then holds
before and after solving), so it is mostly waiting. Free heap held at
339,508 bytes, then 339,412 for the rest of the run, and free PSRAM at
7,290,919 bytes throughout. No stack canary, panic or reboot appeared. There
is no PSRAM baseline for this build yet, so its roughly 125 KB of state is
not split out, and the log does not show whether the allocator put it in PSRAM
or in internal heap.

Blaster's row is 32 five-second readings over 160 seconds. It is limited by
the push, not by the hack: its step is 4 ms and its wait is 0. Its push is
about 12 ms slower than the 31 ms of the earlier rows, and that has not been
investigated, so the cause is unknown. Free heap held between 338,352 and
338,440 bytes and free PSRAM at 7,424,155 throughout, and no stack canary,
panic or reboot appeared. There is no PSRAM baseline for this build, so its
extra PSRAM is not split out. This run used a build with the byte-order fix,
which was applied locally before it reached `main`.

Substrate is built in single precision (`hacks/substrate_single.c`), and its
row is 30 five-second readings of that build. It starts near 20 fps with a
step of about 3 ms. As the cracks and their sand paint build up, the step grows
over about 40 seconds and settles at 37-38 ms and 12.4-12.6 fps, with a wait
of 0, so from then on the frame is the hack's step plus the push.

The same hack in double precision settled at a step of 119-121 ms and 6.2 fps,
over a 31.8 minute capture (382 readings). That capture also showed one
restart, at about 25 minutes: the step fell to 26 ms and climbed back as the
new picture filled, and free PSRAM returned to the same value (only two
distinct readings in the whole run), so its two buffers were freed and
allocated again without a leak. The single-precision build has not been run
through a restart. Free PSRAM was 1,742,096 bytes lower than under HyperCube in
an earlier log, which matches the two image-sized buffers (2 x 466 x 466 x 4
bytes, plus a few KB), so plain `malloc` put both in PSRAM. Free heap stayed
between 336,228 and 339,524 bytes. No stack canary, panic or reboot appeared in
any run.

In an earlier capture Pyro and HyperCube pushed in 41-45 ms and ran at 20-23
fps, against 31 ms and 28-30 fps in their rows above. So push time is longer on
the current build for every hack, not just Substrate or Blaster. The cause has
not been tested (issue #23). The slower pushes began after the byte-order fix,
but other changes landed at the same time.

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
- Flame was the slowest hack at first: 76-592 ms a step (1.8-7.4 fps), because
  its source is all `double` with `sin`, `cos` and `sqrt` in a recursive
  per-point loop, and the ESP32-S3 has only a single-precision FPU. Building
  it in single precision (`hacks/flame_single.c`) cut the step to 26-107 ms.
  Its frame rate is now 4.4-6.8 fps and set by the hack's own pauses between
  pictures (85-187 ms of wait), not by compute. The scorer's `float-heavy`
  flag missed Flame, because it counts call sites in the source, not how often
  loops run it. A press made during a long step is kept and handled when the
  step ends (see Buttons below).
- WhirlWindWarp's step time sits on plateaus that change over time: about 6,
  8, 10, 19-20 and 25-26 ms across restarts, and it shifted within a single
  run (19.1, then 20.4, then 26.4 ms). It switches around 16 forcefields on
  and off at random, which fits, but which ones are expensive has not been
  identified.
- Free heap and PSRAM showed a single reading per hack across five restarts
  each of Flame and WhirlWindWarp, so repeated starts do not leak.
- The slowest of the xlockmore batch were Discrete (about 1 s a frame) and
  Drift (64 ms) until they were built in single precision, which brought them
  to about 150 ms and 11-13 ms. Vines and Sierpinski are slow because they ask
  for long delays (Sierpinski waits about 409 ms between cheap steps). Rod
  found Discrete's slow updates in keeping with other deliberately slow hacks,
  and saw nothing wrong with Discrete, Drift or Flame in single precision.
- Galaxy first ran at 5 fps (152-172 ms a frame): about 4,400 stars, each
  pulled by every galaxy in `double` maths, which the ESP32-S3 emulates in
  software. Building the unmodified source with `double` redefined as `float`
  (`hacks/galaxy_single.c`) cut the step to 38-42 ms and 14 fps with two
  galaxies, and the frames look the same on the host. A count override of 2
  leaked the star buffers on each restart, so it now uses `count: -3` (two or
  three galaxies, re-picked on each restart). With that it ran at 9.4-11.2 fps
  (58-75 ms a step) across two restarts' configurations, which is the cost of
  the third galaxy and the larger star counts.
- Thornbird keeps 400 buffers of 100 rectangles (about 320 KB) in internal
  heap, filled one per frame, so free heap falls to under 1 KB after about
  six seconds. It is bounded, not a leak: switching away returned the heap
  to its earlier value.
- Hacks that draw many primitives per frame are the ones to profile first.

## Other notes

Rorschach keeps a 9.6 KB array on the stack, which overflowed the Arduino
loop task's default 8 KB stack and rebooted the device on the first frame. The
firmware now sets a 16 KB loop stack (about 8 KB less free heap). Any hack with
large local arrays can hit the same limit.

Pyro's `init` builds two 6284-entry sine and cosine tables in double
precision, so restarting it dips to about 17 fps for a few seconds.

## Buttons

M5Unified reads the StopWatch buttons (GPIO2 for A and GPIO1 for B, active
low) only inside `M5.update()` and keeps no edge, so a press that began and
ended during one long hack step was never seen. The firmware now samples both
pins every 5 ms from a small task on core 0, and a latch counts a press once
the pin has held its new level for 30 ms. Repeated presses while a step runs
count as one, and holding a button does not repeat.

The serial switch line reports `press_waited`, the time from the press to the
switch. On the device it was usually 45-100 ms (the 30 ms settle plus one loop
period). Presses made during long steps waited for them: about 0.4 s on Flame
and 0.5-1.3 s around Pedal and Rorschach. About 90 switches, including bursts
of roughly four presses a second, showed no lost press, no reset and no
memory change.

A first version used GPIO interrupts and read the pin level when each ran.
The button bounced on release, the rising-edge interrupt saw the line low, and
the release was logged as a second press, so one click switched two hacks. The
polling design replaced it.

## Pedal

Pedal picks up to 1000 points per picture. The shim used to skip polygons
over 256 points, and the canvas dropped scanline crossings past 64, so half of
Pedal's pictures drew nothing. Both limits are gone. Sorting the crossings
with insertion sort then cost 2.6 million steps per picture on average and
21.6 million on the worst, which took about 4-5 seconds on the device; with
`qsort` above 16 crossings the step means are 126-519 ms. Pedal remains the
second most expensive hack to draw after Flame.
