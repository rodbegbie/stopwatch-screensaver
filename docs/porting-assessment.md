# Porting assessment

Rough effort to port each xscreensaver hack to the StopWatch X11 shim,
generated from xscreensaver 6.16 by `tools/score_hacks.py`. Regenerate
it with `uv run tools/score_hacks.py`.

## How to read this

- **283 hacks** were scanned: every `hacks/*.c` and `hacks/glx/*.c`
  that registers itself with `XSCREENSAVER_MODULE`. Another 125
  files (3D models, helper libraries, command-line tools) are excluded and
  listed at the end.
- The scan is static and heuristic. For each 2D hack it counts the Xlib
  calls in the source that the shim does not declare, and it also
  syntax-checks the unmodified source against the shim's headers
  (`cc -fsyntax-only`). Anything the compiler cannot find is a **shim
  gap**: a missing header, type, struct field, constant or function.
  That second check catches helpers hacks reach through `utils/` that a
  count of Xlib calls cannot see. The effort rating does not run anything,
  so treat it as a prioritisation aid, not an estimate. It also says nothing
  about speed: see "Speed" below.
- The **All hacks** table lists the work still to do first, from least to
  most effort (S, M, L, XL) and then by name, and the ported hacks last.
  Its **Ported** column shows ✅ for a hack in the
  firmware's registry (`hacks/registry.c`) and ❌ for one that was
  attempted and abandoned (see "Failed ports" after the table, which only
  appears when there are any). Failures are recorded in
  `tools/failed_ports.txt`.

## Effort ratings

| Rating | Meaning | Count |
| --- | --- | --- |
| S | 2D, and the unmodified source compiles against the shim | 18 |
| M | 2D, 1-4 shim gaps, no pixmaps or pixel read-back | 13 |
| L | 2D, 5+ shim gaps, or uses pixmaps or pixel read-back | 79 |
| XL | GL: runs on the TinyGL layer, each hack needs its own GL calls and helpers checked (see below) | 137 |
| Ported | Already running on the device, so no rating | 36 |

## Speed

Effort says what a hack takes to compile, not how fast it runs. Braid was
rated S and runs at 3-9 fps, because it draws about 7,500 wide lines a frame,
which nothing in its source says. So each unported S hack is also built with
the shim and run on the host for 300 steps (`tools/probe_hacks.py`), and the
**Speed** column gives its band and host milliseconds per step. A hack rated
M or higher has gaps, which are compile errors, so it cannot be run until the
shim fills them and its Speed is a dash.

- **low**: under 0.015 ms.
- **medium**: 0.015 to 0.05 ms.
- **high**: 0.05 ms or more.
- **does not link**, **does not compile**, **crashed on the host**, **timed
  out**: an S hack that could not be probed.

A ported hack shows the step measured on the device instead.

The host time ranks the device step well (a Spearman correlation of about 0.9
over the 29 hacks the bands were fitted to, and all seven of those with a
device step of 50 ms or more are in the high band) but it is not a prediction
in milliseconds: the device took 60 to 1,500 times as long. The ratio is
highest for hacks that still do software double-precision maths, so a low band
does not clear a hack that does a lot of `double` arithmetic. Those figures
are in-sample. Four hacks ported afterwards, with their predictions committed
first, all fell where predicted: two low-band hacks under 1 ms, a high-band
hack at 8 ms and a heavy one at 830 ms
([speed-predictions.md](speed-predictions.md)). See
[speed-backtest.md](speed-backtest.md) for the table and its limits.

## Flags

- `pixmaps`: uses `XCreatePixmap` or `XCopyArea`. Needs off-screen
  surfaces, which cost PSRAM (466×466 at 16 bits is about 434 KB each).
- `readback`: uses `XGetImage` or `XGetPixel`. Needs pixel read-back, and
  is often slow.
- `xor`: uses `XSetFunction`, so it needs XOR drawing.
- `text`: draws text, so it needs a font path.
- `clipmask`: uses clip masks.
- `float-heavy`: 20 or more `sin`, `cos`, `sqrt` or `pow` calls. The
  ESP32-S3 has a single-precision FPU only, so double-precision maths is
  slow in software. Expect these to need profiling.

About 40 hacks are built on the `xlockmore.h` framework. The shim provides it
(`firmware/src/xs_support/xlockmore.c`), so it is not a flag: it costs nothing
to port.

## GL hacks

The XL hacks use fixed-function OpenGL. They run on the GL layer in
`firmware/src/glshim/`, a thin shim over TinyGL (a software rasteriser) that
draws straight into the canvas. Gears is the first GL hack ported; the
speed and ideas are in issues #43 to #46. An unported XL hack still needs its
own checking: the GL calls it makes may not all be in TinyGL, and the shim
does not yet provide every GL helper under `xs_support/glx/`.

## Measured on the device

Thirty-five hacks have been run so far (default settings, 466×466 canvas
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
| Morph3D | 8.0-11.4 | 48-86 ms | 32.6-36.2 ms | 5.2-8.8 ms | 0 (PSRAM identical every visit) |
| Morph3D (dirty rectangle, `*delay: 10000`) | 14.4-15.2 | 54-58 ms | 2.0-9.2 ms | 2.6-9.4 ms | not re-measured |
| Gears (dirty rectangle, first layout after boot) | 10.6-13.0 | 44-63 ms | 4.4-12.1 ms | 20-29 ms | not re-measured |
| Gears (dirty rectangle, `*delay: 10000`, first layout after boot) | 12.6-17.6 | 46-65 ms | 5.5-13.5 ms | 0-5.8 ms | not re-measured |
| Morph3D, five shapes (final code, overnight, 23 starts) | 11.6-31.2 | 20-75 ms | 6.3-9.1 ms | 2.6-5.2 ms | 0 (PSRAM identical every visit) |
| CubicGrid (final code, overnight, 37 windows) | 11.2-14.0 | 38-53 ms | 32.1-36.1 ms | 0 ms | 0 (PSRAM identical every visit) |
| Gears (final code, overnight, 29 windows, many layouts) | 4.0-19.8 | 39-241 ms | 6.1-23.7 ms | 0-5.2 ms | 0 (PSRAM identical; heap falls 388 B a lap, its known leak) |
| CubicGrid (ticks 30, 27,000 points; not registered) | 5.6-6.0 | 137-148 ms | 32.3-36.0 ms | 0 ms | 1.66 MB more than Morph3D (display list) |
| CubicGrid (ticks 20, 8,000 points; registered) | 11.2-12.4 | 48-52 ms | 32.2-36.1 ms | 0 ms | 0.49 MB more than Morph3D (display list) |
| Celtic | 1.0-20.4 | 29-1025 ms | 0.1-5.5 ms | 7.9-928 ms | none measurable |
| Deluxe (opaque, arcs as polylines; first port) | 2.0-5.4 | 154-510 ms | 32.6 ms | 0 ms | not compared with idle (flat at 7,327,227) |
| Deluxe (opaque, circles as rings) | 13.2-15.4 | 31.5-42.1 ms | 32.5-35.1 ms | 0 ms | not compared with idle (flat at 7,327,227) |
| Deluxe | 10.4-12.8 | 44.6-62.6 ms | 32.5-32.6 ms | 0 ms | not compared with idle (flat at 7,327,227) |

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

## Deluxe

Deluxe was rated L (`GCPlaneMask`, `allocate_alpha_colors`, pixmaps) and
sat in the high Speed band. Its source is unchanged. `deluxe_opaque.c`
includes it with `*transparent` and `*doubleBuffer` off (plane masks and
pixmap double buffering cannot work on this canvas) and routes full wide
circles to `stroke_circle`. Each build was flashed pinned with the rotation
off and logged for 45 seconds (nine five-second windows, the first read
high because of the name overlay).

- As first ported (opaque, arcs through the polyline stroker) it ran at
  2.0-5.4 fps. A host profile put 94% of a frame in the 50 pixel wide
  circles: 400 frames took 1.14 s, 0.07 s without `XDrawArc`, and 0.81 s with
  the joins skipped. An arc is about a thousand wide segments, each filled a
  row at a time, so each pixel was painted about 25 times.
- `stroke_circle` fills the ring in two spans a row: 400 frames took 0.12 s
  on the host, and the board ran 13.2-15.4 fps, paced by the 32.5 ms push.
  The polyline path also leaves gaps in a ring, which the ring does not.
- Translucency is a per-GC `alpha` (26 of 32, the weight upstream's
  translucent path uses) that `stroke.c` blends over the canvas. Blending each
  piece of a polyline separately blended the overlaps twice, so a star's
  corners showed as bright diamonds (11.2-13.2 fps). Drawing a translucent
  shape into a one-bit-per-pixel mask and blending it once removed them, and
  costs 3-5 ms a frame on the board (10.4-12.8 fps) though nothing on the
  host. That is the registered build.
- Not measured: PSRAM against an idle build (it was flat at 7,327,227 in every
  capture, so the 27 KB mask per shape is returned), a restart or lap soak
  with rotation on, and a long run. Heap held at 322,648-323,056 bytes.
  Colour was judged by Rod on the screen.

## Suggested order for shim stage 2

Shim gaps across 2D hacks, ranked so gaps that block hacks
needing few additions come first.

| Gap | Score | 2D hacks needing it |
| --- | --- | --- |
| `XSetGraphicsExposures` | 3.51 | 8 |
| `make_color_loop` | 3.32 | 6 |
| `XSetWindowBackground` | 2.75 | 14 |
| `XImage` | 2.54 | 39 |
| `XDestroyImage` | 2.21 | 33 |
| `rgb_to_hsv` | 2.14 | 9 |
| `ZPixmap` | 1.98 | 35 |
| `GXxor` | 1.96 | 8 |
| `XQueryColor` | 1.87 | 14 |
| `make_color_ramp` | 1.77 | 8 |

## All hacks

| Hack | Kind | Effort | Speed | Ported | Shim gaps | Flags | LOC |
| --- | --- | --- | --- | --- | --- | --- | --- |
| anemone | 2d | S | high (2 ms) | - | - | pixmaps | 458 |
| anemotaxis | 2d | S | high (1.6 ms) | - | - | pixmaps | 760 |
| celtic | 2d | S | high (1.2 ms) | ❌ | - | - | 1141 |
| compass | 2d | S | high (1.7 ms) | - | - | pixmaps, float-heavy | 999 |
| euler2d | 2d | S | high (0.29 ms) | - | - | float-heavy | 893 |
| forest | 2d | S | high (0.15 ms) | - | - | - | 241 |
| fuzzyflakes | 2d | S | high (1.8 ms) | - | - | pixmaps | 655 |
| grav | 2d | S | low (0.0039 ms) | - | - | - | 360 |
| halftone | 2d | S | high (2.1 ms) | - | - | pixmaps | 413 |
| ifs | 2d | S | high (0.35 ms) | - | - | pixmaps | 560 |
| interaggregate | 2d | S | high (0.66 ms) | - | - | - | 989 |
| laser | 2d | S | high (0.14 ms) | - | - | - | 356 |
| lissie | 2d | S | low (0.0042 ms) | - | - | - | 323 |
| lmorph | 2d | S | high (0.28 ms) | - | - | float-heavy | 580 |
| rotor | 2d | S | low (0.0011 ms) | - | - | - | 394 |
| scooter | 2d | S | high (0.19 ms) | - | - | - | 975 |
| truchet | 2d | S | high (3.1 ms) | - | - | pixmaps | 541 |
| wormhole | 2d | S | high (1.6 ms) | - | - | pixmaps | 734 |
| abstractile | 2d | M | - | - | `BlackPixelOfScreen`, `make_color_loop`, `make_color_ramp`, `rgb_to_hsv` | - | 1625 |
| bouboule | 2d | M | - | - | `GXor`, `XSetFunction` | xor | 860 |
| cwaves | 2d | M | - | - | `BlackPixelOfScreen` | - | 219 |
| cynosure | 2d | M | - | - | `XCreateBitmapFromData`, `XSetWindowBackground`, `rgb_to_hsv` | - | 457 |
| hexadrop | 2d | M | - | - | `XSetWindowBackground` | - | 446 |
| hyperball | 2d | M | - | - | `UnmapNotify` | - | 2464 |
| lisa | 2d | M | - | - | `XMaxRequestSize` | - | 744 |
| munch | 2d | M | - | - | `GXxor`, `XSetFunction`, `i_log2`, `pow2.h` | xor | 462 |
| penrose | 2d | M | - | - | `LineOnOffDash` | - | 1342 |
| triangle | 2d | M | - | - | `free_colors`, `make_smooth_colormap` | - | 355 |
| vermiculate | 2d | M | - | - | `XSetWindowBackground`, `ya_random` | - | 1229 |
| worm | 2d | M | - | - | `GXor`, `XClearArea`, `XSetFunction` | xor | 434 |
| xrayswarm | 2d | M | - | - | `XSetGraphicsExposures`, `initTime` | - | 1235 |
| ant | 2d | L | - | - | `CoordModePrevious`, `NUMSTIPPLES`, `XCreatePixmapFromBitmapData`, `automata.h`, `hexagonUnit`, `triangleUnit` | - | 1351 |
| apollonian | 2d | L | - | - | `FcChar8`, `XColor.color`, `XGlyphInfo`, `XQueryColor`, `XftColor`, `XftColorAllocName`, `XftDraw`, `XftDrawCreate`, `XftDrawDestroy`, `XftDrawStringUtf8`, `XftFont`, `XftFontClose`, `XftTextExtentsUtf8`, `error: expected expression`, `load_xft_font_retry`, `overall` | - | 820 |
| apple2-main | 2d | L | - | - | `A2CONTROLLER_DONE`, `A2CONTROLLER_FREE`, `A2_GR_FULL`, `A2_GR_HIRES`, `A2_GR_LORES`, `ANALOGTV_DEFAULTS`, `ANALOGTV_OPTIONS`, `DisplayOfScreen`, `GrayScale`, `KeyPress`, `PseudoColor`, `TTY_BLINK`, `TTY_BOLD`, `TTY_INVERSE`, `TTY_SYMBOLS`, `XDestroyImage`, `XEvent.xkey`, `XGetImage`, `XGetPixel`, `XImage`, `XQueryColors`, `ZPixmap`, `a2_clear_gr`, `a2_clear_hgr`, `a2_cls`, `a2_display_image_loading`, `a2_goto`, `a2_hline`, `a2_hplot`, `a2_invalidate`, `a2_plot`, `a2_printc`, `a2_printc_noscroll`, `a2_prints`, `analogtv_reconfigure`, `ansi-tty.h`, `ansi_tty`, `ansi_tty_free`, `ansi_tty_init`, `ansi_tty_print`, `apple2.h`, `apple2_one_frame`, `apple2_sim_t`, `apple2_start`, `apple2_state_t`, `error: expected expression`, `error: incompatible integer to pointer conversion initializing 'Display *' (aka 'struct XshimDisplay *') with an expression of type 'int' [-Wint-conversion]`, `error: incompatible integer to pointer conversion initializing 'char *' with an expression of type 'int' [-Wint-conversion]`, `flag`, `image`, `load_image_async`, `sim`, `st`, `tc`, `text_data`, `textclient.h`, `textclient_close`, `textclient_getc`, `textclient_open`, `textclient_putc_event`, `textclient_puts`, `textclient_reshape`, `tty`, `tty_char`, `tty_flag`, `utf8_encode`, `utf8_to_latin1`, `utf8wc.h`, `visual_cells`, `visual_class`, `visual_rgb_masks` | pixmaps, readback | 1642 |
| attraction | 2d | L | - | - | `compute_closed_spline`, `error: incompatible integer to pointer conversion assigning to 'int *' from 'int' [-Wint-conversion]`, `error: member reference base type 'int' is not a structure or union`, `error: type name does not allow function specifier to be specified`, `error: type specifier missing, defaults to 'int'; ISO C99 and later do not support implicit int [-Wimplicit-int]`, `free_spline`, `make_color_ramp`, `make_spline`, `spline` | - | 1115 |
| barcode | 2d | L | - | - | `LSBFirst`, `XCreateImage`, `XDestroyImage`, `XImage`, `XPutImage`, `XYBitmap` | - | 2055 |
| binaryhorizon | 2d | L | - | - | `KeyPress`, `XDestroyImage`, `XGetImage`, `XImage`, `XPutImage`, `XPutPixel`, `ZPixmap`, `visual_depth` | pixmaps, readback | 624 |
| binaryring | 2d | L | - | - | `KeyPress`, `XDestroyImage`, `XGetImage`, `XImage`, `XPutImage`, `XPutPixel`, `ZPixmap`, `visual_depth` | pixmaps, readback | 577 |
| blitspin | 2d | L | - | - | `GXand`, `GXclear`, `GXor`, `GXset`, `GXxor`, `XDestroyImage`, `XDisplayHeight`, `XDisplayWidth`, `XGetImage`, `XPutImage`, `XScreenNumberOfScreen`, `async_load_state`, `file_to_pixmap`, `images/gen/som_png.h`, `load_image_async_simple`, `pow2.h`, `som_png`, `to_pow2` | pixmaps, readback, clipmask | 467 |
| boxfit | 2d | L | - | - | `XDestroyImage`, `XGetImage`, `XGetPixel`, `XImage`, `XSetWindowBackground`, `ZPixmap`, `async_load_state`, `load_image_async_simple` | pixmaps, readback | 573 |
| bsod | 2d | L | - | - | `A2CONTROLLER_DONE`, `A2CONTROLLER_FREE`, `A2_GR_FULL`, `A2_GR_HIRES`, `A2_GR_LORES`, `ANALOGTV_DEFAULTS`, `ANALOGTV_OPTIONS`, `FcChar8`, `XClearArea`, `XColor.color`, `XCreateImage`, `XCreatePixmapFromBitmapData`, `XDestroyImage`, `XFetchName`, `XGetImage`, `XGetPixel`, `XGlyphInfo`, `XImage`, `XPutImage`, `XPutPixel`, `XQueryColor`, `XSetPlaneMask`, `XSetWindowBackground`, `XStoreName`, `XYPixmap`, `XftColor`, `XftDraw`, `XftDrawCreate`, `XftDrawDestroy`, `XftDrawStringUtf8`, `XftFont`, `XftFontClose`, `XftTextExtentsUtf8`, `XftTextExtentsUtf8_multi`, `ZPixmap`, `a2_cls`, `a2_goto`, `a2_init_memory_active`, `a2_invalidate`, `a2_poke`, `a2_printc`, `a2_printc_noscroll`, `a2_prints`, `amiga_png`, `android_png`, `apple2.h`, `apple2_one_frame`, `apple2_sim_t`, `apple2_start`, `apple2_state_t`, `apple_png`, `async_load_state`, `atari_png`, `atm_png`, `dvd_png`, `em`, `error: expected expression`, `font`, `gnome1_png`, `gnome2_png`, `hmac_png`, `i1`, `i2`, `images/gen/amiga_png.h`, `images/gen/android_png.h`, `images/gen/apple_png.h`, `images/gen/atari_png.h`, `images/gen/atm_png.h`, `images/gen/dvd_png.h`, `images/gen/gnome1_png.h`, `images/gen/gnome2_png.h`, `images/gen/hmac_png.h`, `images/gen/mac_png.h`, `images/gen/macbomb_png.h`, `images/gen/osx_10_2_png.h`, `images/gen/osx_10_3_png.h`, `images/gen/ransomware_png.h`, `images/gen/sun_png.h`, `load_image_async_simple`, `load_xft_font_retry`, `mac_png`, `macbomb_png`, `osx_10_2_png`, `osx_10_3_png`, `ov`, `ov2`, `ransomware_png`, `screen_number`, `sim`, `st`, `sun_png`, `utf8_decode_combining`, `utf8wc.h`, `xft.h`, `xft_word_wrap`, `xftwrap.h` | pixmaps, readback, clipmask | 7809 |
| bubbles | 2d | L | - | - | `BUBBLE_MAGIC`, `Bubble`, `Bubble_Step`, `DELETE_BUBBLE`, `KEEP_BUBBLE`, `MAX`, `MAX_DROPPAGE`, `MIN`, `bubbles.h`, `default_bubbles`, `error: expected expression`, `head`, `init_default_bubbles`, `least`, `newpix`, `nextbub`, `num_default_bubbles`, `pixmap_list`, `rv`, `tmp`, `tmppix`, `touch` | pixmaps, clipmask | 1468 |
| bumps | 2d | L | - | - | `XDestroyImage`, `XGetImage`, `XGetPixel`, `XImage`, `XQueryColors`, `XSetWindowBackground`, `XShmSegmentInfo`, `ZPixmap`, `async_load_state`, `create_xshm_image`, `destroy_xshm_image`, `load_image_async_simple`, `pScreenImage`, `put_xshm_image`, `xshm.h` | pixmaps, readback | 705 |
| ccurve | 2d | L | - | - | `make_color_loop` | pixmaps | 872 |
| crystal | 2d | L | - | - | `GXxor`, `XCreateColormap`, `XFreeColormap`, `XInstallColormap`, `XSetFunction`, `XSetWindowColormap`, `free_colors`, `has_writable_cells`, `make_random_colormap`, `make_smooth_colormap`, `make_uniform_colormap`, `rotate_colors` | xor, float-heavy | 1286 |
| decayscreen | 2d | L | - | - | `async_load_state`, `load_image_async_simple` | pixmaps | 392 |
| deco | 2d | L | - | - | `DisplayOfScreen`, `XStoreColors`, `allocate_writable_colors`, `error: incompatible integer to pointer conversion initializing 'Display *' (aka 'struct XshimDisplay *') with an expression of type 'int' [-Wint-conversion]`, `has_writable_cells` | - | 345 |
| demon | 2d | L | - | - | `CoordModePrevious`, `GCFillStyle`, `GCStipple`, `NUMSTIPPLES`, `STIPPLESIZE`, `XCreatePixmapFromBitmapData`, `XGCValues.fill_style`, `XGCValues.stipple`, `automata.h`, `hexagonUnit`, `stipples`, `triangleUnit` | - | 953 |
| distort | 2d | L | - | - | `BlackPixelOfScreen`, `XDestroyImage`, `XGetImage`, `XGetPixel`, `XImage`, `XPutImage`, `XPutPixel`, `XShmSegmentInfo`, `ZPixmap`, `async_load_state`, `create_xshm_image`, `destroy_xshm_image`, `load_image_async_simple`, `put_xshm_image`, `xshm.h` | pixmaps, readback | 894 |
| droste | 2d | L | - | - | `BlackPixelOfScreen`, `GET_PARENT_OBJ`, `KeyPress`, `KeySym`, `THREAD_DEFAULTS`, `THREAD_OPTIONS`, `XDestroyImage`, `XEvent.xkey`, `XGetImage`, `XGetPixel`, `XImage`, `XK_Down`, `XK_Left`, `XK_Right`, `XK_Up`, `XLookupString`, `XPutPixel`, `XShmSegmentInfo`, `ZPixmap`, `async_load_state`, `create_xshm_image`, `destroy_xshm_image`, `double_time`, `doubletime.h`, `error: expected expression`, `error: field has incomplete type 'struct threadpool'`, `error: variable has incomplete type 'const struct threadpool_class'`, `hardware_concurrency`, `i_log2_fast`, `keysym`, `load_image_async_simple`, `pow2.h`, `put_xshm_image`, `thread_util.h`, `threadpool`, `threadpool_create`, `threadpool_destroy`, `threadpool_run`, `threadpool_wait`, `xshm.h` | pixmaps, readback | 686 |
| eruption | 2d | L | - | - | `XImage`, `XPutPixel`, `XSetWindowBackground`, `XShmSegmentInfo`, `ZPixmap`, `create_xshm_image`, `destroy_xshm_image`, `error: typedef redefinition with different types ('unsigned long' vs 'unsigned int')`, `img`, `put_xshm_image`, `xshm.h` | - | 608 |
| fiberlamp | 2d | L | - | - | `RootWindow`, `XAllocNamedColor`, `XSetGraphicsExposures`, `XTranslateCoordinates` | pixmaps | 480 |
| filmleader | 2d | L | - | - | `ANALOGTV_DEFAULTS`, `ANALOGTV_OPTIONS`, `ANALOGTV_SIGNAL_LEN`, `FcChar8`, `KeyPress`, `KeySym`, `XCreateImage`, `XDestroyImage`, `XEvent.xkey`, `XGetImage`, `XGetPixel`, `XGlyphInfo`, `XImage`, `XLookupString`, `XPutImage`, `XPutPixel`, `XftColor`, `XftColorAllocName`, `XftColorFree`, `XftDraw`, `XftDrawCreate`, `XftDrawDestroy`, `XftDrawStringUtf8`, `XftFont`, `XftTextExtentsUtf8`, `ZPixmap`, `analogtv`, `analogtv.h`, `analogtv_allocate`, `analogtv_draw`, `analogtv_input`, `analogtv_input_allocate`, `analogtv_load_ximage`, `analogtv_reception`, `analogtv_reception_update`, `analogtv_reconfigure`, `analogtv_release`, `analogtv_set_defaults`, `analogtv_setup_sync`, `double_time`, `doubletime.h`, `error: expected expression`, `extents`, `img`, `img1`, `img2`, `keysym`, `load_xft_font_retry`, `screen_number`, `xftfont` | pixmaps, readback | 548 |
| fireworkx | 2d | L | - | - | `ImageByteOrder`, `MSBFirst`, `XCreateImage`, `XDestroyImage`, `XImage`, `XPutImage`, `ZPixmap` | - | 882 |
| flag | 2d | L | - | - | `GCFont`, `XCharStruct`, `XCreateImage`, `XDestroyImage`, `XDrawString`, `XFontStruct`, `XFreeFont`, `XGCValues.font`, `XGetImage`, `XGetPixel`, `XImage`, `XLoadQueryFont`, `XPutPixel`, `XSetGraphicsExposures`, `XTextExtents`, `XYBitmap`, `XYPixmap`, `ZPixmap`, `bob_png`, `file_to_pixmap`, `font`, `im`, `image_data_to_ximage`, `images/gen/bob_png.h`, `o2`, `overall` | pixmaps, readback, text | 570 |
| flow | 2d | L | - | - | `XSetGraphicsExposures` | pixmaps | 1216 |
| fluidballs | 2d | L | - | - | `FcChar8`, `RootWindow`, `XTranslateCoordinates`, `XftColor`, `XftColorAllocName`, `XftDraw`, `XftDrawCreate`, `XftDrawDestroy`, `XftDrawStringUtf8`, `XftFont`, `XftFontClose`, `error: expected expression`, `error: too few arguments to function call, expected 2, have 1`, `load_xft_font_retry`, `screen_number` | pixmaps | 881 |
| fontglide | 2d | L | - | - | `BlackPixelOfScreen`, `DisplayOfScreen`, `FcChar8`, `XCreateImage`, `XDestroyImage`, `XDrawString`, `XDrawString16`, `XFreeFont`, `XGetAtomName`, `XGetImage`, `XGetPixel`, `XGlyphInfo`, `XImage`, `XLoadQueryFont`, `XLookupString`, `XPutImage`, `XPutPixel`, `XRenderColor`, `XSetFont`, `XTextExtents`, `XTextExtents16`, `XYPixmap`, `XftColor`, `XftColorAllocValue`, `XftColorFree`, `XftDraw`, `XftDrawCreate`, `XftDrawDestroy`, `XftDrawStringUtf8`, `XftFont`, `XftFontClose`, `XftTextExtentsUtf8`, `ZPixmap`, `bg`, `error: Xft is required under X11`, `error: expected expression`, `error: incompatible integer to pointer conversion initializing 'Display *' (aka 'struct XshimDisplay *') with an expression of type 'int' [-Wint-conversion]`, `extents`, `fg`, `in`, `load_xft_font_retry`, `out`, `screen_number`, `swap`, `text_data`, `textclient.h`, `textclient_close`, `textclient_getc`, `textclient_open`, `utf8_decode_combining`, `utf8wc.h`, `xftdraw` | pixmaps, readback, text, clipmask | 2474 |
| glitchpeg | 2d | L | - | - | `BitmapBitOrder`, `ImageByteOrder`, `XCreateImage`, `XDestroyImage`, `XGetPixel`, `XImage`, `XPutImage`, `XPutPixel`, `XtAppAddInput`, `XtDisplayToApplicationContext`, `XtInputExceptMask`, `XtInputId`, `XtInputReadMask`, `XtPointer`, `XtRemoveInput`, `ZPixmap`, `error: operand of type 'XPoint' where arithmetic or pointer type is required`, `image`, `image_data_to_ximage`, `out` | readback | 466 |
| goop | 2d | L | - | - | `AllPlanes`, `DefaultScreenOfDisplay`, `DisplayOfScreen`, `GXclear`, `GXxor`, `WhitePixelOfScreen`, `XSetFunction`, `XSetPlaneMask`, `compute_closed_spline`, `error: incompatible integer to pointer conversion assigning to 'int *' from 'int' [-Wint-conversion]`, `error: incompatible integer to pointer conversion initializing 'Display *' (aka 'struct XshimDisplay *') with an expression of type 'int' [-Wint-conversion]`, `error: member reference base type 'int' is not a structure or union`, `error: type name does not allow function specifier to be specified`, `error: type specifier missing, defaults to 'int'; ISO C99 and later do not support implicit int [-Wimplicit-int]`, `free_spline`, `has_writable_cells`, `make_spline`, `spline` | pixmaps, xor | 651 |
| greynetic | 2d | L | - | - | `GCFillStyle`, `GCStipple`, `XCreatePixmapFromBitmapData`, `XGCValues.fill_style`, `XGCValues.stipple` | - | 297 |
| halo | 2d | L | - | - | `GXxor` | pixmaps | 459 |
| imsmap | 2d | L | - | - | `XCreateImage`, `XDestroyImage`, `XImage`, `XPutImage`, `XPutPixel`, `XYBitmap`, `image` | - | 426 |
| interference | 2d | L | - | - | `GET_PARENT_OBJ`, `THREAD_DEFAULTS`, `THREAD_OPTIONS`, `XImage`, `XPutPixel`, `XShmGetEventBase`, `XShmSegmentInfo`, `ZPixmap`, `create_xshm_image`, `destroy_xshm_image`, `error: expected expression`, `error: field has incomplete type 'struct threadpool'`, `error: too few arguments to function call, expected 2, have 1`, `error: variable has incomplete type 'const struct threadpool_class'`, `hardware_concurrency`, `make_color_loop`, `put_xshm_image`, `thread_memory_alignment`, `thread_util.h`, `threadpool`, `threadpool_create`, `threadpool_destroy`, `threadpool_run`, `threadpool_wait`, `visual_pixmap_depth`, `xshm.h` | pixmaps | 1002 |
| intermomentary | 2d | L | - | - | `XQueryColor`, `XSetTile`, `make_color_ramp`, `rgb_to_hsv` | pixmaps | 605 |
| juggle | 2d | L | - | - | `XDrawImageString`, `XDrawString`, `XFontStruct`, `XFreeFontInfo`, `XLoadQueryFont`, `XTextWidth`, `gettimeofday` | text, float-heavy | 2798 |
| julia | 2d | L | - | - | `Cursor`, `XCreatePixmapCursor`, `XCreatePixmapFromBitmapData`, `XDefineCursor`, `XFreeCursor`, `XSetStipple`, `XSetTSOrigin`, `XUndefineCursor` | pixmaps, float-heavy | 451 |
| kumppa | 2d | L | - | - | `XSetGraphicsExposures` | pixmaps | 545 |
| lcdscrub | 2d | L | - | - | `XCreateImage`, `XDestroyImage`, `XGetPixel`, `XImage`, `XPutImage`, `XPutPixel`, `XYPixmap`, `error: too few arguments to function call, expected 2, have 1` | pixmaps, readback, clipmask | 399 |
| loop | 2d | L | - | - | `CoordModePrevious`, `GCFillStyle`, `GCStipple`, `STIPPLESIZE`, `XCreatePixmapFromBitmapData`, `XGCValues.fill_style`, `XGCValues.stipple`, `automata.h`, `hexagonUnit`, `stipples`, `triangleUnit` | - | 1700 |
| m6502 | 2d | L | - | - | `ANALOGTV_BLACK_LEVEL`, `ANALOGTV_BOT`, `ANALOGTV_DEFAULTS`, `ANALOGTV_OPTIONS`, `ANALOGTV_TOP`, `ANALOGTV_VISLINES`, `ANALOGTV_VIS_END`, `ANALOGTV_VIS_LEN`, `ANALOGTV_VIS_START`, `ANALOGTV_WHITE_LEVEL`, `Bit8`, `analogtv`, `analogtv.h`, `analogtv_allocate`, `analogtv_draw`, `analogtv_draw_solid`, `analogtv_input`, `analogtv_input_allocate`, `analogtv_lcp_to_ntsc`, `analogtv_reception`, `analogtv_reception_update`, `analogtv_reconfigure`, `analogtv_release`, `analogtv_set_defaults`, `analogtv_setup_sync`, `asm6502.h`, `double_time`, `doubletime.h`, `m6502.h`, `m6502_build`, `m6502_destroy6502`, `m6502_next_eval`, `m6502_start_eval_file`, `m6502_start_eval_string`, `machine_6502` | - | 288 |
| marbling | 2d | L | - | - | `DefaultScreenOfDisplay`, `GET_PARENT_OBJ`, `KeyPress`, `KeySym`, `THREAD_DEFAULTS`, `THREAD_OPTIONS`, `XEvent.xkey`, `XImage`, `XK_Down`, `XK_Left`, `XK_Right`, `XK_Up`, `XLookupString`, `XPutPixel`, `XShmSegmentInfo`, `ZPixmap`, `create_xshm_image`, `destroy_xshm_image`, `error: expected expression`, `error: field has incomplete type 'struct threadpool'`, `error: incompatible integer to pointer conversion passing 'int' to parameter of type 'Screen *' (aka 'struct XshimScreen *') [-Wint-conversion]`, `error: variable has incomplete type 'const struct threadpool_class'`, `hardware_concurrency`, `keysym`, `put_xshm_image`, `thread_memory_alignment`, `thread_util.h`, `threadpool_create`, `threadpool_destroy`, `threadpool_run`, `threadpool_wait`, `visual_pixmap_depth`, `xshm.h` | - | 635 |
| memscroller | 2d | L | - | - | `FcChar8`, `XGlyphInfo`, `XImage`, `XShmSegmentInfo`, `XftColor`, `XftColorAllocName`, `XftDraw`, `XftDrawCreate`, `XftDrawDestroy`, `XftDrawStringUtf8`, `XftFont`, `XftFontClose`, `XftTextExtentsUtf8`, `ZPixmap`, `create_xshm_image`, `destroy_xshm_image`, `error: expected expression`, `load_xft_font_retry`, `overall`, `put_xshm_image`, `screen_number`, `xft.h`, `xshm.h` | pixmaps | 626 |
| metaballs | 2d | L | - | - | `BitmapPad`, `XCreateImage`, `XDestroyImage`, `XFree`, `XImage`, `XListPixmapFormats`, `XPutImage`, `XPutPixel`, `XSetWindowBackground`, `ZPixmap` | - | 438 |
| moire | 2d | L | - | - | `BlackPixelOfScreen`, `DefaultScreenOfDisplay`, `WhitePixelOfScreen`, `XImage`, `XPutPixel`, `XQueryColor`, `XShmSegmentInfo`, `ZPixmap`, `create_xshm_image`, `destroy_xshm_image`, `make_color_ramp`, `put_xshm_image`, `rgb_to_hsv`, `visual_depth`, `xshm.h` | - | 253 |
| moire2 | 2d | L | - | - | `GXor`, `GXxor`, `XSetFunction` | pixmaps, xor | 363 |
| nerverot | 2d | L | - | - | `make_color_ramp`, `rgb_to_hsv` | pixmaps, float-heavy | 1367 |
| noseguy | 2d | L | - | - | `FcChar8`, `XCreateImage`, `XDestroyImage`, `XGetImage`, `XGetPixel`, `XGlyphInfo`, `XImage`, `XPutImage`, `XPutPixel`, `XYPixmap`, `XftColor`, `XftColorAllocName`, `XftDraw`, `XftDrawCreate`, `XftDrawDestroy`, `XftDrawStringUtf8`, `XftFont`, `XftFontClose`, `XftTextExtentsUtf8`, `ZPixmap`, `error: expected expression`, `extents`, `i1`, `i2`, `images/gen/nose-f1_png.h`, `images/gen/nose-f2_png.h`, `images/gen/nose-f3_png.h`, `images/gen/nose-f4_png.h`, `images/gen/nose-l1_png.h`, `images/gen/nose-l2_png.h`, `images/gen/nose-r1_png.h`, `images/gen/nose-r2_png.h`, `load_xft_font_retry`, `nose_f1_png`, `nose_f2_png`, `nose_f3_png`, `nose_f4_png`, `nose_l1_png`, `nose_l2_png`, `nose_r1_png`, `nose_r2_png`, `screen_number`, `text_data`, `textclient.h`, `textclient_close`, `textclient_getc`, `textclient_open`, `textclient_reshape` | pixmaps, readback, clipmask | 720 |
| penetrate | 2d | L | - | - | `FcChar8`, `XGlyphInfo`, `XftColor`, `XftColorAllocName`, `XftDraw`, `XftDrawCreate`, `XftDrawDestroy`, `XftDrawStringUtf8`, `XftFont`, `XftFontClose`, `XftTextExtentsUtf8`, `error: expected expression`, `load_xft_font_retry`, `overall`, `screen_number`, `usleep` | - | 1037 |
| phosphor | 2d | L | - | - | `BlackPixelOfScreen`, `DefaultScreenOfDisplay`, `FALSE`, `FcChar8`, `KeyPress`, `TTY_BOLD`, `TTY_INVERSE`, `TTY_ITALIC`, `TTY_SYMBOLS`, `Time`, `XCreateImage`, `XDestroyImage`, `XEvent.xkey`, `XGetImage`, `XGetPixel`, `XGlyphInfo`, `XImage`, `XPutImage`, `XPutPixel`, `XQueryColor`, `XWriteBitmapFile`, `XYBitmap`, `XYPixmap`, `XftColor`, `XftDraw`, `XftDrawCreate`, `XftDrawStringUtf8`, `XftFont`, `XftTextExtentsUtf8`, `XtAppAddTimeOut`, `XtAppContext`, `XtIntervalId`, `XtPointer`, `XtRemoveTimeOut`, `ZPixmap`, `_6x10font_png`, `ansi-tty.h`, `ansi_graphics_unicode`, `ansi_tty`, `ansi_tty_free`, `ansi_tty_init`, `ansi_tty_print`, `ansi_tty_resize`, `app`, `error: expected expression`, `font`, `font_bits`, `im`, `im2`, `images/gen/6x10font_png.h`, `load_xft_font_retry`, `make_color_ramp`, `mm`, `overall`, `rgb_to_hsv`, `screen_number`, `tcell`, `text_data`, `textclient.h`, `textclient_close`, `textclient_getc`, `textclient_open`, `textclient_putc_event`, `textclient_puts`, `textclient_reshape`, `tty`, `tty_char`, `utf8_encode`, `utf8_to_latin1`, `utf8wc.h`, `xft_fg`, `xftdraw`, `xim_color`, `xim_mono` | pixmaps, readback, clipmask | 1260 |
| piecewise | 2d | L | - | - | `make_color_loop` | pixmaps | 1036 |
| polyominoes | 2d | L | - | - | `LSBFirst`, `MSBFirst`, `XCreateImage`, `XDestroyImage`, `XImage`, `XPutImage`, `XYBitmap`, `countof` | - | 2370 |
| pong | 2d | L | - | - | `ANALOGTV_BLACK_LEVEL`, `ANALOGTV_BOT`, `ANALOGTV_DEFAULTS`, `ANALOGTV_OPTIONS`, `ANALOGTV_TOP`, `ANALOGTV_VISLINES`, `ANALOGTV_VIS_END`, `ANALOGTV_VIS_LEN`, `ANALOGTV_VIS_START`, `ButtonPressMask`, `ButtonReleaseMask`, `CurrentTime`, `Cursor`, `FocusChangeMask`, `FocusIn`, `FocusOut`, `GrabModeAsync`, `KeyPress`, `KeyPressMask`, `KeyRelease`, `KeyReleaseMask`, `KeySym`, `X11/keysym.h`, `XCreatePixmapCursor`, `XDefineCursor`, `XDestroyImage`, `XEvent.xkey`, `XGrabPointer`, `XHeightMMOfScreen`, `XHeightOfScreen`, `XK_Down`, `XK_Up`, `XLookupString`, `XUngrabPointer`, `XWarpPointer`, `analogtv`, `analogtv.h`, `analogtv_allocate`, `analogtv_draw`, `analogtv_draw_solid`, `analogtv_draw_string`, `analogtv_font`, `analogtv_font_set_char`, `analogtv_input`, `analogtv_input_allocate`, `analogtv_lcp_to_ntsc`, `analogtv_make_font`, `analogtv_reception`, `analogtv_reception_update`, `analogtv_reconfigure`, `analogtv_release`, `analogtv_set_defaults`, `analogtv_setup_sync`, `double_time`, `doubletime.h`, `key` | pixmaps | 1109 |
| popsquares | 2d | L | - | - | `XQueryColor`, `make_color_ramp`, `rgb_to_hsv` | pixmaps | 310 |
| qix | 2d | L | - | - | `CellsOfScreen`, `DefaultScreenOfDisplay`, `GXxor`, `XQueryColor`, `XSetWindowBackground`, `has_writable_cells`, `rgb_to_hsv` | - | 642 |
| rdbomb | 2d | L | - | - | `DefaultScreenOfDisplay`, `XImage`, `XListPixmapFormats`, `XPixmapFormatValues`, `XSetWindowBackground`, `XShmSegmentInfo`, `ZPixmap`, `create_xshm_image`, `destroy_xshm_image`, `error: expected ')'`, `error: expected function body after function declarator`, `error: expected parameter declarator`, `error: incompatible integer to pointer conversion passing 'int' to parameter of type 'void *' [-Wint-conversion]`, `error: subscripted value is not an array, pointer, or vector`, `has_writable_cells`, `pfv`, `put_xshm_image`, `visual_depth`, `xshm.h` | - | 571 |
| ripples | 2d | L | - | - | `XDestroyImage`, `XGetImage`, `XGetPixel`, `XImage`, `XPutPixel`, `XQueryColor`, `XShmSegmentInfo`, `ZPixmap`, `async_load_state`, `create_xshm_image`, `destroy_xshm_image`, `load_image_async_simple`, `put_xshm_image`, `visual_rgb_masks`, `xshm.h` | readback | 1127 |
| rocks | 2d | L | - | - | `XQueryColor`, `XSetGraphicsExposures` | pixmaps | 562 |
| rotzoomer | 2d | L | - | - | `XDestroyImage`, `XGetImage`, `XGetPixel`, `XImage`, `XPutPixel`, `XShmSegmentInfo`, `ZPixmap`, `async_load_state`, `create_xshm_image`, `destroy_xshm_image`, `load_image_async_simple`, `put_xshm_image`, `xshm.h` | pixmaps, readback | 601 |
| shadebobs | 2d | L | - | - | `BlackPixelOfScreen`, `XCreateImage`, `XDestroyImage`, `XFree`, `XGetPixel`, `XImage`, `XListPixmapFormats`, `XPutImage`, `XPutPixel`, `XSetWindowBackground`, `ZPixmap` | readback | 474 |
| slidescreen | 2d | L | - | - | `XFree`, `XQueryColors`, `async_load_state`, `load_image_async_simple`, `visual_cells` | pixmaps | 507 |
| slip | 2d | L | - | - | `DisplayOfScreen`, `ScreenOfDisplay`, `XSetGraphicsExposures`, `error: incompatible integer to pointer conversion initializing 'Display *' (aka 'struct XshimDisplay *') with an expression of type 'int' [-Wint-conversion]`, `load_image_async` | pixmaps | 376 |
| speedmine | 2d | L | - | - | `XQueryColor`, `error: too few arguments to function call, expected 2, have 1`, `make_color_ramp`, `rgb_to_hsv` | pixmaps, clipmask | 1659 |
| spotlight | 2d | L | - | - | `async_load_state`, `error: too few arguments to function call, expected 2, have 1`, `load_image_async_simple` | pixmaps, clipmask | 355 |
| starfish | 2d | L | - | - | `EvenOddRule`, `GCFillRule`, `XGCValues.fill_rule`, `XSetWindowBackground`, `compute_closed_spline`, `error: incompatible integer to pointer conversion assigning to 'int *' from 'int' [-Wint-conversion]`, `error: member reference base type 'int' is not a structure or union`, `error: type name does not allow function specifier to be specified`, `error: type specifier missing, defaults to 'int'; ISO C99 and later do not support implicit int [-Wimplicit-int]`, `make_spline`, `spline` | - | 564 |
| strange | 2d | L | - | - | `GCGraphicsExposures`, `GET_PARENT_OBJ`, `MSBFirst`, `StaticColor`, `THREAD_OPTIONS`, `TrueColor`, `XGCValues.graphics_exposures`, `XImage`, `XPutPixel`, `XQueryColor`, `XQueryColors`, `XSetFunction`, `XSetGraphicsExposures`, `XShmSegmentInfo`, `ZPixmap`, `aligned_free`, `aligned_malloc`, `create_xshm_image`, `destroy_xshm_image`, `error: field has incomplete type 'struct threadpool'`, `error: incomplete definition of type 'struct threadpool'`, `error: invalid application of 'sizeof' to an incomplete type 'XrmOptionDescRec[]'`, `error: unexpected type name 'ATTRACTOR': expected expression`, `error: variable has incomplete type 'const struct threadpool_class'`, `hardware_concurrency`, `i_log2`, `pow2.h`, `put_xshm_image`, `thread_memory_alignment`, `thread_util.h`, `threadpool_create`, `threadpool_destroy`, `threadpool_run`, `threadpool_wait`, `visual_class`, `visual_pixmap_depth`, `visual_rgb_masks`, `xshm.h` | pixmaps, xor | 1353 |
| swirl | 2d | L | - | - | `XCreateColormap`, `XFree`, `XFreeColormap`, `XImage`, `XInstallColormap`, `XPutPixel`, `XQueryColor`, `XSetWMColormapWindows`, `XSetWindowColormap`, `XShmSegmentInfo`, `XStoreColors`, `ZPixmap`, `create_xshm_image`, `destroy_xshm_image`, `free_colors`, `make_smooth_colormap`, `put_xshm_image`, `rotate_colors`, `xshm.h` | - | 1447 |
| t3d | 2d | L | - | - | `BlackPixelOfScreen`, `Button1Mask`, `Button2Mask`, `Button3Mask`, `GXandInverted`, `GXor`, `KeyPress`, `KeySym`, `XAllocColorCells`, `XEvent.xkey`, `XGetImage`, `XLookupString`, `XPutImage`, `XStoreColors`, `error: too few arguments to function call, expected 2, have 1`, `keysym` | pixmaps, readback, float-heavy | 991 |
| tessellimage | 2d | L | - | - | `ITRIANGLE`, `X11/keysymdef.h`, `XCreateImage`, `XDestroyImage`, `XGetImage`, `XGetPixel`, `XImage`, `XPutImage`, `XPutPixel`, `XQueryColor`, `XYZ`, `ZPixmap`, `async_load_state`, `delaunay`, `delaunay.h`, `delaunay_xyzcompare`, `dimg`, `double_time`, `doubletime.h`, `error: expected expression`, `img2`, `load_image_async_simple`, `p`, `tt`, `v`, `visual_rgb_masks` | pixmaps, readback | 996 |
| testx11 | 2d | L | - | - | `BlackPixelOfScreen`, `GCFont`, `GXxor`, `KeyPress`, `KeySym`, `XClearArea`, `XCreatePixmapFromBitmapData`, `XDestroyImage`, `XDrawString`, `XEvent.xkey`, `XGCValues.font`, `XGetImage`, `XImage`, `XLoadFont`, `XLookupString`, `XPutImage`, `XPutPixel`, `XSetWindowBackground`, `ZPixmap`, `colorbars.h`, `draw_colorbars`, `error: expected ')'`, `error: expected function body after function declarator`, `error: expected parameter declarator`, `image`, `keysym`, `make_color_loop`, `visual_depth` | pixmaps, readback, text, clipmask | 968 |
| twang | 2d | L | - | - | `XDestroyImage`, `XGetImage`, `XGetPixel`, `XImage`, `XPutPixel`, `XShmSegmentInfo`, `ZPixmap`, `async_load_state`, `create_xshm_image`, `destroy_xshm_image`, `load_image_async_simple`, `put_xshm_image`, `xshm.h` | pixmaps, readback | 791 |
| vfeedback | 2d | L | - | - | `ANALOGTV_DEFAULTS`, `ANALOGTV_OPTIONS`, `ANALOGTV_SIGNAL_LEN`, `BlackPixelOfScreen`, `Button6`, `Button7`, `EASE_IN_OUT_SINE`, `KeyPress`, `KeySym`, `RANDSIGN`, `XCreateImage`, `XDestroyImage`, `XEvent.xkey`, `XGetImage`, `XGetPixel`, `XImage`, `XK_Down`, `XK_Left`, `XK_Right`, `XK_Up`, `XLookupString`, `XPutPixel`, `ZPixmap`, `analogtv`, `analogtv.h`, `analogtv_allocate`, `analogtv_draw`, `analogtv_input_allocate`, `analogtv_load_ximage`, `analogtv_reception`, `analogtv_reception_update`, `analogtv_reconfigure`, `analogtv_release`, `analogtv_set_defaults`, `analogtv_setup_sync`, `double_time`, `doubletime.h`, `ease`, `easing.h`, `img`, `in`, `keysym`, `out` | pixmaps, readback | 592 |
| wander | 2d | L | - | - | `make_color_loop` | pixmaps | 284 |
| whirlygig | 2d | L | - | - | `XDrawString` | pixmaps, text, float-heavy | 741 |
| xanalogtv | 2d | L | - | - | `ANALOGTV_DEFAULTS`, `ANALOGTV_OPTIONS`, `ANALOGTV_PIC_LEN`, `ANALOGTV_SCALE`, `ANALOGTV_SIGNAL_LEN`, `ANALOGTV_V`, `ANALOGTV_VIS_LEN`, `ANALOGTV_VIS_START`, `KeyPress`, `KeySym`, `XDestroyImage`, `XEvent.xkey`, `XGetImage`, `XImage`, `XK_Down`, `XK_Left`, `XK_Next`, `XK_Prior`, `XK_Right`, `XK_Up`, `XLookupString`, `XrmDatabase`, `XrmPutResource`, `XrmValue`, `ZPixmap`, `analogtv`, `analogtv.h`, `analogtv_allocate`, `analogtv_draw`, `analogtv_draw_solid_rel_lcp`, `analogtv_draw_string_centered`, `analogtv_font`, `analogtv_input`, `analogtv_input_allocate`, `analogtv_lcp_to_ntsc`, `analogtv_load_ximage`, `analogtv_make_font`, `analogtv_reception`, `analogtv_reception_update`, `analogtv_reconfigure`, `analogtv_release`, `analogtv_set_defaults`, `analogtv_setup_sync`, `analogtv_setup_teletext`, `db`, `file_to_pixmap`, `gethostname`, `image`, `images/gen/testcard_bbcf_png.h`, `images/gen/testcard_pm5544_png.h`, `images/gen/testcard_rca_png.h`, `inp`, `input`, `keysym`, `load_image_async`, `rec`, `testcard_bbcf_png`, `testcard_pm5544_png`, `testcard_rca_png`, `value`, `ximage` | pixmaps, readback | 689 |
| xflame | 2d | L | - | - | `XCreateImage`, `XDestroyImage`, `XGetPixel`, `XImage`, `XPutPixel`, `XQueryColor`, `XShmSegmentInfo`, `ZPixmap`, `bob_png`, `create_xshm_image`, `destroy_xshm_image`, `file_to_ximage`, `image`, `image_data_to_ximage`, `images/gen/bob_png.h`, `out`, `put_xshm_image`, `xshm.h` | readback | 826 |
| xjack | 2d | L | - | - | `FcChar8`, `XClearArea`, `XGlyphInfo`, `XftColor`, `XftColorAllocName`, `XftDraw`, `XftDrawCreate`, `XftDrawDestroy`, `XftDrawStringUtf8`, `XftFont`, `XftFontClose`, `XftTextExtentsUtf8`, `error: expected expression`, `load_xft_font_retry`, `overall`, `screen_number` | pixmaps | 508 |
| xlyap | 2d | L | - | - | `BlackPixelOfScreen`, `Cursor`, `KeyPress`, `KeySym`, `WhitePixelOfScreen`, `X11/cursorfont.h`, `XComposeStatus`, `XEvent.xkey`, `XKeyEvent`, `XLookupString`, `XPending`, `XStoreColors`, `error: expected expression` | pixmaps | 1939 |
| xmatrix | 2d | L | - | - | `KeyPress`, `KeySym`, `XCreateImage`, `XDestroyImage`, `XEvent.xkey`, `XGetImage`, `XGetPixel`, `XImage`, `XLookupString`, `XPutImage`, `XPutPixel`, `XYPixmap`, `XtAppAddTimeOut`, `XtAppContext`, `XtIntervalId`, `XtPointer`, `XtRemoveTimeOut`, `ZPixmap`, `app`, `i1`, `i2`, `im`, `images/gen/matrix1_png.h`, `images/gen/matrix1b_png.h`, `images/gen/matrix2_png.h`, `images/gen/matrix2b_png.h`, `keysym`, `matrix1_png`, `matrix1b_png`, `matrix2_png`, `matrix2b_png`, `text_data`, `textclient.h`, `textclient_close`, `textclient_getc`, `textclient_open`, `textclient_reshape` | pixmaps, readback | 1915 |
| zoom | 2d | L | - | - | `XDestroyImage`, `XGetImage`, `XGetPixel`, `XImage`, `XSetWindowBackground`, `ZPixmap`, `async_load_state`, `error: too few arguments to function call, expected 2, have 1`, `load_image_async_simple` | pixmaps, readback | 290 |
| antinspect | gl | XL | - | - | - | - | 696 |
| antmaze | gl | XL | - | - | - | - | 1612 |
| antspotlight | gl | XL | - | - | - | - | 797 |
| atlantis | gl | XL | - | - | `XDestroyImage` | - | 607 |
| atunnel | gl | XL | - | - | `XDestroyImage` | - | 315 |
| b_lockglue | gl | XL | - | - | - | - | 240 |
| beats | gl | XL | - | - | - | - | 439 |
| blinkbox | gl | XL | - | - | - | - | 615 |
| blocktube | gl | XL | - | - | `XDestroyImage` | - | 454 |
| boing | gl | XL | - | - | - | float-heavy | 666 |
| bouncingcow | gl | XL | - | - | - | - | 649 |
| boxed | gl | XL | - | - | - | - | 1361 |
| cage | gl | XL | - | - | `XDestroyImage` | - | 498 |
| carousel | gl | XL | - | - | - | - | 982 |
| chompytower | gl | XL | - | - | - | - | 1131 |
| circuit | gl | XL | - | - | - | - | 2106 |
| cityflow | gl | XL | - | - | - | - | 561 |
| companion | gl | XL | - | - | - | - | 605 |
| covid19 | gl | XL | - | - | - | - | 657 |
| crackberg | gl | XL | - | - | `XLookupString`, `XNextEvent`, `XPeekEvent`, `XPending` | float-heavy | 1484 |
| crumbler | gl | XL | - | - | - | - | 905 |
| cube21 | gl | XL | - | - | - | - | 943 |
| cubenetic | gl | XL | - | - | - | - | 616 |
| cubestack | gl | XL | - | - | `XLookupString` | - | 453 |
| cubestorm | gl | XL | - | - | `XLookupString` | - | 485 |
| cubetwist | gl | XL | - | - | `XLookupString` | - | 579 |
| cubocteversion | gl | XL | - | - | `XDestroyImage` | - | 5657 |
| dangerball | gl | XL | - | - | - | - | 377 |
| deepstars | gl | XL | - | - | - | - | 385 |
| discoball | gl | XL | - | - | - | - | 709 |
| dnalogo | gl | XL | - | - | `XLookupString` | float-heavy | 3657 |
| dumpsterfire | gl | XL | - | - | - | - | 845 |
| dymaxionmap | gl | XL | - | - | `XCreateImage`, `XDestroyImage`, `XGetPixel`, `XLookupString`, `XPutPixel` | readback | 1729 |
| endgame | gl | XL | - | - | - | - | 1451 |
| energystream | gl | XL | - | - | - | - | 536 |
| engine | gl | XL | - | - | - | - | 1015 |
| esper | gl | XL | - | - | `XLookupString` | - | 2463 |
| etruscanvenus | gl | XL | - | - | - | - | 2761 |
| extrusion | gl | XL | - | - | - | - | 552 |
| flipflop | gl | XL | - | - | - | - | 875 |
| flipscreen3d | gl | XL | - | - | - | - | 524 |
| fliptext | gl | XL | - | - | - | - | 1005 |
| floppy | gl | XL | - | - | - | - | 584 |
| flurry | gl | XL | - | - | - | - | 550 |
| flyingtoasters | gl | XL | - | - | `XDestroyImage` | - | 924 |
| geodesic | gl | XL | - | - | - | float-heavy | 825 |
| geodesicgears | gl | XL | - | - | `XLookupString` | float-heavy | 1802 |
| gflux | gl | XL | - | - | - | - | 799 |
| gibson | gl | XL | - | - | - | - | 1317 |
| glblur | gl | XL | - | - | - | - | 630 |
| glcells | gl | XL | - | - | - | - | 1382 |
| gleidescope | gl | XL | - | - | `XOFFSET` | float-heavy | 1620 |
| glforestfire | gl | XL | - | - | `XDestroyImage` | - | 1089 |
| glhanoi | gl | XL | - | - | - | float-heavy | 2080 |
| glknots | gl | XL | - | - | - | - | 455 |
| glmatrix | gl | XL | - | - | `XDestroyImage`, `XGetPixel`, `XPutPixel` | readback | 1077 |
| glplanet | gl | XL | - | - | `XDestroyImage` | float-heavy | 1111 |
| glschool | gl | XL | - | - | - | - | 229 |
| glslideshow | gl | XL | - | - | - | - | 1917 |
| glsnake | gl | XL | - | - | - | - | 2699 |
| gltext | gl | XL | - | - | - | - | 619 |
| graphstat | gl | XL | - | - | - | - | 750 |
| gravitywell | gl | XL | - | - | - | - | 770 |
| handsy | gl | XL | - | - | `XLookupString` | - | 1158 |
| headroom | gl | XL | - | - | - | - | 628 |
| hexstrut | gl | XL | - | - | `XLookupString` | - | 511 |
| hextrail | gl | XL | - | - | `XLookupString` | - | 782 |
| highvoltage | gl | XL | - | - | `XLookupString` | - | 949 |
| hilbert | gl | XL | - | - | `XLookupString` | - | 1165 |
| hopffibration | gl | XL | - | - | - | - | 3580 |
| hydrostat | gl | XL | - | - | - | - | 796 |
| hypertorus | gl | XL | - | - | `XLookupString` | float-heavy | 2150 |
| hypnowheel | gl | XL | - | - | - | - | 335 |
| jigglypuff | gl | XL | - | - | `XDestroyImage` | - | 1125 |
| jigsaw | gl | XL | - | - | - | - | 1512 |
| juggler3d | gl | XL | - | - | - | float-heavy | 3023 |
| kaleidocycle | gl | XL | - | - | `XLookupString` | - | 584 |
| kallisti | gl | XL | - | - | - | - | 357 |
| klein | gl | XL | - | - | `XLookupString` | float-heavy | 3574 |
| klondike | gl | XL | - | - | `XDestroyImage` | - | 803 |
| lament | gl | XL | - | - | `XDestroyImage`, `XLookupString` | - | 1801 |
| lavalite | gl | XL | - | - | - | - | 1552 |
| lockward | gl | XL | - | - | `XLookupString` | - | 964 |
| mapscroller | gl | XL | - | - | `XDestroyImage`, `XGetPixel`, `XLookupString` | readback | 1672 |
| maze3d | gl | XL | - | - | - | - | 1958 |
| menger | gl | XL | - | - | `XLookupString` | - | 574 |
| mirrorblob | gl | XL | - | - | - | - | 1822 |
| moebius | gl | XL | - | - | `XDestroyImage` | - | 794 |
| moebiusgears | gl | XL | - | - | `XLookupString` | - | 446 |
| molecule | gl | XL | - | - | `XLookupString` | - | 1716 |
| nakagin | gl | XL | - | - | - | - | 1636 |
| noof | gl | XL | - | - | - | - | 530 |
| papercube | gl | XL | - | - | - | - | 1111 |
| peepers | gl | XL | - | - | `XChangeProperty`, `XDestroyImage`, `XInternAtom` | float-heavy | 1470 |
| photopile | gl | XL | - | - | - | - | 869 |
| pinion | gl | XL | - | - | `XLookupString` | - | 1497 |
| pipes | gl | XL | - | - | - | - | 1208 |
| platonicfolding | gl | XL | - | - | `XDestroyImage` | - | 3465 |
| polyhedra-gl | gl | XL | - | - | `XLookupString` | - | 687 |
| polytopes | gl | XL | - | - | `XLookupString` | - | 3194 |
| projectiveplane | gl | XL | - | - | `XLookupString` | float-heavy | 2643 |
| providence | gl | XL | - | - | - | float-heavy | 811 |
| pulsar | gl | XL | - | - | - | - | 509 |
| quasicrystal | gl | XL | - | - | `XLookupString` | - | 494 |
| queens | gl | XL | - | - | - | - | 608 |
| raverhoop | gl | XL | - | - | `XLookupString` | - | 771 |
| razzledazzle | gl | XL | - | - | - | - | 728 |
| romanboy | gl | XL | - | - | - | - | 2565 |
| rubik | gl | XL | - | - | - | - | 2156 |
| rubikblocks | gl | XL | - | - | - | - | 651 |
| sballs | gl | XL | - | - | `XDestroyImage` | - | 830 |
| sierpinski3d | gl | XL | - | - | `XLookupString` | - | 579 |
| skulloop | gl | XL | - | - | - | - | 651 |
| skytentacles | gl | XL | - | - | `XCreateImage`, `XDestroyImage`, `XLookupString`, `XPutPixel` | float-heavy | 1124 |
| sonar | gl | XL | - | - | - | - | 1266 |
| sphereeversion | gl | XL | - | - | `XDestroyImage` | - | 1420 |
| spheremonics | gl | XL | - | - | - | - | 926 |
| splitflap | gl | XL | - | - | - | - | 1427 |
| splodesic | gl | XL | - | - | `XLookupString` | float-heavy | 645 |
| sproingiewrap | gl | XL | - | - | - | - | 227 |
| squirtorus | gl | XL | - | - | - | - | 1006 |
| stairs | gl | XL | - | - | `XDestroyImage`, `XLookupString` | - | 601 |
| starwars | gl | XL | - | - | - | - | 1080 |
| stonerview | gl | XL | - | - | - | - | 156 |
| superquadrics | gl | XL | - | - | - | - | 811 |
| surfaces | gl | XL | - | - | - | float-heavy | 654 |
| tangram | gl | XL | - | - | - | - | 1074 |
| timetunnel | gl | XL | - | - | `XDestroyImage` | - | 1259 |
| topblock | gl | XL | - | - | `XLookupString` | - | 891 |
| tronbit | gl | XL | - | - | `XLookupString` | - | 532 |
| unicrud | gl | XL | - | - | `XLookupString` | - | 1039 |
| unknownpleasures | gl | XL | - | - | `XCreateImage`, `XDestroyImage`, `XGetPixel`, `XLookupString`, `XPutPixel` | readback | 736 |
| vigilance | gl | XL | - | - | `XLookupString` | - | 1171 |
| voronoi | gl | XL | - | - | - | - | 543 |
| winduprobot | gl | XL | - | - | `XDestroyImage`, `XLookupString` | float-heavy | 2505 |
| worldpieces | gl | XL | - | - | `XDestroyImage` | float-heavy | 2194 |
| xshadertoy | gl | XL | - | - | `XFetchName`, `XStoreName` | - | 1192 |
| blaster | 2d | - | 3.6-3.7 ms measured | ✅ | - | - | 1208 |
| braid | 2d | - | 95-296 ms measured | ✅ | - | - | 444 |
| cloudlife | 2d | - | 8.8-8.9 ms measured | ✅ | - | - | 440 |
| coral | 2d | - | 15-25 ms measured | ✅ | - | - | 328 |
| critical | 2d | - | 0.7 ms measured | ✅ | - | - | 462 |
| deluxe | 2d | - | 44.6-62.6 ms measured | ✅ | - | pixmaps, float-heavy | 480 |
| discrete | 2d | - | 149-160 ms measured | ✅ | - | - | 442 |
| drift | 2d | - | 11-13 ms measured | ✅ | - | - | 674 |
| epicycle | 2d | - | 0.3-2.1 ms measured | ✅ | - | - | 803 |
| fadeplot | 2d | - | 2.9-3.2 ms measured | ✅ | - | - | 243 |
| flame | 2d | - | 26-107 ms measured | ✅ | - | - | 457 |
| galaxy | 2d | - | 58-75 ms measured | ✅ | - | - | 462 |
| helix | 2d | - | 0.9-2.1 ms measured | ✅ | - | - | 358 |
| hopalong | 2d | - | 7.3-20.1 ms measured | ✅ | - | - | 563 |
| hypercube | 2d | - | 2.7-2.9 ms measured | ✅ | - | - | 576 |
| kaleidescope | 2d | - | 7-9.8 ms measured | ✅ | - | - | 514 |
| lightning | 2d | - | 1.8-1.9 ms measured | ✅ | - | - | 602 |
| maze | 2d | - | 0.1-1.3 ms measured | ✅ | - | pixmaps, clipmask | 1681 |
| mountain | 2d | - | 0-0.4 ms measured | ✅ | - | - | 283 |
| pedal | 2d | - | 126-519 ms measured | ✅ | - | - | 339 |
| petri | 2d | - | 0.3-2.5 ms measured | ✅ | - | - | 780 |
| pyro | 2d | - | 0.8-1.1 ms measured | ✅ | - | - | 373 |
| rorschach | 2d | - | 0.6-1.7 ms measured | ✅ | - | - | 227 |
| sierpinski | 2d | - | 1.8-4 ms measured | ✅ | - | - | 215 |
| sphere | 2d | - | 0.6-1.1 ms measured | ✅ | - | - | 304 |
| spiral | 2d | - | 0.9-1.2 ms measured | ✅ | - | - | 331 |
| squiral | 2d | - | 0.1-0.3 ms measured | ✅ | - | - | 335 |
| substrate | 2d | - | 2.9-43.9 ms measured | ✅ | - | - | 780 |
| thornbird | 2d | - | 2-2.2 ms measured | ✅ | - | - | 270 |
| vines | 2d | - | 133-160 ms measured | ✅ | - | - | 190 |
| whirlwindwarp | 2d | - | 6.2-26.4 ms measured | ✅ | - | - | 509 |
| xspirograph | 2d | - | 54-56 ms measured | ✅ | - | - | 338 |
| pacman | 2d | - | 1.3-1.9 ms measured | ✅ | `BLUE`, `GHOSTS`, `GHOST_DANGER`, `JAILHEIGHT`, `LEVHEIGHT`, `LEVWIDTH`, `MAXGDIR`, `MAXGFLASH`, `MAXGWAG`, `MAXMOUTH`, `MINGRIDSIZE`, `MINSIZE`, `NOWHERE`, `NUM_BONUS_DOTS`, `PAC_DEATH_FRAMES`, `START`, `XDrawString`, `XLoadQueryFont`, `chasing`, `error: expected expression`, `error: invalid application of 'sizeof' to an incomplete type 'argtype[]'`, `ghoststruct`, `goingin`, `goingout`, `hiding`, `images/gen/pacman_png.h`, `inbox`, `pacman.h`, `pacman_ai.h`, `pacman_bonus_dot_eaten`, `pacman_bonus_dot_pos`, `pacman_createnewlevel`, `pacman_eat_bonus_dot`, `pacman_ghost_update`, `pacman_is_bonus_dot`, `pacman_level.h`, `pacman_png`, `pacman_trackmouse`, `pacman_update`, `pacmangamestruct`, `pp`, `ps_chasing`, `ps_dieing`, `ps_eating` | pixmaps, text, clipmask | 1479 |
| cubicgrid | gl | - | - | ✅ | - | - | 322 |
| gears | gl | - | 65-330 ms measured | ✅ | - | - | 953 |
| morph3d | gl | - | 48-86 ms measured | ✅ | - | - | 841 |

## Failed ports

- **celtic**: shelved at 1.2 fps and 300 KB of heap, see issue #37

## Excluded files

These files in `hacks/` and `hacks/glx/` have no `XSCREENSAVER_MODULE`
entry point, so they are models, helper libraries or command-line
tools rather than screensavers:

analogtv, analogtv-cli, ansi-tty, apple2, asm6502, b_draw, b_sphere, bubble3d,
bubbles-default, buildlwo, chessmodels, companion_disc, companion_heart,
companion_quad, countries, cow_face, cow_hide, cow_hoofs, cow_horns, cow_tail,
cow_udder, delaunay, dolphin, dropshadow, dumpster_model, dymaxionmap-coords,
earth, erase-gl, extrusion-helix2, extrusion-helix3, extrusion-helix4,
extrusion-joinoffset, extrusion-screw, extrusion-taper, extrusion-twistoid,
ffmpeg-out, floppy_model, flurry-smoke, flurry-spark, flurry-star, flurry-
texture, fps, fps-gl, gllist, glschool_alg, glschool_gl, glsl-utils,
gltrackball, glut_stroke, glut_swidth, grab-ximage, handsy_model,
headroom_model, highvoltage_model, hopfanimations, involute, kallisti_model,
klondike-game, lament_model, marching, normals, pacman_ai, pacman_level,
pipeobjs, polyhedra, quaternion, quickhull, recanim, robot, robot-wireframe,
rotator, s1_1, s1_2, s1_3, s1_4, s1_5, s1_6, s1_b, screenhack, seccam, shark,
ships, skull_model, sonar-icmp, sonar-sim, sphere, sphereeversion-analytic,
sphereeversion-corrugations, splitflap_obj, sproingies, stonerview-move,
stonerview-osc, stonerview-view, swim, tangram_shapes, teapot, teeth_model,
texfont, timezones, toast, toast2, toaster, toaster_base, toaster_handle,
toaster_handle2, toaster_jet, toaster_knob, toaster_slots, toaster_wing,
trackball, triangle, tronbit_idle1, tronbit_idle2, tronbit_no, tronbit_yes,
tube, tunnel_draw, webcollage-helper, whale, ximage-loader, xlock-gl-utils,
xlockmore, xscreensaver-getimage, xscreensaver-gl-visual, xsublim
