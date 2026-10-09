# Shim: thick lines, arcs and writable pixmaps Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use
> superpowers:subagent-driven-development (recommended) or
> superpowers:executing-plans to implement this plan task-by-task. Steps use
> checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the X11 shim honour line width, cap and join styles, draw
arcs (full and partial), and let `XCopyArea` write into pixmaps, without
changing any hack that does not ask for these.

**Architecture:** GC attributes live on `struct XshimGC`. A new
`x11shim/stroke.c` turns a wide segment or polyline into rectangle, quad,
disc and join fills through the canvas primitives, and `xshim.c` calls it only
when the width is above 1. Width 0 or 1 keeps the current code path. Pixmaps
gain a constructor and a destination path in `XCopyArea`, in `pixmap.c`.

**Tech Stack:** C11, PlatformIO native env, Unity, AddressSanitizer.

**Spec:** `docs/superpowers/specs/2026-10-08-shim-thick-lines-arcs-pixmaps-design.md`

## Global Constraints

- Width 0 or 1 draws exactly what `main` draws today (same pixels, same code
  path, no added work per call beyond one comparison).
- Clip and size arithmetic uses `int64_t`; the device's `long` is 32 bits.
- Every canvas write goes through `canvas_clear`, `canvas_point`,
  `canvas_line`, `canvas_fill_rect`, `canvas_fill_polygon`,
  `canvas_fill_ellipse` or `canvas_paste_rect`, or calls
  `canvas_mark_dirty`, so the dirty-row push stays correct.
- Arc angles are in 64ths of a degree; single-precision maths only
  (`float`, `sinf`, `cosf`), because the S3's FPU is single-precision.
- Defaults are width 0, `CapButt`, `JoinMiter`, `LineSolid`, as in Xlib.
- Xlib geometry: a width-`w` line puts `w` pixels across, starting `w / 2`
  (integer division) before the centre pixel; projecting caps extend `w / 2`
  past each end; the miter limit falls back to bevel when the miter length
  exceeds 10 times the width.
- Shim code is ours, MIT. Copied hacks stay byte-identical (`cmp` against
  `vendor/`).
- Commits: named paths with `git add`, end with
  `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.
- Run `source tools/env.sh` first from the repo root (in a worktree use the
  commands in AGENTS.md "Gotchas"); `NO_COLOR=1` on `pio` output.
- Flashing needs Rod's say-so in this conversation, every time.

## Review Focus

- A zero-length line (`x1==x2`, `y1==y2`) with round caps draws a dot; with
  butt caps it draws nothing wider than Xlib does (a single point at width 1).
- Huge widths and coordinates (`INT_MAX`, `-INT_MAX`) neither overflow nor
  hang; work is bounded by the canvas.
- A segment partly or wholly off the canvas clips cleanly and marks only the
  rows it touched.
- `XDrawLines` with `n` of 0, 1 or 2, repeated points, and a path that
  doubles back (a 180 degree turn) draws no gaps and no crash.
- `XCreatePixmap` with zero or absurd sizes returns `None`; `XCopyArea`
  between pixmaps of different depth does nothing; copies clip at both edges.

## File structure

- Create `firmware/src/x11shim/stroke.h`, `stroke.c`: wide segments, caps,
  joins, polylines.
- Create `firmware/src/x11shim/arc.h`, `arc.c`: arc point generation.
- Modify `firmware/src/x11shim/xshim.h`: GC fields, constants, new
  prototypes, comments that are no longer true.
- Modify `firmware/src/x11shim/xshim.c`: GC setters, route draw calls.
- Modify `firmware/src/x11shim/pixmap.c`, `pixmap.h`: `XCreatePixmap`, pixmap
  destination in `XCopyArea`.
- Tests: `firmware/test/test_xshim/test_xshim.c` (shim behaviour) and
  `firmware/test/test_hacks/test_hacks.c` (frame hashes).

## Task 0: Worktree and baseline

**Files:**

- Modify: `firmware/test/test_hacks/test_hacks.c`

**Interfaces:**

- Produces: `test_frames_of_every_hack_but_maze_match_main` and
  `test_maze_frame_matches_main` (hash tests used by every later task).

- [ ] **Step 1: Create the worktree** at `.claude/worktrees/shim-thick-lines`
  on branch `feature/shim-thick-lines-arcs-pixmaps` (it carries the spec).
  Symlink `vendor/`, `.venv/` and `.platformio/` from the main checkout.
- [ ] **Step 2: Check determinism.** Write a helper in `test_hacks.c`,
  `static uint64_t hash_after(int index, int frames)`: `srandom(1)`, run the
  hack through `runner_create`/`runner_start`/`runner_step` for `frames`
  frames (use 200), return `frame_hash()`. A temporary test runs it twice per
  hack and asserts the hashes match; list any hack that differs and exclude
  it from pinning with a comment naming why (for example it reads the clock).
- [ ] **Step 3: Pin the baseline on unmodified code.** Add a
  `static const uint64_t kBaseline[]` indexed by `g_hacks[]` position.
  Fill it by temporarily asserting `TEST_ASSERT_EQUAL_UINT64(0, h)` and copying
  the printed values. Maze's entry lives in its own test
  (`test_maze_frame_matches_main`) so Task 8 can change it on purpose. Add
  `test_frames_of_every_hack_but_maze_match_main` (loops, skips Maze and the
  non-deterministic ones).
- [ ] **Step 4: Run** `pio test -e native -f test_hacks`. Expected: PASS.
- [ ] **Step 5: Mutation check.** Change one `canvas_point` colour in
  `canvas.c` temporarily; both tests fail; revert.
- [ ] **Step 6: Record Maze's "before" frame:** `pio run -e dump`, then
  `.pio/build/dump/program 24 3000 /tmp/maze-before.raw` (confirm index 24 is
  Maze in `registry.c`), convert with `tools/rgb565_to_png.py`.
- [ ] **Step 7: Commit** `test_hacks.c`:
  "Pin every hack's frame before the shim changes".

## Task 1: GC attributes and constants

**Files:**

- Modify: `firmware/src/x11shim/xshim.h`, `firmware/src/x11shim/xshim.c`
- Test: `firmware/test/test_xshim/test_xshim.c`

**Interfaces:**

- Produces: `struct XshimGC` gains `int line_width, cap_style, join_style`.
  `int XSetLineAttributes(Display *, GC, unsigned int width, int line_style,
  int cap_style, int join_style)`. Constants `LineSolid`, `CapNotLast`,
  `CapButt`, `CapRound`, `CapProjecting`, `JoinMiter`, `JoinRound`,
  `JoinBevel`, `Convex`, `Nonconvex`, `GCCapStyle`, `GCJoinStyle`, with the
  real Xlib values (`CapNotLast 0, CapButt 1, CapRound 2, CapProjecting 3`;
  `JoinMiter 0, JoinRound 1, JoinBevel 2`; `GCCapStyle (1L<<6)`,
  `GCJoinStyle (1L<<7)`). `XGCValues` gains `cap_style` and `join_style`.

- [ ] **Step 1: Write failing tests:**
  `test_gc_defaults_are_width_0_butt_miter`,
  `test_create_gc_reads_line_width_cap_and_join_from_the_mask`,
  `test_change_gc_updates_only_the_masked_line_fields`,
  `test_set_line_attributes_sets_all_three`. Assert on the three fields.
- [ ] **Step 2: Run** `pio test -e native -f test_xshim`; expect compile
  failure or FAIL.
- [ ] **Step 3: Implement** in `xshim.h`/`xshim.c`. Defaults are stored
  explicitly, not left as zero (`CapButt` is 1). Update the stale comments on
  `XGCValues` and `XChangeGC`.
- [ ] **Step 4: Run all native tests.** Expect PASS, and the Task 0 hash tests
  unchanged.
- [ ] **Step 5: Survey.** Temporarily make `XSetLineAttributes` and
  `XCreateGC` record the largest width seen in a global; after each hack runs
  200 frames in a throwaway test, print it via the assert trick. Write the list
  of hacks with width above 1 in the commit message (expected: Maze; Blaster
  and XSpirograph only if their Retina branches run). Remove the global.
- [ ] **Step 6: Commit:** "Shim: keep line width, cap and join on the GC".

## Task 2: Wide segments and caps

**Files:**

- Create: `firmware/src/x11shim/stroke.h`, `firmware/src/x11shim/stroke.c`
- Modify: `firmware/src/x11shim/xshim.c` (`XDrawLine`)
- Test: `firmware/test/test_xshim/test_xshim.c`

**Interfaces:**

- Consumes: Task 1's GC fields.
- Produces: in `stroke.h`,
  `void stroke_segment(Canvas *c, const struct XshimGC *gc, int x1, int y1,
  int x2, int y2)` draws one segment of the GC's width with its cap style
  (for a polyline's inner segments Task 3 calls an internal variant with butt
  ends; expose it as `void stroke_segment_ends(Canvas *c, const struct
  XshimGC *gc, int x1, int y1, int x2, int y2, int cap_start, int cap_end)`
  where each flag is a cap style or `CapButt`).

- [ ] **Step 1: Write failing tests** on a 64 by 64 canvas, counting set
  pixels and probing named ones:
  `test_wide_horizontal_line_butt_caps` (width 3, (10,10)-(20,10): 33 pixels,
  rows 9 to 11, columns 10 to 20), `test_wide_even_width_starts_half_before`
  (width 2: rows 9 and 10, 22 pixels), `test_projecting_caps_extend_half_the_
  width` (width 3: 39 pixels), `test_round_caps_width_6_have_tip_but_no_corner`
  ((x1-2,y) set, (x1-3,y-3) unset), `test_diagonal_wide_line_covers_the_centre_
  line_and_is_about_width_thick` (width 4, (5,5)-(40,25): every Bresenham
  pixel set; set count within 15 percent of length times width),
  `test_zero_length_wide_line_round_cap_is_a_dot_butt_is_a_point`,
  `test_wide_line_far_off_canvas_draws_nothing_and_does_not_overflow` (use
  `INT_MAX`), `test_wide_line_clips_at_the_canvas_edge_and_marks_dirty_rows`
  (check `canvas_dirty_row`), and `test_width_0_and_1_match_the_old_line`
  (compare with a `canvas_line` canvas, pixel for pixel).
- [ ] **Step 2: Run;** expect FAIL.
- [ ] **Step 3: Implement** `stroke_segment_ends`. Axis-aligned: one
  `canvas_fill_rect` including caps. Otherwise: build the quad from the
  segment's unit normal (`float`), fill with `canvas_fill_polygon`; projecting
  extends the quad along the direction; round adds
  `canvas_fill_ellipse` of diameter `width` centred on the end point. Sums in
  `int64_t`, clamped as `clamp_coord` does.
- [ ] **Step 4: Route** `XDrawLine`: width above 1 calls `stroke_segment`,
  otherwise the existing `canvas_line`.
- [ ] **Step 5: Run all native tests;** expect PASS, hash tests unchanged
  except possibly Maze (leave Task 0's Maze test failing until Task 7 if it
  does; note it in the commit).
- [ ] **Step 6: Mutation checks,** each failing a named test then reverted:
  drop the projecting extension; swap which side of centre the extra pixel of
  an even width goes; drop the round disc; remove the `INT_MAX` clamp.
- [ ] **Step 7: Commit:** "Shim: draw wide lines with butt, round and
  projecting caps".

## Task 3: Joins and polylines

**Files:**

- Modify: `firmware/src/x11shim/stroke.h`, `stroke.c`, `xshim.c`
  (`XDrawLines`, `XDrawRectangle`)
- Test: `firmware/test/test_xshim/test_xshim.c`

**Interfaces:**

- Consumes: `stroke_segment_ends`.
- Produces: `void stroke_polyline(Canvas *c, const struct XshimGC *gc, const
  XPoint *pts, int n, int closed)`: segments with the GC's joins between
  them, caps only at the two ends (none when `closed`).

- [ ] **Step 1: Write failing tests:** `test_miter_join_fills_the_outer_
  corner` (width 4, right angle: the outer corner pixel is set),
  `test_bevel_join_leaves_the_outer_corner_unset`,
  `test_round_join_fills_a_disc_at_the_vertex`,
  `test_sharp_angle_miter_falls_back_to_bevel` (turn of 5 degrees: no spike
  longer than 10 times the width), `test_polyline_with_two_points_equals_a_
  segment`, `test_polyline_n_0_and_1_draw_nothing_wide`,
  `test_repeated_points_do_not_crash`, `test_180_degree_turn_has_no_gap`,
  `test_draw_rectangle_wide_has_mitered_corners` (width 3 default miter: the
  four outer corners set), `test_width_1_polyline_matches_old_draw_lines`.
- [ ] **Step 2: Run;** expect FAIL.
- [ ] **Step 3: Implement** `stroke_polyline` and route `XDrawLines` (only for
  width above 1) and `XDrawRectangle` (as a closed four-point path with the
  `(w+1) by (h+1)` extent it has now). Skip zero-length inner segments.
- [ ] **Step 4: Run all native tests;** PASS.
- [ ] **Step 5: Mutation checks:** raise the miter limit to 1000; make bevel
  fill the miter; skip round joins; each fails a named test; revert.
- [ ] **Step 6: Commit:** "Shim: miter, bevel and round joins for wide
  polylines".

## Task 4: Segments

**Files:**

- Modify: `firmware/src/x11shim/xshim.h`, `xshim.c`
- Test: `firmware/test/test_xshim/test_xshim.c`

**Interfaces:**

- Produces: `typedef struct { short x1, y1, x2, y2; } XSegment;`
  `int XDrawSegments(Display *, Drawable, GC, XSegment *segs, int n)`: each
  segment drawn independently, caps on both ends.

- [ ] **Step 1: Write failing tests:**
  `test_draw_segments_draws_each_segment_independently`,
  `test_draw_segments_with_zero_count_draws_nothing`,
  `test_draw_segments_wide_uses_caps_not_joins`.
- [ ] **Step 2: Run;** FAIL. **Step 3: Implement** via `XDrawLine`.
  **Step 4: Run;** PASS.
- [ ] **Step 5: Commit:** "Shim: XSegment and XDrawSegments".

## Task 5: Arcs

**Files:**

- Create: `firmware/src/x11shim/arc.h`, `arc.c`
- Modify: `firmware/src/x11shim/xshim.h`, `xshim.c` (`XDrawArc`, `XFillArc`,
  `XFillArcs`, new `XDrawArcs`)
- Test: `firmware/test/test_xshim/test_xshim.c`

**Interfaces:**

- Produces: `int arc_points(int x, int y, unsigned w, unsigned h, int angle1,
  int angle2, XPoint *out, int max)` returns the number of points on the
  ellipse outline from `angle1` through `angle1 + angle2` (64ths of a
  degree, counter-clockwise from three o'clock, y up as in X), never more
  than `max`. `int XDrawArc(Display *, Drawable, GC, int x, int y, unsigned
  int w, unsigned int h, int angle1, int angle2)` and `int XDrawArcs(Display
  *, Drawable, GC, XArc *, int n)`. `XFillArc` fills the pie slice for partial
  arcs (centre plus arc points) and keeps the existing fast path when
  `angle2 >= 360 * 64`.

- [ ] **Step 1: Write failing tests:**
  `test_arc_points_lie_within_a_pixel_of_the_ellipse` (three sizes, compared
  with a `double` reference computed in the test), `test_arc_points_start_
  and_end_at_the_requested_angles`, `test_negative_angle2_runs_clockwise`,
  `test_angle2_above_360_degrees_is_a_full_ellipse`, `test_draw_arc_quarter_
  draws_only_that_quadrant`, `test_draw_arc_wide_uses_the_stroke_code`
  (width 3: more pixels than width 1), `test_fill_arc_half_fills_a_half_disc`,
  `test_fill_arc_full_still_matches_the_old_ellipse` (pixel for pixel),
  `test_zero_size_arc_draws_nothing`, `test_huge_arc_size_does_not_hang`.
- [ ] **Step 2: Run;** FAIL. **Step 3: Implement** `arc_points` with `sinf`
  and `cosf` and a step sized to the larger radius; route the draw calls.
  **Step 4: Run;** PASS, hash tests unchanged (Blaster draws only full
  ellipses).
- [ ] **Step 5: Mutation checks:** swap sin and cos; flip the direction; skip
  the pie centre point; revert after each.
- [ ] **Step 6: Commit:** "Shim: XDrawArc and partial-arc XFillArc".

## Task 6: Writable pixmaps

**Files:**

- Modify: `firmware/src/x11shim/pixmap.c`, `pixmap.h`, `xshim.h`
- Test: `firmware/test/test_xshim/test_xshim.c`

**Interfaces:**

- Produces: `Pixmap XCreatePixmap(Display *, Drawable, unsigned int w,
  unsigned int h, unsigned int depth)`: depth 1 gives a bitmap, any other
  depth an RGB565 pixmap; zero-filled; `None` for a zero size or one that
  does not allocate (w * h above 4 million pixels counts as too big).
  `XCopyArea` now also accepts a pixmap destination when source and
  destination have the same depth: depth 16 copies pixels, depth 1 copies
  bits (colours ignored). It honours the GC clip mask and clips to both
  pixmaps. Window sources are still not supported.

- [ ] **Step 1: Write failing tests:** `test_create_pixmap_is_zeroed_and_
  reports_its_geometry`, `test_create_pixmap_rejects_zero_and_huge_sizes`,
  `test_copy_area_into_a_colour_pixmap_then_out_to_the_canvas`,
  `test_copy_area_into_a_bitmap_copies_bits`, `test_copy_area_between_
  different_depths_does_nothing`, `test_copy_area_clips_at_destination_
  edges`, `test_copy_area_with_a_clip_mask_skips_masked_pixels`,
  `test_scale_pixmap_loop_like_pacman_fills_the_destination` (the same
  1-pixel-column and 1-pixel-row `XCopyArea` loops `scale_pixmap` uses, 4 by
  4 to 8 by 8, checking every destination pixel), `test_drawing_primitive_
  with_a_pixmap_drawable_still_draws_to_the_canvas` (pins current behaviour).
- [ ] **Step 2: Run;** FAIL. **Step 3: Implement,** reusing `pixmap_new`;
  split `copy_pixels` into a source reader and a destination writer.
  **Step 4: Run;** PASS, with ASan clean.
- [ ] **Step 5: Mutation checks:** ignore the depth check; skip the
  destination clip; drop the clip mask; revert after each.
- [ ] **Step 6: Commit:** "Shim: XCreatePixmap and XCopyArea into pixmaps".

## Task 7: Maze before and after, and the whole suite

**Files:**

- Modify: `firmware/test/test_hacks/test_hacks.c`

- [ ] **Step 1: Run the whole native suite:** `pio test -e native`; all pass
  except possibly `test_maze_frame_matches_main`.
- [ ] **Step 2: Dump Maze after:** `pio run -e dump`, then
  `.pio/build/dump/program 24 3000 /tmp/maze-after.raw`, convert to PNG, and
  build a side-by-side with a small `uv run` pillow script (before from
  Task 0). Check Maze by name and index. Show Rod the pair.
- [ ] **Step 3: If the only differences are thicker walls,** update Maze's
  pinned hash and say so in the commit; every other hack's hash must be
  untouched. If any other hack differs, stop and report it.
- [ ] **Step 4: Timing bound.** `time` the dump of 3000 frames of Maze on
  `main` (before binary from Task 0) and now; report both. It is a bound, not
  a device number.
- [ ] **Step 5: Build for the device:** `pio run -e stopwatch` compiles;
  check stack use of the new functions with `cc -O1 -fstack-usage` flags from
  AGENTS.md; the quad and arc buffers stay small (no function above 1 KB).
- [ ] **Step 6: Commit:** "Maze draws 2-pixel walls; pin its new frame".

## Task 8: Docs, assessment, delivery

**Files:**

- Modify: `firmware/src/x11shim/xshim.h` (comments), `AGENTS.md`,
  `tools/assessment_measured.md`, `docs/porting-assessment.md` (generated)

- [ ] **Step 1: Fix stale comments** in `xshim.h`: the pixmap comment, the
  `XChangeGC` comment, the line-width note on `XGCValues`.
- [ ] **Step 2: AGENTS.md:** replace the pixmap sentence in "Gotchas" (pixmaps
  are no longer read-only for `XCopyArea`) and add the line-width rule (width
  above 1 goes through `stroke.c`; width 0 or 1 is the old path).
- [ ] **Step 3: Regenerate the assessment:** `uv run tools/score_hacks.py`;
  expect fewer listed gaps for L rows; read the diff.
- [ ] **Step 4: Lint:** `markdownlint` from the repo root on each changed
  Markdown file; `uv run tools/check_notices.py`;
  `uv run --with pytest --with pillow pytest tools/tests`.
- [ ] **Step 5: Ask Rod before flashing.** With approval: pin Maze with
  `-DSTART_HACK=\"maze\" -DROTATE_SECONDS=0`, capture its serial log from the
  second 5 s window on, and compare `step`, `push` and `rows` with a capture
  of `main` taken the same way. Report "compiles", "host tests pass" and "runs
  on the device" separately; mark what is unmeasured. Add the numbers to
  `tools/assessment_measured.md`. Rebuild without the pin afterwards.
- [ ] **Step 6: Commit** the docs by named path; open the draft PR with
  `entire trail create --base main`, `Fixes` none (no issue), listing what is
  unverified; read the PR body back with `gh pr view`. Ask Rod whether the
  spec and plan files stay in the PR.
