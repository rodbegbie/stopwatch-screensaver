## Measured on the device

Thirty-four hacks have been run so far (default settings, 466×466 canvas
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
| Pacman | 75.6-79.2 | 1.3-1.9 ms | 0.8-1.4 ms | 10.0-10.6 ms | about 580 KB |
| Braid | 3.2-8.6 | 95-296 ms | 19.5-22.7 ms | 0 ms | none measurable |
| Mountain | 44.0-45.2 | 0-0.4 ms | 0.1-0.4 ms | 21.9-22.1 ms | about 20 KB |
| Epicycle | 22.8-44.0 | 0.3-2.1 ms | 0.3-0.8 ms | 22.0-59.5 ms | none measurable |
| Kaleidescope | 27.8-34.4 | 7.0-9.8 ms | 17.7-25.5 ms | 0-3.7 ms | none measurable |
| Gears | 2.8-10.0 | 65-330 ms | 32.3-34.5 ms | 0 ms | 0.7-1.4 MB |
| Morph3D | 8.0-8.4 | 78-86 ms | 32.6 ms | 8.8 ms | not measured |
| Celtic | 1.0-20.4 | 29-1025 ms | 0.1-5.5 ms | 7.9-928 ms | none measurable |

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
rightmost pixel written in each row, and the planner pushes those spans, or one
full push when that is cheaper. M5GFX keeps a framebuffer for this panel, and
`endWrite` flushes one bounding box around everything written in the batch. So
rows are grouped into batches by a cost model fitted to device probes, and rows
far apart get their own: on Pyro, two corner pixels in one batch took 11.7 ms
and 0.1 ms in separate ones, and 20 scattered rows of 40 pixels took 1.8 ms
batched and 0.5 ms apart, while 20 adjacent rows took 0.2 ms batched. A first
version that put every row in one batch left Squiral at 9.3 ms for 20 rows
(thought at the time to be a floor between display updates; it was a flush box
spanning the screen). Steady readings on the device with the grouped batches
and the rotation off (previous push 31 ms and 28-30 fps):

| Hack | push | fps | rows per frame |
| --- | --- | --- | --- |
| Pyro | 1.1-1.5 ms | 83 | 56-74 |
| Squiral | 0.5 ms | 88 | 20-21 |
| Lightning | 4.3-4.5 ms | 64 | 85-91 |
| Maze | 0.5-4.3 ms | 6-29 | 24-76 |
| HyperCube | 12.1-13.4 ms | 55-59 | 287-305 |
| Blaster | 5.1-5.5 ms | 67 | 248-259 |
| CloudLife | 27.7 ms | 26.4 | 464 |
| Pedal | 19.6-33.9 ms | 0.2-0.4 | 348-495 |

Pedal's row is from the first version and was not repeated. Maze, Lightning and
Pyro are held back by their own delays, not the push, and the fps column shows
that for every hack that waits. Substrate's early frames pushed in 1.0-5.0 ms
(44 fps) and its later ones in 3.7-6.4 ms as its picture filled. CloudLife and
Pedal redraw nearly every row, so they gain little. The host estimates (a lower
bound, since they counted only pixels that changed colour) were too low for
hacks that redraw unchanged pixels. Pyro's step stayed at 0.7-0.9 ms and
Substrate's first readings at 2.6-3.0 ms, as before, so the marking costs
nothing visible on the hot path. Rod checked Maze, the name and fps labels and
several hacks on the screen with the first version; the grouped version needs
the same look.

Free heap and free PSRAM return to exactly the same values every time a
hack is switched back to, so switching does not leak.

## What limits the frame rate

- A full push of the 434 KB canvas costs a steady 31 ms, but the firmware now
  sends only the rows a hack drew (issue #6), so the ceiling depends on the
  hack: about 30 fps for hacks that redraw most of the screen, and 55-90 fps
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

## Wide lines, arcs and pixmaps

The shim now honours `line_width`, cap and join styles, draws `XDrawArc` and
partial `XFillArc`, and lets `XCopyArea` write into pixmaps made by
`XCreatePixmap`. Width 0 and 1 keep the old line code.

No registered hack draws a wide line here. A survey over 200 frames of each
found a largest width of 1 (XSpirograph and Blaster; their Retina branches do
not run), and Maze's width-2 call sits under `HAVE_JWXYZ`, which is not
defined. So nothing the device runs today uses the new code, and the checks
were that nothing else changed:

- Frame hashes of 26 of the 27 hacks (every one but Coral, which reads the
  clock) are identical before and after, taken at four points over 200 frames.
  Maze, Pyro, Hopalong, Galaxy, Blaster and Substrate also dumped identical
  raw frames.
- On the device, Maze pinned with the rotation off for 75 seconds on each
  build, plain `main` and this branch: 11 five-second windows each, with the
  same medians for fps (24.4), push (1.1 ms) and rows (32), and step medians of
  0.30 and 0.40 ms. The two captures started at different points in Maze's
  cycle, so single windows differ and the 0.1 ms is within the resolution of
  the log. No reboot, panic or stack canary appeared.
- Not measured: the new code on the device. Wide lines, arcs and the pixmap
  copy are covered by host tests only. Blaster, which fills arcs, was not
  re-measured on the device after the change. Pacman is the first port that
  needs them.

## Pacman

Pacman runs at 75.6-79.2 fps (second window onward, 46 windows of 5 seconds
pinned with the rotation off), paced by its own 10 ms delay: the step is
1.3-1.9 ms, the push 0.8-1.4 ms for 45-84 rows a frame, and the wait 10.0-10.6
ms. It takes about 580 KB of PSRAM (7,424,155 free before it starts, 6,842,931
after) and about 50 KB of internal heap (the scaled sprites are each under
4 KB, so `malloc` keeps them in internal RAM). Rod checked the colours on the
screen: cyan walls, a yellow Pacman, the four ghost colours.

Two recursions in the hack had to go. The level generator (`creatlevelblock`
and `nextstep`) recursed up to about 315 KB deep on the host and rebooted the
board on its first frame (stack canary, loop task, 16 KB). The build now
patches it out, so Pacman always plays the fixed level. The ghosts' route home
(`recur_back_track`) reaches 453 levels of 48 bytes from some cells, about
22 KB; the build rewrites it as a loop over a heap stack that visits cells in
the same order, pinned over 30,000 frames. With a temporary stack gauge in the
serial line (not committed), the loop task's lowest free stack was 5,092 bytes
of 16,384 and falling after three minutes with the recursion, and a steady
13,844 bytes over four minutes after the rewrite.

Upstream also leaks about 514 KB of pixmaps each time Pacman starts. The
runner now releases a stopped hack's pixmaps. Rotating every 5 seconds for 70
rotations (two Pacman visits), the hack after each visit read the same heap
and PSRAM (289,228 and 7,342,579).

Not measured: runs longer than four minutes pinned, and the stack gauge only
saw the loop task while ghosts followed routes home, which the host pin shows
they do but the log does not mark.

## Braid

Braid is too slow to be a good screensaver, and four rounds of work took it
from 0.8-2.0 fps to 3.2-8.6 fps. Rod judged the colours and the wide strands
right on the screen, and the colour spin too slow. Each braid lasts 100 frames
and the next is random, so the rate steps between plateaus (strand count and
line width set the cost). One light braid ran at 21.2 fps. Readings are second
windows of 5 seconds, pinned with the rotation off, at 150-180 seconds each.

Braid redraws every segment of the braid on every frame (up to about 7,500
`XDrawLine` calls) to spin the colours, and a random width of 1-7 pixels sends
most of them through the stroker.

| Build | First braid | Second | Third |
| --- | --- | --- | --- |
| As copied | 2.0 fps, 482 ms | 1.2 fps, 828 ms | 0.8 fps, 1,422 ms |
| Single-precision wrapper | 2.8 fps, 350 ms | 1.6 fps, 634 ms | 1.0 fps, 1,142 ms |
| Disc rows cached by width | 6.6 fps, 130 ms | 4.2 fps, 223 ms | 2.8 fps, 357 ms |
| Fast `sin` and `cos` | 7.0 fps, 123 ms | 4.4 fps, 212 ms | 2.8 fps, 342 ms |
| Literals made floats | 8.6 fps, 95 ms | 5.2 fps, 176 ms | 3.2 fps, 296 ms |

The step is the hack's draw call; the push stayed at 19.5-22.7 ms (377-405
rows a frame). What each change was worth, and how it was found:

- **Wide-line discs.** A width above 3 gets round caps, and
  `canvas_fill_ellipse` works every row out in `double` (a division, `sqrt`,
  `ceil`, `floor`), which the S3 does in software. A round-capped segment draws
  two discs. The host profile showed the ellipse at 6% of the time, because a
  Mac does `double` at full speed; on the board it was the largest cost, 2.7 to
  3.2 times. `stroke.c` now builds each width's rows once (widths to 16) and a
  test pins the pixels to `canvas_fill_ellipse`.
- **Literals.** `single_precision.h` renames the keyword `double`, but
  `0.5 * (1.0 + sinf(x)) * r2` and `t / theta * M_PI` were still software
  double: the object file called `__muldf3`, `__adddf3` and `__divdf3`. A probe
  build with
  `XDrawLine` returning at once showed the hack's own step was 45-90 ms,
  and swapping in a fast `sin` and `cos` alone moved it by about 5%. Rewriting
  the literals as floats at build time (`tools/float_literals.py`) removed the
  helpers and took the first braid from 123 to 95 ms. Frames stayed identical.
- **Still slow.** The rest is drawing: each wide segment is a quad filled a row
  at a time (a float division per edge per row, then a rectangle fill per row)
  plus two discs. A braid with many strands and width 7 still takes about 300
  ms. Braid's own `applywordbackto` runs inside the inner loop and cannot be
  changed, because the hack stays byte-identical. Reaching 10 fps would need the
  stroker's per-row cost cut by more than half, which has not been tried.

Free PSRAM was 7,417,507 bytes throughout, and free internal heap 325,196
bytes (326,220 before the disc table, which takes 1 KB of static memory). No
stack canary, panic or reboot appeared in any capture. Not measured: runs over
three minutes, and Braid's restart on the device beyond the three braids seen.

## Four ports that test the Speed bands

Mountain, Epicycle, Kaleidescope and Celtic were ported to test the host Speed
bands, which were fitted to the first 29 hacks. Predictions were committed
before any port (`docs/speed-predictions.md`, which also has the verdicts),
and each was flashed unmodified, pinned with the rotation off, for 180 seconds
(29-35 five-second windows, the first dropped).

- **Mountain** (low band, 0.0005 ms on the host): step median 0.1 ms, 0-0.4,
  paced by its own 20 ms delay at 44-45 fps; it idles at 0.0 ms between
  pictures. It takes about 20 KB of PSRAM (7,404,051 free against 7,424,155).
- **Epicycle** (low band, 0.0009 ms, 31 `double`s): step median 0.4 ms, 0.3-2.1,
  with two of 34 windows over 1.4 ms (1.5 and 2.1 ms; the other 32 are 0.3-0.8
  ms), probably the windows holding a restart. Free heap 327,220-327,612.
- **Kaleidescope** (high band, 0.0695 ms): step median 8.3 ms, 7.0-9.8, at
  27.8-34.4 fps with a 17.7-25.5 ms push. Free heap sat at 219,416-219,556 bytes,
  about 105 KB below an idle build, with PSRAM untouched.
- **Celtic** (high band, 1.12 ms, 42 `double`s, wide round-capped lines):
  step median 832 ms, 29-1,025, at about 1.2 fps. Its `assert()` calls
  `abort()` on a failed allocation, and a picture's memory depends on the
  pattern: the first held about 300 KB of the 325 KB of free internal heap
  (free heap read 25,684 bytes), the next two 232,436 and 247,516, and it
  returned to 328,556 between pictures. Nothing aborted, but the first picture
  left little margin. Three windows drew nothing (rows=0) yet took 844-1,025 ms
  a step, so much of the cost is computing, not drawing. Rod took it out of
  the rotation (issue #37); it is no longer in `g_hacks[]`.

None showed a stack canary, panic or reboot, and free PSRAM was constant
within each run (Mountain's 20 KB is its offset from the idle figure).
