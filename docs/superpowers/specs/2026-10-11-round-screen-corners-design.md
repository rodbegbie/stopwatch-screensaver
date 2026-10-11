# Skip the round screen's corners (issue #77)

## Intent

The StopWatch panel is round. The four corners of the 466 by 466 canvas
(about 21% of its pixels) are never visible, yet every full push, every
GL clear and every GL fill still spends time on them. Rod wants to squeeze
out what we can across the board. The full-screen hacks that sag matter
most: HexTrail at count 10, the GL hacks, and the dropped Hypnowheel and
Boing. No particular frame rate is the target; the aim is the gain.

## Goals

- Push only the visible disc of each dirty row, grouped into the cheapest
  bands.
- Clear only the disc in `glshim_clear` (colour and depth).
- Rasterise only the disc in TinyGL's fill (and lines and points, if
  cheap to add).
- Measure each layer on the board against the build before it.

## Non-goals

- Rescuing Hypnowheel or Boing. If one becomes viable, report it; do not
  chase it here.
- Changing how hacks draw, or touching copied hack sources.
- Overlapping the push with rendering (a separate idea in `gl.md`).

## Anti-goals

- Anything visible on the screen changing: a missing edge pixel, a seam,
  an overlay clipped.
- A slower push for sparse hacks (Pyro, Maze): the plan must never pick
  something the cost model rates worse than today's plan.
- Unknown Pleasures' TE-synced push tearing again.

## Decisions Rod made

- Junk in the canvas corners is fine. GL hacks may leave stale or undrawn
  pixels there. Tests that pin GL frame hashes move to hashing only the
  disc.
- This is a runner and display change every hack sees. Rod asked for it
  (issue #77) and chose "across the board" over targeting a few hacks.
- Flashing the connected board as often as is helpful is approved for this
  work.

## The disc

- Centre (233, 233) in canvas pixels, radius 233.
- A pixel counts as visible if any part of it could fall inside the
  circle. That errs towards pushing a pixel too many, never one too few.
  On the device, compare a frame's edge against today's build.
- One table of per-row visible spans `[x0, x1]`, computed once, shared by
  the push, the clear and the fill. At GL scale 2 the TinyGL buffer is
  233 by 233: use a span table for that size, or derive it so that each
  half-res pixel is drawn if any of its 2 by 2 canvas block is visible.
- The host test canvas sizes differ from 466. The disc follows the canvas
  size (centre `w/2, h/2`, radius `min(w, h)/2`), so tests can use small
  canvases. Check whether any test relies on corner pixels being pushed.

## Layer 1: the push

- `push_plan` clips each dirty row's span to the disc before grouping. A
  row whose dirty span lies wholly in a corner is dropped.
- Grouping: replace the greedy merge with the cheapest split of the dirty
  rows into consecutive groups under the existing cost model
  (`group_cost`). Rows in a group stay contiguous in y order, as now.
  Check the planning cost on the device: an O(n^2) search over 466 rows
  may cost more than it saves; a bounded window or a smarter greedy is
  fine if it gets close to the optimum.
- `PUSH_ALL` (one `pushImage` of the whole canvas) stays as an option
  only when the model says it is cheaper than the disc plan. For a full
  frame the model predicts 31.1 ms for `PUSH_ALL` against 26.8 ms for
  about 19 bands and 28.2 ms for today's greedy grouping of clipped rows.
- `test_push_present` replays every hack into a shadow display. Change its
  comparison to the disc, and keep it catching a missed mark.
- Overlays: the QR is the largest square inside the circle and the badge
  and text are central. Confirm none of them is clipped.
- Hacks with `sync_to_refresh` (only Unknown Pleasures) keep today's
  push unchanged: whole rows, today's grouping, `PUSH_ALL` for a full
  frame. Their TE offset was swept for that shape. Rod's call: more UP
  work is due under #68, so it is exempt for now, with a note on #68.
- The cost model was fitted to Pyro shapes. If the measured full-frame
  push differs from the prediction by more than a millisecond or so,
  stop and bring the numbers before tuning the model.

## Layer 2: the GL clear

- `glshim_clear` fills each row of its box only where the box meets the
  disc. Same for the depth clear (`clear_depth`).
- The dirty boxes stay rectangles; the push clips them to the disc anyway.
- `double_into_canvas` (scale 2) may skip the corners too.

## Layer 3: the GL fill

- Clip TinyGL's triangle fill to the disc, through `tools/tinygl_patch.py`
  (TinyGL stays byte-identical in `src/tinygl/`). The fill wrapper
  `gl_draw_triangle_fill` and the per-scanline loop in `ztriangle.h` are
  the places to look; clipping each scanline to the row's span is the
  natural cut.
- Lines and points: clip if it is cheap. A point or line in a corner is
  never seen.
- `tools/gl_budget.py` uses the same patched tree; its fill counts should
  follow, or say why not.
- Measure with `-DGL_PROFILE` and `tools/ab_flash.py` on Gears' boot
  layout and CubicGrid. Report the fill stage before and after.

## Measuring

- Each layer gets its own A/B on the board, the build before against the
  build after, alternated, at least two rounds.
- Push: a full-screen hack pinned (HexTrail at count 10, or Gears),
  `push=` from the second 5 s window on, and a sparse one (Pyro) to show
  no regression.
- Report fps mean and worst window, `push=`, `step=`, and for GL the
  `glprof` stages. Report "compiles", "host tests pass" and "runs on the
  device" separately.
- Rod confirms the screen at the end of the work, not before (he is away
  meanwhile): edges intact, overlays intact, and the brightness dip
  (#68) on a full-screen hack, today's build against the disc build. A
  change in the dip counts as a visible change (Rod's call).

## Calls made while writing this (Claude's, not Rod's)

- A layer that measures neutral or worse on the board is dropped, not
  kept behind a flag. Stop and bring the numbers to Rod before dropping
  it.
- Noise threshold for "no regression" on a sparse hack: mean `push=`
  within 0.3 ms over two alternated rounds.
- Each layer is its own commit (or commits), so any one can be reverted.
- Existing corner tests (`test_push_plan`'s two-corner-pixel test,
  `test_glshim`'s corner triangle) move inside the disc and keep what
  they test. Add a test that the QR square lies inside the disc.

## Left to the builder

- How the span table is stored (static table at the canvas size, or
  computed on demand) and where it lives (`core/`).
- Exact grouping algorithm, within the "never worse than today" rule.
- Whether lines and points are worth clipping.
- Order of commits, as long as each layer is measured separately.
