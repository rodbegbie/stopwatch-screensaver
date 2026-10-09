# Shim: thick lines, arcs and writable pixmaps

## Purpose

Pacman (the next port, a separate branch) needs three things the X11 shim
lacks. This branch adds them, test first, with no hack added. Pacman can be
dropped later without losing the shim work.

## What exists today

- `line_width` is stored nowhere: `XCreateGC` and `XChangeGC` honour only the
  foreground, and every line is one pixel wide. The field was added in
  commit 06b5db5 "accepted but ignored" to let HyperCube, XSpirograph and
  Petri compile. It was a shortcut, not a decision.
- `XFillArc` draws full ellipses only; partial arcs are ignored. There is no
  `XDrawArc`.
- Pixmaps are read-only sources for `XCopyArea` and `XCopyPlane`. The only
  way to get one is `image_data_to_pixmap`.

## Goals

- Lines honour `line_width`, `cap_style` and `join_style` exactly for the
  styles our hacks use.
- `XDrawArc` and partial arcs, for both `XDrawArc` and `XFillArc`.
- A pixmap can be created and written with `XCopyArea`, which is all Pacman's
  sprite scaling does.
- Every hack that sets no width, or a width of 0 or 1, is unchanged: same
  pixels, same speed.

## Non-goals

- Drawing primitives into a pixmap. Only `XCopyArea` writes to one. A
  primitive given a pixmap as its drawable keeps today's behaviour (it draws
  to the canvas), and a test pins that.
- Dashes, fill styles, stipples, line styles other than `LineSolid`.
- Any change to the runner, the canvas or the push.
- The Pacman port, its sprite blob and its registry entry (branch 2).

## Thick lines

The GC gains `line_width`, `cap_style` and `join_style`, set by `XCreateGC`,
`XChangeGC` (`GCLineWidth`, `GCCapStyle`, `GCJoinStyle`) and
`XSetLineAttributes`. Defaults are width 0, `CapButt`, `JoinMiter`, as in
Xlib.

Width 0 or 1 takes today's code path, untouched. Wider lines:

- **Axis-aligned segments** become a rectangle fill. Maze's walls are
  expected to hit this path; that is checked against the code, not assumed.
- **Other segments** are expanded to a quad (the segment offset by half the
  width on each side) and filled with the existing polygon fill.
- **Caps:** butt adds nothing, projecting extends the quad by half the width,
  round adds a filled disc at each end.
- **Joins** (in `XDrawLines`, `XDrawRectangle`, `XDrawArc` polylines):
  bevel fills the triangle between the two outer corners, round adds a disc,
  and miter fills the corner when the miter ratio is within Xlib's limit and
  falls back to bevel beyond it.
- All coordinate arithmetic uses `int64_t` before clipping, as the clip rules
  require; the device's `long` is 32 bits.
- Every write goes through `canvas_fill_rect`, `hspan` or
  `canvas_fill_polygon`, so the dirty-row tracking from #6 stays correct.

## Arcs

`XDrawArc` and the partial-arc case of `XFillArc` share one routine that walks
the ellipse between `angle1` and `angle1 + angle2` (64ths of a degree). A
full ellipse keeps the existing fast path. The outline goes through the thick
line code when the width is above 1. Angles use single precision, because the
S3's FPU is single-precision only; a test checks the result against a
double-precision reference for a few sizes.

## Writable pixmaps

`XCreatePixmap` allocates a pixmap of depth 1 (a bitmap) or 16 (RGB565) with
the existing `struct XshimPixmap` and plain `malloc`, as the blob loader does
(large blocks reach PSRAM on the device). `XCopyArea` accepts a pixmap as its
destination, for same-depth source and destination, clipped to the destination
and honouring the GC clip mask.
Contents start zeroed. `XFreePixmap` and `XGetGeometry` already handle the
handle type.

## Supporting additions

`XSegment` and `XDrawSegments`, `XSetLineAttributes`, and the constants
`LineSolid`, `CapNotLast`, `CapButt`, `CapRound`, `CapProjecting`,
`JoinMiter`, `JoinRound`, `JoinBevel`, `Convex` and the `GC*` mask bits.

## Testing

Each item is test first in `firmware/test/test_xshim/`, then broken on
purpose to watch the test fail (a cap, a join, the miter limit, the dirty
mark, the destination clip, the depth check).

- **Pixel tests** for each cap and join, at width 2, 3 and 6, on a small
  canvas, by comparing with hand-checked expected pixels.
- **Unchanged hacks:** a hash of the frame for every registered hack except
  Maze, taken on `main` before any change and pinned, after `srandom(1)`.
  Maze's frame is expected to change; the before and after are dumped and
  shown to Rod.
- **`test_push_present`** keeps replaying every hack into a shadow display to
  catch a missed dirty mark.
- **Pixmap tests:** create, copy in, copy out, clip at the edges, depth
  mismatch, free.
- A host timing check for the thick path, to see that nothing is
  pathologically slow. It is a bound, not a device number.

## Verification on the device

Reported separately as compiles, host tests pass, and runs on the device.
Flashing needs Rod's say-so each time.

- Which hacks set a width above 1 is checked by dumping each hack with a
  trace on `XSetLineAttributes` and `GCLineWidth`. Maze is expected; Blaster
  and XSpirograph only if their Retina branches run, which this checks.
- Maze before and after, pinned with `START_HACK` and rotation off: `step`,
  `push` and `rows` from the serial log, read from the second 5 s window on.
- **Pass bar:** no regression in Maze's step or push beyond run-to-run noise.
  If there is one, it is reported, not hidden, and Rod decides.
- Colour is judged on the device by Rod, on a colour known to be right.

## Risks

- Maze's output changes, as it should to match xscreensaver. Rod reviews the
  frames.
- Wider lines mark wider dirty spans, which could raise `push` for hacks that
  use them. Unmeasured until the device run.
- Round caps and joins on very short segments (shorter than the width) need
  care; tests cover a zero-length line, which Xlib draws as a dot for round
  caps.

## Out of scope for later

Dashed lines, stippled fills, drawing primitives into pixmaps, and any
resource-use cost of keeping many pixmaps live. Each becomes an issue if a
future port needs it.
