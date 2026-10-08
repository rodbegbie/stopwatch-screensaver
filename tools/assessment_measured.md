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
| Blaster | 27.8 | 3.6-3.7 ms | 31.2-31.3 ms | 0 ms | not measured |
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

Blaster's row is three steady five-second readings, taken after the canvas
moved to display byte order (issue #23; see below). It is limited by the push,
not by the hack: its step is under 4 ms and its wait is 0. Free heap held
between 337,988 and 338,372 bytes and free PSRAM at 7,424,155 throughout, and
no stack canary, panic or reboot appeared. Before that change its push took
43.4-44.4 ms and it ran at 20.4-20.8 fps. There is no PSRAM baseline for this
build, so its extra PSRAM is not split out.

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

For a while after the byte-order fix, every hack pushed in 41-45 ms and ran
at 20-23 fps where the rows above show 31 ms and 28-30 fps. Issue #23 found the
cause: with `setSwapBytes(true)`, M5GFX swapped the bytes of all 217,156 pixels
on every push, which cost 9.8 ms (Pyro pushed in 31.2 ms with the swap off and
41.0 ms with it on). The canvas now holds pixels in the display's byte order
and the swap is off. Steady readings on the device after that change, with the
rotation off:

| Hack | push | fps |
| --- | --- | --- |
| Pyro | 31.2 ms | 30.0 |
| HyperCube | 31.5 ms | 28.0 |
| Blaster | 31.2-31.3 ms | 27.8 |
| Substrate | 31.1-31.2 ms | 28.6-29.0 early on, 18.0 once its step grew to 24 ms |
| Maze | 31.1 ms | 8.0-10.6 (paced by its own delays) |

Substrate's capture was only 22 seconds, so its row above, which covers the
whole build-up, still shows the earlier push of 41.1-44.4 ms and the frame
rates that went with it. Its steps are unchanged. Rows measured before the
byte-order fix and not repeated are unaffected.

Pushing part of the canvas scales with its area. On Pyro, with the swap on,
the top half took 20.6 ms (of 41.0) and a quarter-size block pushed row by row
took 13.7 ms. Each separate `pushImage` call costs about 12 microseconds, so a
row-by-row push of the whole canvas took 46.8 ms and a row-by-row push of the
inscribed circle took 38.7 ms, against 41.0 ms for one call.

Issue #6 then sent only the rows a hack drew. The canvas keeps the leftmost and
rightmost pixel written in each row, and the firmware pushes those spans in one
`startWrite`, where a call costs about 5 microseconds instead of 16, or one
full push when the rows would cost more. Steady readings on the device, with
the rotation off (previous push 31 ms and 28-30 fps):

| Hack | push | fps | rows per frame |
| --- | --- | --- | --- |
| Pyro | 4.1-4.5 ms | 83 | 58-72 |
| Squiral | 9.3-10.3 ms | 87 | 20 |
| Lightning | 4.3-4.6 ms | 64 | 85-91 |
| Maze | 0.5-4.3 ms | 6-29 | 24-76 |
| HyperCube | 11.6-13.0 ms | 57-61 | 286-306 |
| Blaster | 13.5 ms | 54 | 253 |
| CloudLife | 27.7 ms | 26.6 | 464 |
| Pedal | 19.6-33.9 ms | 0.2-0.4 | 348-495 |

Maze, Lightning and Pyro are held back by their own delays, not the push.
Substrate's early frames pushed in 1.7-6.8 ms (45 fps) and its later ones in
6-10 ms. CloudLife and Pedal redraw nearly every row, so they gain little. The
host estimates (a lower bound, since they counted only pixels that changed
colour) were too low for hacks that redraw unchanged pixels: Blaster was
estimated at 3.3 ms and took 13.5. Squiral pushes about 20 rows but takes
10 ms each time; every push of 17-25 rows took 10.2-11.3 ms whatever the row
count, which looks like a floor between display updates when frames come back
to back. That has not been tested. Pyro's step stayed at 0.8 ms and
Substrate's first readings at 2.8-3.0 ms, as before, so the marking costs
nothing visible on the hot path. Rod checked Maze, the name and fps labels and
several hacks on the screen: correct colours, no stale pixels.

Free heap and free PSRAM return to exactly the same values every time a
hack is switched back to, so switching does not leak.

## What limits the frame rate

- A full push of the 434 KB canvas costs a steady 31 ms, but the firmware now
  sends only the rows a hack drew (issue #6), so the ceiling depends on the
  hack: about 30 fps for hacks that redraw most of the screen, and 60-90 fps
  for sparse ones (see the table above).
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
