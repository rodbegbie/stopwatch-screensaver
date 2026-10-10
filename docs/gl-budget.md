# GL budget: cubicgrid and morph3d

`tools/gl_budget.py` runs a GL hack on the host, counts what each frame asks of
TinyGL, and predicts the device frame time. It was built to choose between two
candidates for the next GL port (issue #45) without flashing either.

```text
uv run tools/gl_budget.py [--frames N] [--seeds a,b,c] name[:seed] ...
```

Gears is read from `firmware/src/hacks/gears/`; any other name from
`vendor/xscreensaver-6.16/hacks/glx/` (git-ignored, never copied into the
repo). A hack that does not build is reported with its first error, and the
exit status is 1.

## What it counts

The counters sit inside a private copy of TinyGL, built only for the tool
(`firmware/src/tinygl/` and the firmware builds are untouched). Counting there
means a display list that is built once and replayed every frame is counted on
every frame. Per frame, it records:

- **Vertices** set up, and how many of them were lit.
- **Triangles** rasterised, after back-face culling, and the **pixels** they
  cover (screen area, overdraw included, capped at the canvas).
- **Lines** and **points** drawn.

## The cost model

A frame costs a fixed 56.6 ms (clear 24.6 ms, display push 32 ms), plus 4 us
per vertex, 2.8 us more for each enabled light on each lit vertex (both from the
Pipes probe in #43, measured with one light), plus a fill term. The fill term
is 570 ns a pixel, fitted so that the heavy Gears scene (seed 13, 17,700
vertices) costs the 40 ms of fill that #43 measured.

Checked against Gears, the only hack with device numbers. The fit and the
tests use 60 frames; the table below is a 300 frame run, and Gears' pixel
count drifts by about 15 percent between the two as the gears turn (at 60
frames the heavy scene is 217 ms and its fill is exactly 40 ms; at 300 it is
211 ms and 34 ms):

| Scene | Predicted | Measured (#43) |
| --- | --- | --- |
| Gears seed 13, heavy | 211 ms, 4.7 fps | 197 to 217 ms, about 5 fps |
| Gears seed 11, light (held out) | 117 ms, 8.5 fps | about 111 ms, 9 fps |

The light scene was not used in the fit and lands within 6 percent.

## The comparison

Means over 300 frames after a 10 frame warm-up. Both hacks are lit or point
based, so the fixed 56.6 ms sets a ceiling of about 17.7 fps for anything.

| Hack | Vertices | Triangles | Points | Predicted | fps |
| --- | --- | --- | --- | --- | --- |
| morph3d, four of its five shapes (seeds 1 to 8) | 3,864 to 7,200 | 2,300 to 4,000 | 0 | 87 to 114 ms | 8.8 to 11.5 |
| cubicgrid, default `ticks` 30 | 27,000 | 0 | 27,000 | 165 ms | 6.1 |

- **morph3d** is lit and vertex-bound, but treat 9 to 11 fps as optimistic.
  It enables two lights and two-sided lighting, and TinyGL shades once per
  light, while the model's lighting cost was measured with one (about +20 ms
  for seed 1). Its triangles are also tiny, 3 to 4 pixels each against 15 to
  29 in the Gears scenes the fill figure was fitted to. The fill term absorbs
  per-triangle cost at Gears' ratio, so it under-predicts here: if all of
  Gears' fill were per-triangle cost, morph3d's would be about 35 ms rather
  than 8. Together these could put morph3d nearer 6 to 8 fps.
- **cubicgrid** draws a 30 by 30 by 30 grid of points (`ticks` cubed) from a
  display list every frame. At the default that is 27,000 vertices, which is
  why it lands at about 6 fps. The model does not cost the points themselves,
  and they are drawn 2.5 pixels wide (`bigdots` defaults to True), so 6 fps is
  an optimistic floor too.
- Its size is a resource. Setting `*ticks` through a `HackEntry` override, as
  Galaxy does with `*count`, shrinks the cost with the cube, by the same
  arithmetic (vertices only, no point cost): `ticks` 20 gives 8,000 points,
  about 89 ms and 11 fps; 15 gives 3,375, about 70 ms and 14 fps.

## Verdict

Port **morph3d** first, but the margin is narrower than the first numbers
suggested. Both hacks are optimistic for reasons the model cannot see, and by
different amounts. morph3d needs no override and should land in the high single
digits; cubicgrid at its default is at best 6 fps and probably worse, and needs
a `*ticks` override of about 15 to 20 to be competitive. morph3d is also a lit,
morphing solid, which is what the GL layer is for. Port **cubicgrid** second,
once morph3d has given us a second point to check the model against, and the
tool can be taught the two-light and small-triangle cases from it.

The numbers say what each frame costs, not how it looks: a dot lattice at 14
fps and a morphing polyhedron at 10 fps are different bets on what Rod will
want to see on the board.

## Accuracy against the device

Gears, Morph3D and CubicGrid have all now run on the board. The predictions
below were committed to this document before each port was flashed. Measured
is step plus push, the quantity the model predicts.

| Scene | Predicted | Measured | Error |
| --- | --- | --- | --- |
| Gears heavy, seed 13 | 211 ms | about 207 ms (#43) | in-sample, fitted |
| Gears light, seed 11 | 117 ms | about 111 ms (#43) | +5% (held out) |
| Morph3D, 5,040 or 5,100 vertices (corrected, see below) | 98 to 100 ms | 111 to 118 ms | +11% to +20% |
| Morph3D, 2,300 vertices (not in the first survey) | none | 84 ms | |
| CubicGrid, 27,000 points | 165 ms | 170 to 180 ms | +3% to +9% |
| CubicGrid, 8,000 points | 89 ms | 80 to 88 ms | -10% to -1% |

- **Good to about 10 percent for Gears and CubicGrid; Morph3D was 11 to 20
  percent too optimistic** (see the correction below), until the model charged
  each enabled light. It ranked them correctly throughout, which is the use it
  was built for: choosing which hack to port and what to turn down.
- **The Gears rows are weaker evidence than the rest.** Their "measured"
  figures are the breakdown quoted in issue #43, not a capture made for this
  comparison, and the fill figure was fitted to the heavy one.
- **Correction (from the overnight capture, below).** An earlier version of
  this table matched Morph3D's measured steps to its shapes by size alone and
  called them the 7,200 and 3,864 vertex shapes, giving errors of -3 percent to
  +4 percent. The overnight capture shows the 78 to 82 ms and 48 ms steps were
  the 5,040 or 5,100 vertex shapes and the 2,300 vertex shape, so the old model
  under-predicted by 11 to 20 percent. The device does not seed `random()`, so a
  start cannot be asked for its shape; shapes are matched by the order of their
  step times, which must follow vertex count.
- **The review's worry about lights was right.** Morph3D has two lights and the
  model charged one lit cost per vertex. Charging 2.8 us for each enabled light
  (the `light_terms` counter) reproduces all five shapes: predicted old steps of
  48, 66, 80 or 82 and 102 ms against 48 and 78 to 82 measured directly and about
  45, 65, 81 and 102 inferred. Its tiny triangles and CubicGrid's wide points
  have not shown up as errors, and CubicGrid's two sizes imply about
  4.7 microseconds a point against the model's 4.
- **Use it for ranking and go or no-go, with a margin of 10 percent.** A
  prediction within 10 percent of a frame-rate threshold needs the board to
  settle it. Add each new GL port's measured step to the table and refit
  `pixel_ns` if the errors drift in one direction.

## Device check: Morph3D

Flashed on 2026-10-09. The first capture was nine 5 second windows of one
start: 8.0 to 8.4 fps, a step of 78 to 86 ms and a push of 32.6 ms. A second
capture rotated every 10 seconds for 26 minutes (155 rotations, 4 to 5 laps, no
resets or panics) and gave five Morph3D starts: four at 8.2 to 8.4 fps (step 78
to 82 ms) and one at 11.4 fps (step 48 ms). Push was 36 ms on that build.
PSRAM was identical on every visit; the free heap on entering Morph3D fell 308,
36 and 40 bytes on the three laps after the first, which is in line with
Gears' known upstream leak and cannot be attributed to Morph3D.

This section's first reading of those steps (that they were the 3,864 and 7,200
vertex shapes) was wrong; see the correction under "Accuracy against the
device" and the overnight capture below.

## Device check: CubicGrid

Flashed on 2026-10-09 at its default grid (`ticks` 30, 27,000 points), over 45
seconds with rotation off: 5.6 to 6.0 fps, a step of 137 to 148 ms and a push
of 32.3 to 36.0 ms, so step plus push is 170 to 180 ms. The harness predicted
165 ms and 6.1 fps, which is within about 8 percent. The step fell from 148 to
137 ms over the capture as the grid turned, so it is not constant. Free PSRAM
read 1.66 MB below Morph3D's, which is the display list of 27,000 points; heap
was flat. The points the model does not cost were not a large part of the frame.

### CubicGrid at 20 ticks

The registry now sets `*ticks: 20` (8,000 points). The harness's arithmetic for
that was about 89 ms and 11 fps, with `ticks` 15 at about 70 ms and 14 fps.
Measured on the board over 45 seconds: 11.2 to 12.4 fps, a step of 48 to 52 ms
and a push of 32.2 to 36.1 ms, so step plus push is 80 to 88 ms, a little
under the prediction. Free PSRAM was 0.49 MB below Morph3D's. Two points on
the same hack, 27,000 points at 170 to 180 ms and 8,000 at 80 to 88 ms, give
about 4.7 us a point, close to the model's 4 us a vertex; the points are
cheap, as the model assumed.

## Where a frame goes (CubicGrid profile)

After the clip-epsilon fix (#43), CubicGrid's step at 8,000 points is 46.7 ms
in its first window. Temporary early returns in TinyGL, built only for the
experiment and never committed, split it on the board (each boot is
deterministic, so windows compare directly):

| Part | Time | Share |
| --- | --- | --- |
| Colour clear (a build with no colour clear ran at 34.0 ms) | 12.7 ms | 27% |
| List read and op dispatch, the hack's own work (the rest of the 22.3 ms floor left when `glopVertex` returns at once) | about 9.6 ms | 21% |
| Vertex pipeline: transform, clip code, projection (35.6 ms with `gl_draw_point` also skipped, less the floor) | 13.3 ms, about 1.7 us a vertex | 28% |
| `gl_draw_point` | 11.1 ms, about 1.4 us a point | 24% |

What this says for #43:

- **`gl_shade_vertex` has no `double` work left on the per-vertex path** for
  these hacks. After the epsilon fix the only soft-double references in
  TinyGL are `glFrustum`, `glopLight`, `glopRotate`, `glClearDepth` and a debug
  printer (all per call), and two compares against `1E-3` in `gl_shade_vertex`
  that run only for positional lights and specular. Gears and Morph3D use
  directional lights with specular off, so they never run. That patch would be
  a guess without a hack to time it on.
- **CubicGrid asks only for a colour clear**, so skipping a z clear would save
  nothing there, and Gears and Morph3D need theirs for depth test.
- **The colour clear runs at about 34 MB/s** (434 KB in 12.7 ms), and the list
  is probably also read from PSRAM at about that rate, which is a guess, not a
  measurement. These costs are memory bandwidth, not arithmetic.
- **The largest idea was a dirty rectangle for GL** (clear and push only the
  box a hack drew). It is prototyped below; overlapping the push with the next
  frame is the other big lever and is untried.

## Dirty rectangle prototype (#43)

`tools/gl_budget.py` now reports, per frame, the box around what a hack drew
and what clearing and pushing only that area would cost, as a percentage of the
canvas. Over 300 frames:

| Hack | Box | Clear | Push |
| --- | --- | --- | --- |
| Morph3D, all four shapes | 6 to 9% | 6 to 9% | 7 to 9% |
| Gears, four seeds (light to heaviest) | 19 to 27% | 19 to 27% | 19 to 27% |
| CubicGrid | 100% | 100% | 100% |

Morph3D's flower is small, so the box is far smaller than the 30 percent I
first guessed. The prototype (`glshim_note_box` and `glshim_clear` in
`firmware/src/glshim/glshim.c`, hooked in by `tools/tinygl_patch.py`) tracks
that box. `glClear` clears only the box drawn since the last clear; a swap
marks only what was drawn and what the clear erased. A new context starts with
a full clear and a full push, and a new clear colour is a full clear, so it
cannot leave stale pixels behind. It assumes nothing else draws on the canvas
while a GL hack runs, which the overlay already respects (it restores what it
stamps).

On the board, same boots, step and push in ms (rows is the canvas rows sent a
frame, out of 466):

| Hack | Before | Dirty rectangle |
| --- | --- | --- |
| Morph3D | step 75 to 83, push 32.5, wait 8.8, rows 466, 8.2 to 8.8 fps | step 55 to 59, push 2 to 9, wait 34 to 42, rows 92 to 192, 9.8 to 10.2 fps |
| Morph3D, `*delay: 10000` (now registered) | | step 54 to 58, push 2 to 9, wait 2.6 to 9.4, 14.4 to 15.2 fps |
| Gears, first layout after boot | step 65 to 79, push 32 to 34, wait 0, 9.0 to 10.2 fps | step 44 to 63, push 4 to 12, wait 20 to 29, rows 194 to 303, 10.6 to 13.0 fps |
| Gears, `*delay: 10000` (now registered) | | step 46 to 65, push 5 to 14, wait 0 to 6, rows 184 to 290, 12.6 to 17.6 fps (about 14.5 on average) |
| CubicGrid | step 46.7, push 32, rows 466 | step 47.7, push 32, rows 466 |

- **The work shrinks a lot and the frame rate barely follows**, because both
  hacks ask for a pause after each frame (`*delay` 40 ms for Morph3D, 30 ms for
  Gears) and the loop only credits the push against it. Morph3D's frame is now
  step plus a 40 ms pause. Shortening the pause is a per-hack registry override.
  Morph3D now registers `*delay: 10000`: 10 ms instead of 40 gives about
  14.6 fps, 75 percent over the 8.4 fps it started the evening at. Gears now
  registers the same 10 ms: its first layout went from 9 to 10 fps to about
  14.5, with the work unchanged. A heavy Gears layout is render-bound and the
  pause makes no difference to it.
- **CubicGrid pays about 1 ms (2 percent)** for the bookkeeping on every point,
  and gains nothing, because its points span the whole screen.
- **Correctness:** the pinned frame hash of every hack is unchanged, so the
  output is pixel-identical, and the shadow-display replay in `test_push_present`
  still passes, so no changed row went unmarked. New tests in `test_glshim`
  cover a moving box, depth, a new clear colour and the first frame.
- The cost model's clear and push constants (24.6 and 32 ms) assume a full
  canvas; with this change they scale by the percentages above.

## Overnight capture (issue #50)

On 2026-10-10 from 01:00 to 03:14, from `main` at 4ddb0a5 (dirty rectangle, 10 ms
pauses for Morph3D and Gears, epsilon fix), rotating every 10 seconds: 795
rotations, 23 laps of the 35 hacks, no resets or panics.

**Leaks.** PSRAM was identical (7,327,227 bytes) on all 23 visits to Morph3D,
CubicGrid and Gears. Free internal heap on entering a hack fell by 388 bytes a
lap on average after lap 2 (range 36 to 1,364). Each lap's fall tracks Gears'
own per-visit variation exactly (correlation 1.00, standard deviation 403 bytes
in both), while Morph3D's per-visit variation is 0.0 and CubicGrid's 1.7 bytes.
So the drift is Gears' known upstream leak (`free_gears` never frees
`bp->gears`), and Morph3D and CubicGrid leak nothing measurable.

**Morph3D's shapes.** Morph3D has five shapes, with 2,300, 3,864, 5,040, 5,100
and 7,200 vertices (the eight-seed survey in "The comparison" missed the 2,300
one; 30 seeds found all five). Its 23 starts fell into four clusters of step
time. 5,040 and 5,100 cannot be told apart, and step must rise with vertex
count, so the clusters map onto the sizes in order:

| Shape (vertices) | Starts | Step | fps | Push | Rows sent |
| --- | --- | --- | --- | --- | --- |
| 2,300 | 7 | 20 to 24 ms | 28.0 to 31.2 | 6.3 to 7.2 ms | 105 to 125 |
| 3,864 | 6 | 38 to 41 ms | 19.0 to 20.0 | 7.8 to 8.8 ms | 140 to 155 |
| 5,040 or 5,100 | 7 | 52 to 59 ms | 14.2 to 15.8 | 8.0 to 9.1 ms | 140 to 162 |
| 7,200 | 3 | 74 to 75 ms | 11.6 to 11.8 | 8.2 to 8.7 ms | 143 to 151 |

Adding back the clear and epsilon savings (22.6 ms and 0.7 us a vertex) gives
the steps those shapes would have had before this work: about 45, 65, 81 and
102 ms. Two were measured directly then: 48 ms (the 2,300 shape) and 78 to
82 ms (the 5,040 or 5,100 shape), which is how the earlier mismatch shows. With
a light term for each enabled light the model predicts 48, 66, 80 or 82 and
102 ms (within 2 percent for four shapes and 7 for the 2,300 one).

The earlier claim here that the middle shapes had never been seen was wrong the
other way round: the 78 to 82 ms starts were them, and the shapes not yet seen
were the 3,864 and 7,200 vertex ones, which have now appeared.

**Other hacks, final code.** CubicGrid (37 windows) 11.2 to 14.0 fps, step 38
to 53 ms (it falls as the grid turns), push 32 to 36 ms, 464 to 466 rows.
Gears (29 windows, many layouts) 4.0 to 19.8 fps, step 39 to 241 ms, push 6 to
24 ms, 135 to 352 rows.

## What this does not tell us

- **Only Gears is calibrated**, although three hacks now agree with it. The fit
  used one heavy and one light scene, both made of many small triangles (15 to
  30 pixels each). The fill figure of 570 ns a pixel absorbs per-triangle cost,
  so a scene of few large triangles is likely over-predicted.
- **The constants predate the clip-epsilon fix** (#43). That fix took about
  0.7 us a vertex off the device (CubicGrid's step fell 5.6 ms at 8,000
  points), so on a build with it the model's vertex cost is about that high
  until it is refitted.
- **Lighting cost is per enabled light**, and the model now charges it so
  (`light_terms`); the 2.8 us a light was measured with a directional light and
  no specular, so a hack with positional lights or specular is likely to cost
  more.
- **Lines and points have no cost of their own** beyond vertex setup.
  cubicgrid is entirely points, so its figure is an optimistic floor.
- **Wireframe or point polygon modes** still count the full triangle area as
  fill, so they would be over-predicted.
- **The `ticks` figures are hand arithmetic.** The tool cannot apply a
  `HackEntry` override yet, so they cannot be checked with it.
- **Host `random()` layouts.** Hacks that pick a layout at start (Gears,
  morph3d) vary a lot by seed; use several seeds, as the default does.
- **No PSRAM contention, no flash cache misses**, and nothing is measured on
  the device. Flash the winner and compare, then refit.
