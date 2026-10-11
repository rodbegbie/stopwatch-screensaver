# Round Screen Corners Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use
> superpowers:subagent-driven-development (recommended) or
> superpowers:executing-plans to implement this plan task-by-task. Steps
> use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Stop spending push, clear and fill time on the four corners of
the 466 by 466 canvas that the round panel never shows.

**Architecture:** A canvas can carry a disc: a per-row table of visible
spans. `push_plan` clips dirty spans to it and picks the cheapest banding;
`glshim` clears, and TinyGL fills, only inside it. A canvas without a disc
behaves exactly as today, so host tests and pinned hashes that do not opt
in are unchanged.

**Tech Stack:** C (ESP32-S3, PlatformIO, Unity host tests under ASan),
M5GFX, TinyGL patched by `tools/tinygl_patch.py` (Python 3.13, pytest).

**Spec:** `docs/superpowers/specs/2026-10-11-round-screen-corners-design.md`

## Global Constraints

- Disc: centre `(w/2, h/2)`, radius `min(w, h)/2`; a pixel is visible if
  any part of it could fall inside the circle.
- Hacks with `sync_to_refresh` (Unknown Pleasures) keep today's push
  exactly: whole dirty rows, greedy grouping, `PUSH_ALL` for a full frame.
- The plan must never be rated worse by the cost model than today's
  greedy plan of the same (clipped) rows.
- Copied hacks and `src/tinygl/` stay byte-identical; TinyGL changes go
  through `tools/tinygl_patch.py` with checked anchors.
- A layer that measures neutral or worse on the board is dropped, after
  bringing the numbers to Rod.
- "No regression" on a sparse hack: mean `push=` within 0.3 ms over two
  alternated rounds.
- Report "compiles", "host tests pass" and "runs on the device"
  separately. Rod's eye checks come at the end, all together.
- Commits use named paths and end with the session's trailer.

## Review Focus

1. A dirty span wholly in a corner: the row is dropped, not pushed as an
   empty or inverted span. Test in Task 2.
1. The overlay patches (badge, QR at 328 px, info line) lie inside the
   disc. Test in Task 2.
1. A canvas whose disc rows start at x > 0 still gets its edge pixels:
   the leftmost visible pixel of the widest and narrowest rows is pushed.
   Test in Task 1.
1. Scale 2 GL: a half-res pixel whose 2 by 2 block touches the disc is
   cleared and filled. Test in Task 4.
1. A triangle whose left end is clipped still shades and depth-tests the
   visible pixels exactly as before (interpolants advanced). Test in
   Task 5.

---

### Task 1: The disc on a canvas

**Files:**

- Modify: `firmware/src/core/canvas.h`, `firmware/src/core/canvas.c`
- Test: `firmware/test/test_canvas/test_canvas.c`

**Interfaces:**

- Produces: in `Canvas`, `int16_t *vis_x0, *vis_x1;` (NULL: no disc, every
  pixel visible). `int canvas_set_disc(Canvas *c, void *(*alloc)(size_t));`
  returns 0 or -1, fills both tables. `bool canvas_visible_span(const
  Canvas *c, int y, int *x0, int *x1);` gives row y's visible span
  (whole row without a disc). `canvas_free` frees the tables.

- [ ] **Step 1: Write the failing tests** in `test_canvas.c`:
  `test_no_disc_means_whole_rows` (span of any row is 0..w-1);
  `test_disc_rows_are_symmetric_and_widest_in_the_middle` (466 canvas:
  row y and row 465-y match; rows 232 and 233 span 0..465; row 0 span is
  narrow and centred, x0 + x1 == 465);
  `test_disc_includes_every_pixel_touching_the_circle` (for every pixel of
  a 466 canvas, inside the span iff the nearest point of its square to
  (233, 233) is at distance < 233, computed in `double` in the test);
  `test_disc_follows_a_small_canvas` (16 by 16: row 0 is non-empty, the
  corner pixel (0, 0) is outside).
- [ ] **Step 2: Run** `pio test -e native -f test_canvas`; expect the new
  tests to fail to compile.
- [ ] **Step 3: Implement** `canvas_set_disc` and `canvas_visible_span`.
  Work the span out in integer arithmetic (`int64_t`) on doubled
  coordinates, so host and device agree exactly.
- [ ] **Step 4: Run** `pio test -e native -f test_canvas`; expect PASS.
  Mutation-check: shrink the span by one pixel; the touching test fails.
- [ ] **Step 5: Commit** `canvas.h`, `canvas.c`, `test_canvas.c`.

### Task 2: Clip and band the push

**Files:**

- Modify: `firmware/src/core/push_plan.h`, `firmware/src/core/push_plan.c`
- Modify: `firmware/src/main.cpp` (call site, `canvas_set_disc` at boot)
- Test: `firmware/test/test_push_plan/test_push_plan.c`,
  `firmware/test/test_push_present/test_push_present.c`,
  `firmware/test/test_qr_layout/` (QR inside the disc)

**Interfaces:**

- Consumes: `canvas_visible_span` (Task 1).
- Produces: `PushKind push_plan(Canvas *c, PushRow *rows, int *count,
  bool use_disc);` and `int push_present(Canvas *c, PushRow *scratch,
  const PushSink *sink, bool use_disc);`. `use_disc` false is today's
  behaviour exactly. With it true and a disc on the canvas, spans are
  clipped and rows are banded by the cheapest split; `PUSH_ALL` is
  returned only when cheaper than that split.

- [ ] **Step 1: Update the existing call sites** to pass `false`; run
  `pio test -e native -f test_push_plan`; all existing tests pass
  unchanged (the `false` path is today's).
- [ ] **Step 2: Write the failing tests** in `test_push_plan.c`, on a
  466 canvas with `canvas_set_disc`:
  `test_disc_drops_a_row_dirty_only_in_a_corner` (point at (0, 0):
  PUSH_NONE, count 0);
  `test_disc_clips_a_full_row_to_its_span` (row 0 fully dirty: one row,
  x0/x1 equal to `canvas_visible_span` of row 0);
  `test_disc_full_frame_is_rows_not_all` (fresh canvas: PUSH_ROWS, 466
  rows, more than one group);
  `test_disc_plan_never_costs_more_than_greedy` (for a fresh canvas, a
  checkerboard of dirty rows, and 200 seeded random dirty rectangles,
  the model cost of the disc plan is at most the greedy plan's on the same
  clipped rows; expose `int64_t push_plan_cost(const PushRow *rows, int
  count)` in `push_plan.h` for this);
  `test_without_disc_flag_a_disc_canvas_plans_as_today` (fresh disc
  canvas with `use_disc` false: PUSH_ALL).
- [ ] **Step 3: Run** `pio test -e native -f test_push_plan`; expect the
  new tests to fail.
- [ ] **Step 4: Implement** in `push_plan.c`: clip each dirty span to the
  visible span (drop the row if empty), then group by the cheapest split
  of the dirty rows into consecutive groups under `group_cost`. Use a
  dynamic programme with the lookback limited to 64 dirty rows, keep the
  greedy split as well, and use whichever the model rates cheaper. Then
  compare the total against `PUSH_ALL` as now.
- [ ] **Step 5: Run** `pio test -e native -f test_push_plan`; expect
  PASS. Mutation-check: drop the greedy fallback, then turn the DP
  lookback to 1; the never-costs-more test must catch at least one.
- [ ] **Step 6: Disc in the replay**: in `test_push_present.c`, add the
  disc to the canvas, pass `true`, and compare shadow and canvas only
  inside the disc (`shadow_matches_canvas` walks `canvas_visible_span`).
  Keep a second copy of the every-hack replay with `false` and an exact
  compare, so the synced path stays covered. Run `pio test -e native -f
  test_push_present`; expect PASS. Mutation-check: shrink the clip by one
  pixel at each end and watch the replay fail.
- [ ] **Step 7: Overlays**: test that the QR square from `qr_layout`
  (all four corners), the badge patch and the info patch lie inside the
  disc of a 466 canvas.
- [ ] **Step 8: Wire main.cpp**: `canvas_set_disc(&canvas, ...)` after
  the canvas is allocated (same allocator as the canvas), and
  `push_present(..., !g_hacks[runner_index(runner)]->sync_to_refresh)`.
  `rows=` still counts rows pushed.
- [ ] **Step 9: Full host suite and device build**: `pio test -e native`
  (about 5 min), `pio run -e stopwatch`, `pio run -e dump`.
- [ ] **Step 10: Commit** each of the above as it goes green.

### Task 3: Measure the push on the board

**Files:** none committed except the notes in the PR body.

- [ ] **Step 1:** Build two images, each with its own
  `PLATFORMIO_BUILD_DIR`: `main` (baseline, at `origin/main`) and this
  branch. Each is pinned with `ROTATE_SECONDS=0`.
- [ ] **Step 2:** For HexTrail (full-screen), then Pyro (sparse): flash
  baseline, capture 60 s; flash branch, capture 60 s; repeat once. Use
  `tools/ab_flash.py` if it accepts the build directories, otherwise the
  serial capture script from AGENTS.md.
- [ ] **Step 3:** Tabulate `push=`, `step=`, fps mean and worst window,
  `rows=`, from the second window on. Expect HexTrail's `push=` near
  27 ms against 31; Pyro within 0.3 ms.
- [ ] **Step 4:** If the full-screen push is more than about 1 ms off the
  model's 26.8 ms, stop and bring the numbers to Rod.

### Task 4: Clip the GL clear to the disc

**Files:**

- Modify: `firmware/src/glshim/glshim.c` (and `glshim.h` for the export)
- Test: `firmware/test/test_glshim/test_glshim.c`

**Interfaces:**

- Consumes: `Canvas.vis_x0/vis_x1` (Task 1).
- Produces: `extern const int16_t *glshim_span_x0, *glshim_span_x1;` at
  TinyGL buffer size (NULL when the canvas has no disc). Set in
  `glshim_open`, cleared in `glshim_close`. At scale 2, row y's span is
  the union of canvas rows 2y and 2y+1, halved outwards (x0/2, x1/2).

- [ ] **Step 1: Write the failing tests** (64 canvas with a disc):
  `test_disc_clear_leaves_the_corners_alone` (paint the canvas a sentinel,
  open, clear to black: (0, 0) keeps the sentinel, centre and every
  visible span's ends are black);
  `test_disc_depth_clear_is_clipped_too` (read `zbuf` directly);
  `test_disc_scale_two_clears_every_block_touching_the_disc` (scale 2:
  every canvas pixel in the disc is black after a clear and a swap);
  `test_no_disc_clear_is_unchanged` (existing behaviour: (0, 0) cleared).
- [ ] **Step 2: Run** `pio test -e native -f test_glshim`; expect FAIL.
- [ ] **Step 3: Implement**: in `glshim_clear` and `clear_depth`, fill
  each row of the area intersected with that row's span.
- [ ] **Step 4: Run** `pio test -e native -f test_glshim`; expect PASS.
  Mutation-check the scale-2 rounding (x1/2 without rounding out).
- [ ] **Step 5:** The existing corner-triangle tests use a canvas without
  a disc: leave them. Run the full suite; GL hack hashes in `test_hacks`
  stay pinned, since its canvas has no disc.
- [ ] **Step 6: Measure**: `ab_flash.py --hack Gears` and CubicGrid,
  `-DGL_PROFILE`, before and after; report the `clear` stage.
- [ ] **Step 7: Commit.**

### Task 5: Clip TinyGL's fill to the disc

**Files:**

- Modify: `tools/tinygl_patch.py`, `tools/tests/` (patch test),
  `firmware/test/test_glshim/test_glshim.c`

**Interfaces:**

- Consumes: `glshim_span_x0/x1` (Task 4).

- [ ] **Step 1: Write the failing tests**:
  `test_disc_fill_leaves_the_corners_alone` (a full-screen quad on a
  disc canvas painted with a sentinel: corners keep it, every visible
  pixel is the quad's colour);
  `test_disc_clipped_triangle_matches_the_unclipped_one_inside` (a smooth
  shaded, depth-tested triangle crossing the disc's left edge, drawn on a
  disc canvas and on a plain one: identical inside the disc, pixel for
  pixel); a pytest that the new patch's anchor is found.
- [ ] **Step 2: Run**; expect FAIL.
- [ ] **Step 3: Implement** a `Replace` in `tinygl_patch.py` at the
  scanline setup in `ztriangle.h` (where `n = (x2 >> 16) - x1;` and
  `pp`, `pz` are set): when `glshim_span_x0` is set, clamp the scanline
  to the row's span, advance every interpolant used by the variant (z,
  r, g, b, s, t, sz, tz) by the skipped count times its per-x step, and
  skip the line if nothing is left. The row is
  `(pp1 - pbuf) / linesize`; carry it in a counter instead if cheaper.
- [ ] **Step 4: Run** host tests and `uv run --with pytest --with pillow
  pytest tools/tests`; expect PASS. Mutation-check: drop the left-end
  interpolant advance; the matches-unclipped test must fail.
- [ ] **Step 5:** Lines and points: clip only if it is a one-line check
  in the plot macro; otherwise note it as not done.
- [ ] **Step 6: Measure** Gears and CubicGrid with `GL_PROFILE`, before
  and after; report the `fill` stage and fps. Drop the layer if it does
  not win.
- [ ] **Step 7: Commit.**

### Task 6: Docs, eye checks, delivery

- [ ] **Step 1:** Update `.claude/rules/device-display.md` (disc push,
  the synced exemption) and `gl.md` (clipped clear and fill), and the
  measured numbers.
- [ ] **Step 2:** Leave the board on the branch build for Rod with a
  list: edges intact on a full-screen hack (HexTrail), the badge and QR
  intact, and the brightness dip compared with `main`.
- [ ] **Step 3:** Deliver with the `deliver-branch` skill, dropping the
  spec and plan from the branch tip.
