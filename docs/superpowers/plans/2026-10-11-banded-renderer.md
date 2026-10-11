# Banded GL Renderer Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use
> superpowers:subagent-driven-development (recommended) or
> superpowers:executing-plans to implement this plan task-by-task. Steps use
> checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make Unknown Pleasures and Hypnowheel visibly faster on the board by
rasterising GL frames in horizontal bands held in internal RAM.

**Architecture:** A frame that starts with a qualifying `glClear` is held:
TinyGL still clips, culls, lights and offsets as now, but where it would call
a rasteriser (`ZB_fillTriangle*`, `ZB_line*`, `ZB_plot`) it stores the call
instead. At the swap each band of rows is cleared in internal RAM, every
stored call that touches it is replayed through the same rasteriser against
a band-sized `ZBuffer`, and the band is copied to the canvas. Anything the
held path cannot take spills: the held calls are drawn out (colour and depth)
and the rest of the frame draws directly.

Because held calls replay through TinyGL's own rasterisers with the same
inputs, a held frame matches a direct one pixel for pixel. The tests compare
exactly, which is stricter than the spec's tolerance, and the pinned frame
hashes change only where Task 5 fixes the additive blend's colours.

**Tech Stack:** C (TinyGL patched by `tools/tinygl_patch.py`), `glshim`,
PlatformIO (`native` Unity tests under AddressSanitizer, `stopwatch` board
build), Python 3.13 with pytest for the patch tests, `tools/ab_flash.py` for
board A/B runs.

**Spec:** `docs/superpowers/specs/2026-10-11-banded-renderer-design.md`

## Global Constraints

- Copied hacks stay byte-identical; TinyGL under `firmware/src/tinygl/` stays
  byte-identical. Change TinyGL only through `tools/tinygl_patch.py` with
  checked anchors (add a test in `tools/tests/test_tinygl_patch.py` for each).
- Test first, on the host; mutation-check every new test (AGENTS.md).
- One path for every GL hack; the only per-hack switch is
  `glshim_set_banding(0)`, for hacks that draw against earlier frames' depth
  (Pipes' wrapper).
- Band memory is allocated only while a GL context is open.
- No GL hack may get slower: A/B every registered GL hack on the board
  (Gears, Morph3D, CubicGrid, StonerView, Unknown Pleasures, HexTrail).
- Report "compiles", "host tests pass" and "runs on the device" separately.
  Rod judges the look; stop for him where a task says so.
- Commits: named paths with `git add`, never `-A`; end with
  `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Work on `feature/43-banded-renderer` (branched from
  `feature/43-gl-speed-experiments`, which has `GL_PROFILE`, lazy lighting and
  `tools/ab_flash.py`). Planning docs do not merge to `main`.
- Board numbers to beat (profile builds, lazy lighting on): Unknown Pleasures
  174 ms step; Hypnowheel 706 ms; Gears boot layout 57.7 ms; Morph3D 58.4 ms;
  CubicGrid 47.9 ms.

## Review Focus

1. A frame that clears colour only, with the depth test on, after a held
   frame: the PSRAM z-buffer is stale. Expect the context to stop banding for
   good from that frame on, so later frames match the direct path (Task 3).
1. Internal RAM unavailable at `glshim_open`: expect banding to stay off and
   every frame to draw directly, never a crash (Task 3).
1. A hack stopped between its clear and its swap (rotation or a button
   press): expect the held calls to be dropped at close, and the next hack's
   first swap to replay nothing (Task 3).
1. Text drawn into a held frame (`ztext.c` writes `pbuf` directly): expect a
   spill first, so the text lands over what was drawn before it (Task 3).
1. Opening and closing a context many times: expect no growth in allocated
   bytes (Task 3).

---

### Task 1: Hypnowheel in single precision

**Files:**

- Create: `firmware/src/hacks/hypnowheel_single_gl.c`
- Modify: `firmware/src/hacks/registry.c` (entry under `HYPNOWHEEL_PROBE`)
- Modify: `firmware/patch_float_literals.py` (if the hack has decimal
  literals that `nm` shows as soft-double helpers)

**Interfaces:**

- Produces: `hypnowheel_hack`, registered last in `g_hacks[]` only when built
  with `-DHYPNOWHEEL_PROBE`. Without the flag the registry and every test are
  unchanged.

- [ ] **Step 1: Write the wrapper.** Model it on `hextrail_single_gl.c`:
  `#define USE_GL`, include `xlockmore.h`, `colors.h` and `rotator.h` first,
  define `hypnowheel_get_position` and `hypnowheel_get_rotation` adapters
  (the hack passes `double` locals' addresses at lines 250, 255, 278 and 288),
  then `hacks/single_precision.h` and `hacks/single_frand.h`, then map the two
  names and include the hack (or its `_floatlit.c` copy). Follow the
  "Single precision" recipe in `.claude/skills/port-hack/techniques.md`.
- [ ] **Step 2: Register it under the flag.** Use the overrides it had before
  it was dropped (`fd2bc80`): `{"*ncolors: 64", "*cycles: 0",
  "*delay: 10000", NULL}`, `XLOCKMORE_HACK_WITH(hypnowheel, "Hypnowheel",
  ...)`, inside `#ifdef HYPNOWHEEL_PROBE`, with `&hypnowheel_hack` last in
  `g_hacks[]`. Leave `failed_ports.txt` and the build filters alone; the old
  `hypnowheel_gl.c` stays excluded.
- [ ] **Step 3: Check for soft-double helpers.** Compile the wrapper with
  `-Os -mlongcalls` and the shim's include paths and run
  `xtensa-esp32s3-elf-nm <obj> | grep ' U __.*df'`. Expected: no helper on the
  per-vertex path (`draw_spiral`). If some remain, add the hack to
  `patch_float_literals.py` and include `hypnowheel_floatlit.c`.
- [ ] **Step 4: Compare frames on the host.** Build `-e dump` with
  `PLATFORMIO_BUILD_FLAGS=-DHYPNOWHEEL_PROBE`, dump the last index at 1, 50
  and 200 frames, and do the same with a temporary build of the double
  `hypnowheel_gl.c`. Expected: early frames identical or near-identical, and
  the same look later (convert with `tools/rgb565_to_png.py` and compare).
- [ ] **Step 5: Run the full host suite.**
  Run: `pio test -e native` (from `firmware/`, after `source tools/env.sh`).
  Expected: all pass (the flag is off).
- [ ] **Step 6: Profile on the board.**
  Run: `.venv/bin/python -I tools/ab_flash.py --hack Hypnowheel --seconds 90
  --rounds 1 --out <scratch>/ab-hypno-single --keys
  fps,step,push,clear,vertex,tri,fill "single=-DGL_PROFILE -DHYPNOWHEEL_PROBE"`.
  Expected: the step falls well below 706 ms; about 275 ms of it was the trig.
- [ ] **Step 7: Ask Rod to look.** Pinned Hypnowheel build, rotation off.
- [ ] **Step 8: Commit** the wrapper, the registry change and any literal-pass
  change, with the board numbers in the message.

### Task 2: Rasterisers that stay inside a band

**Files:**

- Modify: `tools/tinygl_patch.py` (three new `Replace` entries)
- Test: `tools/tests/test_tinygl_patch.py`
- Create: `firmware/test/test_glshim_band/test_glshim_band.c`

**Interfaces:**

- Produces: every rasteriser draws only rows `0 <= y < zb->ysize`, whatever
  the `ZBufferPoint` y values (they may be negative or past the end). Task 3
  relies on this: it translates points by the band's first row and sets
  `ysize` to the band height.

- [ ] **Step 1: Write the failing tests** in `test_glshim_band.c`. Each opens a
  `ZBuffer` with `ZB_open(64, 8, ZB_MODE_5R6G5B, rows)` over a 64 by 24 buffer
  whose middle 8 rows are the band, and the 8 rows either side are guards
  filled with a sentinel:
  - `test_a_triangle_taller_than_the_band_fills_only_band_rows`: points at
    y = -5, 3 and 14, filled with `ZB_fillTriangleSmoothNOBLEND`. Assert each
    band row has drawn pixels and every guard pixel still holds the sentinel.
  - `test_a_triangle_wholly_below_the_band_draws_nothing`: y = 9, 12, 15.
  - `test_a_line_crossing_the_band_draws_only_inside_it`: `ZB_line_z` from
    (2, -6) to (60, 13), and the same check with smooth lines on
    (`zb->line_smooth = 1`).
  - `test_a_point_outside_the_band_draws_nothing`: `ZB_plot` at y = -1 and
    y = 8, pointsize 1 and 3.
- [ ] **Step 2: Run them and watch them fail.**
  Run: `pio test -e native -f test_glshim_band`.
  Expected: guard pixels overwritten, or AddressSanitizer reports.
- [ ] **Step 3: Patch the three rasterisers** through `tinygl_patch.py`:
  - `src/ztriangle.h`: keep a row counter from `p0->y`; skip the span draw
    while it is below 0 and leave both parts once it reaches `zb->ysize`.
    Edge stepping continues as now, so pixels inside the band are unchanged.
  - `src/zline.h`: `PUTPIXEL` draws only when `pp` lies within
    `[zb->pbuf, zb->pbuf + zb->ysize * zb->linesize)`.
  - `src/zline.c` `ZB_plot`: skip any pixel with y outside `[0, zb->ysize)`.
  `zb_aa_plot` already checks bounds.
- [ ] **Step 4: Add patch tests** in `test_tinygl_patch.py` asserting each new
  text is present in the patched tree, as the existing tests do.
- [ ] **Step 5: Run everything.** `pio test -e native` and
  `uv run --with pytest --with pillow pytest tools/tests`. Expected: all pass,
  and every pinned frame hash in `test_hacks.c` unchanged.
- [ ] **Step 6: Mutation-check:** remove each of the three guards in turn and
  watch its test fail (scratchpad script, per AGENTS.md).
- [ ] **Step 7: Commit.**

### Task 3: Held frames for triangles, with spill and fallback

**Files:**

- Create: `firmware/src/glshim/glshim_band.h`, `firmware/src/glshim/glshim_band.c`
- Modify: `firmware/src/glshim/glshim.c` (clear, swap, open, close)
- Modify: `firmware/src/glshim/glshim.h` (`glshim_set_banding`)
- Modify: `tools/tinygl_patch.py` (fill call sites in `clip.c`; text and
  `glDrawPixels` spill)
- Test: `firmware/test/test_glshim_band/test_glshim_band.c`,
  `tools/tests/test_tinygl_patch.py`

**Interfaces:**

- Consumes: Task 2's band-safe rasterisers.
- Produces (in `glshim_band.h`):

```c
typedef void (*GlshimFillFn)(ZBuffer *, ZBufferPoint *, ZBufferPoint *,
                             ZBufferPoint *);
int glshim_band_open(int w, int h);  /* 0, or -1 if internal RAM is short */
void glshim_band_close(void);        /* drops any held calls */
void glshim_band_begin(ZBuffer *zb, PIXEL clear_color, GLint clear_z);
int glshim_band_holding(void);
int glshim_band_fill(ZBuffer *zb, GlshimFillFn fn, ZBufferPoint *a,
                     ZBufferPoint *b, ZBufferPoint *c); /* 1 if held */
void glshim_band_spill(ZBuffer *zb);  /* draw held calls, colour and depth */
void glshim_band_finish(ZBuffer *zb, int x0, int y0, int x1, int y1);
void glshim_band_set_capacity(int records); /* tests only */
```

- Produces (in `glshim.h`): `void glshim_set_banding(int on);`, 1 by default,
  reset to 1 by `glshim_close`. Pipes' wrapper will call it with 0.

Decisions the implementer must follow:

- **Record:** 64 bytes on the board. A 4-byte header (kind, rasteriser
  index into a table of the `ZB_fillTriangle*` functions, state index) and
  three vertices of `x`, `y` (int16), `z`, `r`, `g`, `b` (int32). Add
  `_Static_assert(sizeof(GlshimRecord) == 64)` under `__XTENSA__`. A texture
  fill, or any rasteriser not in the table, spills.
- **State table:** the `ZBuffer` fields a rasteriser reads (`enable_blend`,
  `sfactor`, `dfactor`, `blendeq`, `depth_test`, `depth_write`,
  `line_smooth`, `pointsize`), up to 255 distinct states a frame, a new entry
  only when the state differs from the last. Overflow spills. Because records
  are taken after the fill wrapper in `tinygl_patch.py`, the opaque
  `SRC_ALPHA` case and the polygon offset are already applied to what is
  stored.
- **Where things live:** records in PSRAM (`heap_caps_malloc` with
  `MALLOC_CAP_SPIRAM` on the board, `malloc` on the host), capacity 16,384
  (1 MB). A table of each record's first and last row (two `uint16_t`) in
  internal RAM (64 KB). One band of colour and depth, `GLSHIM_BAND_ROWS` = 32
  rows by the canvas width (about 60 KB), in internal RAM
  (`MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT`). If either internal allocation
  fails, `glshim_band_open` returns -1 and banding stays off.
- **Held frames:** `glshim_clear` calls `glshim_band_begin` instead of filling
  when banding is on and the clear covers colour and depth, or colour with the
  depth test off. It still adds the erased box to the swap's dirty box, and
  sets `g_z_valid = false`, because a held frame never writes the PSRAM
  z-buffer.
- **Replay:** for each band that the box `(x0, y0)` to `(x1, y1)` (this
  frame's drawn box joined with the erased box) touches, fill the band with
  the clear colour and depth. Replay, in order, every record whose row range
  meets the band, with y values moved up by the band's first row, through a
  `ZBuffer` copy whose `pbuf`, `zbuf` and `ysize` are the band's. Then copy
  the band's rows inside the box, columns `x0` to `x1`, to the canvas.
- **Spill:** replays the same way, but over the whole canvas box that a
  direct clear would have cleared plus the drawn box, and copies depth as
  well as colour to the PSRAM z-buffer. Then the frame continues directly.
  Spill on: a full store, a full state table, an unknown rasteriser, a text
  pixel (`ztext.c`) or `glDrawPixels`, and lines or points (until Task 4).
- **Sticky fallback:** when a frame draws with the depth test on but without
  a depth clear after a held frame, call `glshim_set_banding(0)` for the rest
  of the context. That one frame draws against stale depth.

- [ ] **Step 1: Write the failing tests** in `test_glshim_band.c`. Each draws
  a scene once with `glshim_set_banding(0)` and once with banding on, from
  `srandom(1)`, and asserts `TEST_ASSERT_EQUAL_MEMORY` on the canvas:
  - `test_a_held_frame_of_overlapping_lit_triangles_matches_direct`
    (depth test, smooth shading, two lights).
  - `test_a_held_frame_with_flat_shading_matches_direct`.
  - `test_a_held_frame_with_additive_blending_matches_direct`
    (`GL_ONE, GL_ONE`, depth test off).
  - `test_a_held_frame_with_polygon_offset_matches_direct`.
  - `test_a_triangle_on_a_band_boundary_matches_direct` (rows 31 and 32).
  - `test_three_frames_of_a_moving_triangle_match_direct` (the erased box).
  - `test_a_frame_that_overflows_the_store_matches_direct`
    (`glshim_band_set_capacity(4)`, 20 triangles).
  - `test_a_frame_without_a_clear_draws_directly` (assert
    `glshim_band_holding() == 0` after drawing).
  - `test_text_in_a_held_frame_lands_over_earlier_triangles` (Review Focus 4).
  - `test_a_colour_only_clear_with_depth_on_after_a_held_frame_stops_banding`
    (Review Focus 1: the frame after that one matches direct).
  - `test_banding_stays_off_when_internal_ram_is_short` (a test hook that makes
    the next band allocation fail; frames still match direct; Review Focus 2).
  - `test_closing_mid_frame_drops_the_held_calls` (clear, draw, close, open,
    swap: canvas unchanged by the old calls; Review Focus 3).
  - `test_opening_and_closing_fifty_times_does_not_grow_memory`
    (`__sanitizer_get_current_allocated_bytes()`; Review Focus 5).
- [ ] **Step 2: Run them and watch them fail** (`-f test_glshim_band`).
- [ ] **Step 3: Implement** `glshim_band.c` and the `glshim.c` hooks, then
  patch the fill calls in `gl_draw_triangle_fill_plain` (`clip.c`) through
  `tinygl_patch.py` to `if (!glshim_band_fill(c->zb, fn, a, b, c)) fn(...)`,
  and add `glshim_band_spill` before `ztext.c`'s pixel write and in
  `glopDrawPixels`.
- [ ] **Step 4: Run all host tests.** Expected: all pass, and every pinned
  frame hash unchanged (held frames are exact).
- [ ] **Step 5: Mutation-check:** skip the erased box in the copy; skip the
  clear of a band; drop the y translation; skip the stale-depth switch-off;
  skip the drop at close. Each must fail a test.
- [ ] **Step 6: Build for the board** (`pio run -e stopwatch`) and note the
  free internal heap in the `run` line once flashed.
- [ ] **Step 7: A/B on the board** against `feature/43-gl-speed-experiments`,
  every registered GL hack plus Hypnowheel (`-DHYPNOWHEEL_PROBE`), with
  `-DGL_PROFILE`, two rounds of 60 s. Unknown Pleasures still spills on its
  lines here, so expect no change for it; expect Gears and Hypnowheel faster.
- [ ] **Step 8: Commit,** with the table in the message.

### Task 4: Lines and points in held frames

**Files:**

- Modify: `firmware/src/glshim/glshim_band.h`, `glshim_band.c`
- Modify: `tools/tinygl_patch.py` (line and point call sites in `clip.c`'s
  `gl_draw_line` and `gl_draw_point`)
- Test: `firmware/test/test_glshim_band/test_glshim_band.c`

**Interfaces:**

- Consumes: Task 3's record, state table and replay.
- Produces:

```c
typedef void (*GlshimLineFn)(ZBuffer *, ZBufferPoint *, ZBufferPoint *);
int glshim_band_line(ZBuffer *zb, GlshimLineFn fn, ZBufferPoint *a,
                     ZBufferPoint *b);           /* 1 if held */
int glshim_band_point(ZBuffer *zb, ZBufferPoint *p); /* 1 if held */
```

- A line record also keeps each end's `fx` and `fy` (float), within the
  64 bytes. A point's row range includes its `pointsize`.

- [ ] **Step 1: Write the failing tests** (direct against held, exact):
  - `test_smooth_lines_over_offset_quads_match_direct` (Unknown Pleasures'
    pattern: filled quads with `glPolygonOffset(0.5, 0.5)`, then smooth
    lines along their top edges, depth test on).
  - `test_plain_depth_tested_lines_match_direct`.
  - `test_a_line_crossing_every_band_matches_direct`.
  - `test_points_of_size_one_and_three_on_band_edges_match_direct`.
  - `test_a_held_frame_with_lines_does_not_spill` (a spill counter hook).
- [ ] **Step 2: Run them and watch them fail.**
- [ ] **Step 3: Implement** the line and point records and replay, and route
  `ZB_line`, `ZB_line_z` and `ZB_plot` calls through them.
- [ ] **Step 4: Run all host tests.** Expected: all pass, hashes unchanged.
- [ ] **Step 5: Mutation-check:** drop `fx`/`fy` from the record; drop the
  point size from the row range.
- [ ] **Step 6: A/B on the board** as in Task 3, Step 7. Expected: Unknown
  Pleasures faster; no registered GL hack slower.
- [ ] **Step 7: Ask Rod to look** at Unknown Pleasures and Hypnowheel, pinned.
- [ ] **Step 8: Commit.**

### Task 5: A correct, faster additive blend

**Files:**

- Modify: `tools/tinygl_patch.py` (the `GL_ONE, GL_ONE` path)
- Test: `firmware/test/test_glshim/test_glshim.c`, `test_hacks.c` hashes

**Interfaces:**

- Produces: `static inline PIXEL glshim_add565(PIXEL dst, PIXEL src)`, a
  per-channel saturating add on the canvas's byte-swapped RGB565, used
  wherever TinyGL would apply `GL_ONE, GL_ONE` with `GL_FUNC_ADD`. TinyGL's
  macro decodes swapped pixels as unswapped (`.claude/rules/gl.md`), so this
  changes colours: deliberately.

- [ ] **Step 1: Write the failing tests:**
  - `test_additive_blend_adds_each_channel` (dark red `rgb565(64, 0, 0)` plus
    dark blue `rgb565(0, 0, 64)` gives `rgb565(64, 0, 64)` in canvas order).
  - `test_additive_blend_saturates_each_channel_on_its_own`
    (`rgb565(200, 40, 0)` plus `rgb565(200, 40, 0)` gives
    `rgb565(248, 80, 0)`: red saturates, green does not carry into it).
- [ ] **Step 2: Run them and watch them fail.**
- [ ] **Step 3: Implement** with masks (the #73 idea: split the channels with
  masks, add, detect overflow per channel, OR in saturation). Two pixels per
  32-bit word where a span allows it is the implementer's call; measure.
- [ ] **Step 4: Re-pin the hashes** of any registered hack that uses
  `GL_ONE, GL_ONE` (`grep -n glBlendFunc` across GL hacks first), only after
  looking at a dumped frame of each.
- [ ] **Step 5: Profile Hypnowheel on the board;** ask Rod to look, since its
  colours change.
- [ ] **Step 6: Commit.**

### Task 6: Half resolution, as an option for Hypnowheel

**Files:**

- Modify: `firmware/src/glshim/glshim.c`, `glshim.h`
- Modify: `firmware/src/hacks/hypnowheel_single_gl.c` (under
  `HYPNOWHEEL_HALF`)
- Test: `firmware/test/test_glshim_band/test_glshim_band.c`

**Interfaces:**

- Produces: `void glshim_set_render_scale(int scale);` (1 or 2), read by the
  next `glshim_open` and reset to 1 by `glshim_close`. At scale 2 TinyGL draws
  into a half-size PSRAM buffer, held frames band over that buffer, and the
  swap scales the dirty box up 2 by 2 into the canvas and marks the scaled
  box dirty.

- [ ] **Step 1: Write the failing tests:**
  - `test_at_scale_two_each_pixel_becomes_a_two_by_two_block`.
  - `test_at_scale_two_the_dirty_box_is_scaled`.
  - `test_closing_resets_the_scale`.
- [ ] **Step 2: Run them and watch them fail.**
- [ ] **Step 3: Implement,** and have the Hypnowheel wrapper call
  `glshim_set_render_scale(2)` from its init when built with
  `-DHYPNOWHEEL_HALF` (empty `XSCREENSAVER_MODULE` and register its own init,
  as `blaster_safe.c` does).
- [ ] **Step 4: Run all host tests.**
- [ ] **Step 5: Flash both pinned** (`full=-DHYPNOWHEEL_PROBE`,
  `half=-DHYPNOWHEEL_PROBE -DHYPNOWHEEL_HALF`) with `ab_flash.py`, give Rod the
  numbers side by side, and ask him to choose.
- [ ] **Step 6: Commit.**

### Task 7: Rotation decision, notes and delivery

**Files:**

- Modify: `.claude/rules/gl.md`, `tools/assessment_measured.md`,
  `firmware/src/glshim/glshim_prof.h` comment if needed
- Modify (if Rod says yes): `registry.c`, `failed_ports.txt`,
  `platformio.ini`, `test_hacks.c` (Hypnowheel back in the rotation)

- [ ] **Step 1: Ask Rod** whether Hypnowheel goes back into the rotation, and
  at which resolution. If yes, register it unconditionally, remove its
  `failed_ports.txt` line, pin its frame hash after looking at a dump, and
  regenerate the assessment (`uv run tools/score_hacks.py`).
- [ ] **Step 2: Write the gotchas** into `gl.md`: held frames, spill triggers,
  the sticky fallback, `glshim_set_banding(0)` for hacks that draw against
  earlier depth, band memory, and the new board numbers. Add measured rows to
  `assessment_measured.md`.
- [ ] **Step 3: Run everything** (`pio test -e native`, the Python tests,
  `check_notices.py`, `markdownlint` on touched Markdown).
- [ ] **Step 4: Reflash `main`'s rotating build** once Rod is done looking.
- [ ] **Step 5: Deliver** with the `deliver-branch` skill: a draft PR with the
  board table and what is unverified.
