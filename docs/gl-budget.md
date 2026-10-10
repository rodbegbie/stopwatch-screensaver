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
per vertex, 2.8 us more per lit vertex (both from the Pipes probe in #43), plus
a fill term. The fill term is 570 ns a pixel, fitted so that the heavy Gears
scene (seed 13, 17,700 vertices) costs the 40 ms of fill that #43 measured.

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
| morph3d, four shapes (seeds 1 to 8) | 3,864 to 7,200 | 2,300 to 4,000 | 0 | 87 to 114 ms | 8.8 to 11.5 |
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
| Morph3D, heavy shape | 114 ms | 111 to 118 ms | -3% to +4% |
| Morph3D, light shape | 87 ms | 84 ms | -3% |
| CubicGrid, 27,000 points | 165 ms | 170 to 180 ms | +3% to +9% |
| CubicGrid, 8,000 points | 89 ms | 80 to 88 ms | -10% to -1% |

- **Good to about 10 percent** on these four scenes that were not used in the
  fit, in both directions. It also ranked them correctly, which is the use it
  was built for: choosing which hack to port and what to turn down.
- **The Gears rows are weaker evidence than the rest.** Their "measured"
  figures are the breakdown quoted in issue #43, not a capture made for this
  comparison, and the fill figure was fitted to the heavy one.
- **The device shapes of Morph3D are inferred.** The device does not seed
  `random()`, so a Morph3D start cannot be asked for its shape: the heavy and
  light rows are matched by step time (78 to 82 ms and 48 ms). The two middle
  shapes (5,040 and 5,100 vertices, predicted at about 98 to 100 ms) have not
  appeared in five starts.
- **The worries raised in review did not show up.** The model charges one
  light, and Morph3D has two; its triangles are 3 to 4 pixels where Gears'
  are 15 to 30; CubicGrid's points are drawn 2.5 pixels wide and uncosted. All
  three would have made the model optimistic, and none did on the board:
  Morph3D landed within 4 percent, and CubicGrid's two sizes imply about
  4.7 microseconds a point against the model's 4. They may still bite a hack
  that leans harder on them, so the caveats under "What this does not tell
  us" stand.
- **Use it for ranking and go or no-go, with a margin of 10 percent.** A
  prediction within 10 percent of a frame-rate threshold needs the board to
  settle it. Add each
  new GL port's measured step to the table and refit `pixel_ns` if the errors
  drift in one direction.

## Device check: Morph3D

Flashed on 2026-10-09. The first capture was nine 5 second windows of one
start: 8.0 to 8.4 fps, a step of 78 to 86 ms and a push of 32.6 ms. A second
capture rotated every 10 seconds for 26 minutes (155 rotations, 4 to 5 laps, no
resets or panics) and gave five Morph3D starts: four at 8.2 to 8.4 fps (step 78
to 82 ms) and one at 11.4 fps (step 48 ms). Push was 36 ms on that build.
PSRAM was identical on every visit; the free heap on entering Morph3D fell 308,
36 and 40 bytes on the three laps after the first, which is in line with
Gears' known upstream leak and cannot be attributed to Morph3D.

Against the prediction (step plus push 87 to 114 ms, so a step of 55 to 82 ms):
the 48 ms start sits just under the light shape's 55 ms, and the 78 to 82 ms
starts match the heavy shape's 82 ms. No start landed in the 66 to 68 ms the
two middle shapes (5,040 and 5,100 vertices) predict, so those shapes have not
been seen on the board, and the device does not seed `random()`, so a shape
cannot be asked for. The model held at both ends; its middle is unchecked.

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

## What this does not tell us

- **Only Gears is calibrated**, although three hacks now agree with it. The fit
  used one heavy and one light scene, both made of many small triangles (15 to
  30 pixels each). The fill figure of 570 ns a pixel absorbs per-triangle cost,
  so a scene of few large triangles is likely over-predicted.
- **The constants predate the clip-epsilon fix** (#43). That fix took about
  0.7 us a vertex off the device (CubicGrid's step fell 5.6 ms at 8,000
  points), so on a build with it the model's vertex cost is about that high
  until it is refitted.
- **Lighting cost is per light.** The model charges one flat figure per lit
  vertex, measured with a single light.
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
