---
paths:
  - "firmware/src/glshim/**"
  - "firmware/src/tinygl/**"
  - "firmware/src/xs_support/glx/**"
  - "firmware/src/hacks/gears/**"
  - "firmware/src/hacks/gears_gl.c"
  - "firmware/src/hacks/morph3d/**"
  - "firmware/src/hacks/morph3d_gl.c"
  - "firmware/src/hacks/cubicgrid/**"
  - "firmware/src/hacks/cubicgrid_gl.c"
  - "firmware/src/hacks/unknownpleasures/**"
  - "firmware/src/hacks/unknownpleasures_gl.c"
  - "firmware/src/hacks/pipes/**"
  - "firmware/src/hacks/pipes_gl.c"
  - "firmware/src/hacks/glmatrix_single_gl.c"
  - "firmware/src/hacks/menger_gl.c"
  - "firmware/src/hacks/superquadrics_single_gl.c"
  - "firmware/native/gl_budget_main.c"
  - "firmware/patch_tinygl.py"
  - "tools/tinygl_patch.py"
  - "tools/gl_budget.py"
---

# GL layer and TinyGL gotchas

OpenGL hacks run on TinyGL, a software OpenGL (C-Chads fork, commit
`36a7987`, zlib-style licence). Issues #42 to #46 hold the numbers and ideas.

- `src/tinygl/` is byte-identical to upstream and excluded from every env.
  `firmware/patch_tinygl.py` runs `tools/tinygl_patch.py` and writes the
  patched tree, plus one unity file, into `.pio/build/<env>/generated/`;
  `glshim/tinygl_build.c` compiles it. Each patch checks its anchor and fails
  with "has upstream changed?". TinyGL's README allows building it as one
  unit, and it compiles cleanly that way.
- A GL hack gets `USE_GL` from a one-line wrapper: `hacks/<name>_gl.c` for
  the hack, `glshim/glx_<name>.c` for each helper copied to
  `xs_support/glx/`. Exclude the copies in all three envs' `build_src_filter`
  and keep `-Isrc/xs_support/glx` in `build_flags`. Never define `HAVE_GL`:
  it adds `glx_context` to `ModeInfo`, which `xlockmore.c` (built without
  it) allocates smaller. Build each helper as its own unit: `tube.h` and
  `normals.h` define clashing types.
- TinyGL draws straight into `canvas->px` at the canvas size, with its
  pixel packer patched to the canvas's byte-swapped RGB565. Test colours
  with exact `rgb565()` values: white looks the same either way round.
- Display list name 0 means "invalid" to hacks (`if (!glGenLists(1)) abort()`)
  and TinyGL handed it out first; the patch starts at 1. Its list buffers
  are 64 slots, not 4,096: each list took 16 KB and 500 lists overflowed
  PSRAM (boot loop, `StoreProhibited` at `0x4000` in `alloc_list`).
- TinyGL counts vertex-array stride in floats and reads the array when a
  display list is replayed; GL counts bytes and reads at the draw call, and
  hacks free the array straight after (`tube.c`, `sphere.c`). Hacks'
  `glVertexPointer` and friends are renamed to `glshim_*` by macros in
  `glshim.h`, and `glDrawArrays` expands at once. The symptom of the old
  behaviour was needles through the tubes.
- The context is freed by the shim: `glshim_open` registers
  `xshim_set_release_hook`, which `xshim_release_pixmaps` (the runner's
  `stop()`) runs. A second `init_GL` replaces the first, which hides a
  missing hook, so test `glshim_is_open()` right after the hack stops.
- `glshim_open` returns NULL for an empty canvas or a failed z-buffer
  allocation only. `glInit`'s own allocations (lists, matrix stacks) are
  unchecked upstream, so running out of PSRAM after the z-buffer crashes there.
- `glDrawArrays` reads every client array as `GL_FLOAT` and ignores the type
  argument: check a new hack's `glVertexPointer` calls before trusting it.
- TinyGL leaves specular lighting off (`zEnableSpecular = 0`) unless the
  application calls `glSetEnableSpecular(1)`, so Gears has no highlights
  and `glMateriali` is invisible without it. Enabling it costs speed (#43).
- TinyGL is built without `GL_POLYGON` (`TGL_FEATURE_GL_POLYGON 0`), which
  upstream also uses to compile out `GL_LINE_LOOP`. `tinygl_patch.py` puts the
  line loop back (issue #80): StonerView's squares now have their outlines.
  `GL_POLYGON` still draws nothing: Rubik's wrapper (`rubik_gl.c`) draws its
  convex octagon stickers as a triangle fan, and with a polygon offset, since
  a sticker is 0.01 above its cube face, about two 16-bit depth steps at the
  cube's distance, and the face showed through in patches (7,152 coloured
  pixels in the pinned frame, 8,678 with the offset). Unknown Pleasures' rim
  loop is drawn too, but sits at depth -0.015 behind the slab's top face
  (-0.01), so no frame changed. An unhandled primitive used to keep counting
  vertices, and the fifth overwrote the matrices and colours after
  `c->vertex` (Pipes' `buildlwo.c` crashed the host); `tinygl_patch.py` now
  holds it at the first slot. Check a new hack for both.
- A decal drawn a hair above a surface z-fights with it at 16-bit depth, and a
  test that only asks whether something drew passes. Count the decal's
  coloured pixels, and mutation-check the offset (`test_hacks.c`'s Rubik
  sticker test fails at 7,152 without it).
- TinyGL lacks `glTexGeni`, `glFog*` and `glInterleavedArrays` (#46). The
  header declares `glFog*` and the `GL_SPHERE_MAP` enum; the library defines
  neither.
- TinyGL snaps every vertex to a whole pixel (`zp.x`, `zp.y`) before it draws,
  so a line that moves a little jumps a whole pixel somewhere along its length:
  Unknown Pleasures' flat stretches hopped, which read as flicker. The patch
  keeps the fractional position (`zp.fx`, `zp.fy`) and draws a line with Wu's
  algorithm when a hack enables `GL_LINE_SMOOTH`. Only lines get it; filled
  triangles are still snapped. A line whose two colours differ is drawn plain.
- Smooth lines ignore vertex alpha (`ZB_line_aa` reads only r, g, b), so
  Unknown Pleasures' per-row fade (`glColor4f` with an eased alpha) does not
  happen and every ridge draws at full intensity. Rod approved that look on the
  device and closed the review finding as wontfix.
- Unknown Pleasures runs at `*speed: 0.3` (a registry override; the panel dims
  after each push by an amount that grows with the scroll per push, see
  `device-display.md` and issue #68). The hack halves `*count` at displays of
  480 pixels or less, so the default of 80 draws 40 rows and `*count: 40` draws
  20. Every new row is added by `tick_unk` (about every 1.2 s at full speed).
  Upstream adds it at the end of a draw and restarts its clock at that frame's
  time, and the frame was drawn at the scroll's clamp, so the time past the
  interval never scrolled: the picture stood still for a push (issue #134,
  filmed as a 190 ms stall every seventh push at 40 rows of 50 points).
  `unknownpleasures_gl.c` adds the row before the draw and moves the clock on
  by one interval. The tick is 12 to 35 ms (software `double` `cos`) and the
  TE sync wait absorbs it, so it never stretched a frame. `glLineWidth` is a
  no-op in the shim.
- TinyGL has no alpha: the blend macro adds colours, ignores the blend
  function's alpha, and decodes pixels as if they were not byte-swapped, so
  it is only right for black and white. A white-on-black frame hides this, and
  smooth (grey) lines did not. The fill wrapper (`gl_draw_triangle_fill` in
  `tinygl_patch.py`) treats `SRC_ALPHA, ONE_MINUS_SRC_ALPHA`, which is alpha 1
  in real GL, as an opaque fill. A blend that only adds (each factor `GL_ONE`
  or one TinyGL treats as one, such as StonerView's `GL_SRC_ALPHA, GL_ONE`,
  with `GL_FUNC_ADD`) takes `glshim_add565`, a saturating add per channel on
  the swapped pixel (`TGL_BLEND_ADDS` in `tinygl_patch.py`); StonerView's
  colours went from scattered dark and rainbow squares to smooth runs of hue,
  and Rod preferred them. The other blend functions still use the macro and
  are wrong for any colour but black and white.
- Polygon offset was stored and never read. The fill wrapper applies it:
  `factor` times the polygon's steepest depth slope plus `units` times one
  depth step, pushed away from the viewer. Without it a line drawn on the edge
  of its own filled polygon is erased in dashes. Unknown Pleasures' own
  `glPolygonOffset (0.5, 0.5)` is not enough here: vertices are snapped to
  whole pixels, so a polygon's depth is wrong by up to a pixel's slope in each
  direction, and with only the hack's offset each quad still ate a few percent
  of its own line, a different few each frame. The wrapper adds
  `|dz/dx| + |dz/dy|` whenever an offset is asked for (not for an offset of 0, 0).
  Measured on the host by the second difference of a frame's total brightness
  while the picture drifts a fifth of a pixel a frame: 3,201 at the hack's own
  offset, about 500 (the floor) with the margin. An earlier depth-fighting
  experiment found no effect only because blending made every quad invisible:
  change one thing at a time, and check that the thing under test can act.
- The offset's snapping margin takes the sign of the offset (a negative offset
  pulls toward the viewer, so a positive margin would weaken it), the bias is
  clamped to about +/-2^29, and each shifted vertex depth is clamped to the
  16-bit range with the originals restored exactly: an offset that carried a
  fill past the far plane used to wrap round and beat a nearer fill. Tests use
  a second fill slightly behind the first, because a twin at equal depth wins
  a tie either way and cannot tell the signs apart.
- The clipper keeps lines inside the view volume, and the viewport maps NDC 1 to
  pixel `xsize - 0.5`, so only a viewport larger than the canvas can carry a
  line past its right or bottom edge. The smooth-line plot's guards are
  defensive otherwise; `test_glshim` uses `glViewport(0, 0, 2 * SIZE, 2 * SIZE)`
  to reach them.
- Speed is about 6 us per lit vertex plus about 80 ms a frame (clear 25 ms,
  push 32 ms): Gears runs at 4 to 5 fps for its heaviest layout and about 9
  for light ones. `-O2` for TinyGL and specular lookup tables changed
  nothing; whole-firmware `-O2` crashes the compiler in `braid_single.c`.
  `CLIP_EPSILON` was an exponent literal (`1E-5`), which `float_literals.py`
  leaves alone, so `gl_clipcode` ran a software `double` multiply on every
  vertex; `tinygl_patch.py` now makes it `1E-5f`, which took 5.6 ms off
  CubicGrid's 52 ms step (0.7 us a vertex) and about 3 ms off Morph3D's.
  `gl_shade_vertex` has not been checked for `double` the same way.
- Gears picks a random layout on each start, so a running Gears holds 0.9 to
  2.5 MB. Measure leaks while stopped, not by comparing two running moments.
  `free_gears` never frees `bp->gears`: 40 to 1,500 bytes a start, upstream.
- The host stack figure for Gears (21 KB, unoptimised and under
  AddressSanitizer) is not the board's (2.4 KB used in the spike); the host
  test is a tripwire and the board is the proof.
- `dump_main.c` raw frames are already un-swapped; `rgb565_to_png.py` reads
  them as plain RGB565. Swapping them again gives psychedelic stripes.
- `tools/gl_budget.py` counts vertices, triangles and fill in a private,
  instrumented copy of TinyGL (`instrument()` adds the counters after
  `tinygl_patch.patch_tree`, with the same checked anchors). The firmware's
  TinyGL never has them. Its fill cost is fitted to one Gears scene, so it
  over-predicts few-large-triangle scenes, and lines and points cost nothing
  beyond vertex setup. Add a second calibration point after each GL port.
- TinyGL's `glLightModelfv` copied four floats whatever the parameter, so
  `glLightModelfv(GL_LIGHT_MODEL_TWO_SIDE, one_float)` (Morph3D) read past the
  array: AddressSanitizer's global-buffer-overflow. `tinygl_patch.py` now copies
  four only for `GL_LIGHT_MODEL_AMBIENT`. `glopLightModel` still reads the
  value back as an int from a float slot, which only works because any
  non-zero float has non-zero bits; leave it unless a hack passes 0.
- Morph3D is small and wanders by design: a window (not iconic) build scales
  the object to 0.3 and moves it on a Lissajous path, so it is about 70 px
  across on the 466 px canvas and its position differs frame to frame. It
  cannot be enlarged without patching the hack. It enables two lights and
  two-sided lighting; `docs/gl-budget.md` predicts 6 to 11 fps.
- Dirty rectangle: TinyGL reports the box around everything it draws to
  `glshim_note_box` (hooks in `tinygl_patch.py`: triangles, points, lines,
  text pixels, and `glDrawPixels` as the whole canvas). `glClear` clears only the
  box drawn since the last clear and a swap marks only what was drawn and what
  the clear erased. A new context, and a new clear colour, are full clears. The
  contract: nothing else may draw on the canvas while a GL hack runs (the overlay
  restores what it stamps). A line's hook must order its endpoints before adding
  the margin: `p1.x - 1` to `p2.x + 1` shrinks a right-to-left line's box.
  `tools/gl_budget.py` gets its boxes from `glshim_box_observer` so it cannot
  drift from the firmware's rule.
- The round panel (issue #77): when the canvas has a disc, `glshim_open`
  exports its spans at TinyGL's size (`glshim_span_x0/x1`; at scale 2 a pixel
  counts if any of its 2 by 2 block does). `glClear` (colour and depth) and the
  scale-2 swap skip the corners, and `tinygl_patch.py` cuts each fill scanline
  to the span, advancing depth and colour to the new first pixel. The clip is
  decided once per triangle: the disc is convex, so a triangle with every vertex
  inside needs none. Checking every scanline cost Gears 3.2 ms a frame though it
  never reaches the corners; per triangle it costs 0.46 ms (58.27 to 58.73).
  HexTrail went from 48.62 to 45.32 ms, CubicGrid's clear from 12.3 to 10.0 ms.
  Textured fills (their own `DRAW_LINE`), lines and points are not clipped.
  Host test canvases have no disc, so pinned hashes are unchanged;
  `test_push_present` replays every hack on a disc canvas and compares only the
  disc. `gl_budget.py` counts fill without a disc, so it overstates corner fill.
- `glDrawPixels` keeps its data pointer in a 4-byte `GLParam`, so it cannot draw
  on a 64-bit host (fine on the 32-bit board); it has a patch-level test only.
- A hack's own `*delay` is a pause after each frame and the loop credits only the
  push against it, so it hides speed-ups: Morph3D (40 ms) and Gears (30 ms) were
  held to 10 and 11 to 13 fps until their registered override cut it to 10 ms.
  Check `wait=` in the log before judging a speed-up.
- Measuring on the board: a boot is deterministic (same hack, same layout, same
  windows), so alternate builds and compare window by window; one earlier
  outlier read 54.9 ms against 52.3 and was never explained. CubicGrid (fixed
  8,000 unlit points) and Gears' first layout after boot are the repeatable
  benchmarks; Morph3D's shape is random per boot. To split a frame, add a
  temporary `#if GL_PROFILE == n` early return to a TinyGL function through
  `tinygl_patch.py` and flash with `-DGL_PROFILE=n`; never commit it.
- A `glClear` of the depth buffer while the depth test is off does nothing
  until the test is enabled (`glshim_depth_test_changed`, patched into
  `misc.c`): nothing reads depth with the test off, and the clear is a second
  full-canvas fill of PSRAM. HexTrail clears it every frame and never enables
  the test; skipping it took 13.0 fps to 15.3. `glshim_depth_clears()` counts
  the clears that ran, for the tests.
- TinyGL has `glOrtho` commented out of its header (Boing needs it). `glshim.c`
  builds the usual matrix and calls `glMultMatrixf`; `test_glshim` checks it
  with an off-centre, non-square box and with the near and far planes, since
  ordering two triangles alone could not tell the depth term was wrong.
- Boing on TinyGL: its smooth, blended scanlines and shadow are the slow part
  (see `hacks.md`). The budget model predicted 106 ms a frame with scanlines
  and the board took 204 (171 step, 32 push): add that as a calibration point,
  since the model has no blend term. HexTrail was predicted at 15.7 fps for
  its early frames at count 20 and the first window on the board read 15.0,
  but the model reads one early frame and the hack's cost grows over its
  cycle (the step reached 293 ms).
- The push overlap (`repaints_all`, issue #118): before each clear or
  primitive, `follow_canvas` points TinyGL's `pbuf` at the current
  `canvas->px`. The colour dirty rectangle is kept per buffer (the buffer
  just left keeps its box, valid flag and clear colour), so a clear still
  rewrites only what was drawn in that buffer; a buffer TinyGL never drew in
  gets a full clear. The box last drawn in the buffer just left is marked
  erased, since that is what the display shows. The swap does not follow:
  a frame that draws nothing must mark nothing, or the push would send the
  older frame the new buffer holds. If the two buffers' clear colours
  differ, the switch marks the whole canvas: the clear finds its own buffer
  already that colour and rewrites nothing (red, green, red left the
  display green). `glshim_canvas_overwritten()` forces a full clear for
  writers other than TinyGL (`test_push_overlap`'s noise
  run). Not for a scale-2 hack (`double_into_canvas` writes only the box) or
  Pipes (skips clears). Pinned A/B, plain to overlapped: Bubble3D 11.2 to
  12.0 fps, Gears 15.2 to 16.5, CubicGrid 15.3 to 19.5, StonerView 36.4 to
  43.8, HexTrail 15.2 to 20.5 before its first restart; Morph3D about 3%
  (small push), unflagged. The step grew 5 to 10 ms with both cores on
  PSRAM.
- The swap marks two bounding boxes (erased, drawn), not a list of
  regions: a drawing in the top half and an erase in the bottom one mark
  every row between. A test that asserts a row stays clean must keep both
  frames' drawings on one side of it (`corner_frame(f % 2)`).
- Board profiles (issue #43): `-DGL_PROFILE` prints a `glprof` line after
  each `run` line with cycle counts per stage (clear, list, vertex, shade,
  tri, fill, point, line; nested, so vertex includes tri and fill), and
  `tools/ab_flash.py` builds variants pinned, flashes them in turn and
  tabulates the windows. Each variant needs its own build directory:
  PlatformIO cleans a build directory, image and all, when the build flags
  change. Gears' boot layout: fill 28 ms, vertex work outside lighting
  17 ms, clear 6 ms, lighting 3.7 ms of a 61 ms step.
- Lazy lighting is the default (`GL_LAZY_SHADE`; `-DGL_EAGER_SHADE` restores
  TinyGL's order): a vertex is lit when something that uses it is drawn, so
  culled triangles cost nothing. Gears 61.1 to 57.7 ms, Morph3D 61.4 to
  58.4 ms, frame hashes unchanged. `GL_IRAM` (still a switch) and two
  experiments since removed, matrix stacks in internal RAM and the data
  cache's next-line prefetch (autoload) over PSRAM, moved Gears by under 1 ms.
  The prefetch needs a region that holds only mapped pages: over the whole
  32 MB data bus it boot-looped the board (the panic handler faulted too), and
  a region worked out from an allocation's address cannot promise that.
- Fill is compute-bound, not PSRAM-miss-bound. A new PSRAM cache line costs
  129 cycles to read and about 220 to read and write back (the `membench`
  line of a profile build), but neighbouring triangles reuse lines. A banded
  renderer that held a frame's triangles and replayed them into 32-row bands
  in internal RAM was exact but slower: Gears 59 to 76 ms (replay 38 ms
  against 30 ms of direct fill), Hypnowheel 476 to 528 ms. It was reverted
  (`feature/43-banded-renderer` history). Faster fill needs fewer cycles a
  pixel or fewer pixels, not other memory.
- Every `Replace` anchor in `tinygl_patch.py` must be upstream text: the
  anchor tests delete it from the unpatched source. To add next to text an
  earlier patch inserted, extend that patch's replacement instead.
- Resolving a merge conflict in `tools/tinygl_patch.py`: the conflict region
  can span the ends of `REPLACEMENTS`, `LATE_REPLACEMENTS` and `HOT_FUNCTIONS`,
  so keeping both sides can put an entry in the wrong tuple (a `Replace` among
  the `Hot` entries fails collection with "'Replace' object has no attribute
  'definition'"). Check each kept entry's tuple. `ruff format` on the file also
  rewraps an `AA_LINE +` line nobody touched; revert that hunk.
- `glshim_set_render_scale(2)`, read by the next `glshim_open`, renders at half
  the width and height: `glViewport` is a macro for `glshim_Viewport`, which
  divides by the scale, and the swap doubles the dirty box into the canvas.
  Hypnowheel's fill went from 334 to 129 ms (not a quarter: rows and triangles
  still cost). Nothing registered uses it.
- Added for the 2026-10-12 batch, each test-first: `glInterleavedArrays` for
  `GL_V3F`, `GL_C3F_V3F` and `GL_N3F_V3F` (what `gllist.c` uses);
  `glTexImage2D` converting RGBA and luminance bytes to RGB (TinyGL exits on
  anything but three-component `GL_RGB`); `glColor4ub`; `glTexGeni`,
  `glDepthFunc` and `glTexEnvf` accepted and ignored; `glTexParameterf`; the
  OpenGL 1.2 colour-control names; and `texfont.h` (`texfont.c`), which draws
  TinyGL's public-domain 8x8 font as quads at half the point size.
- TinyGL fills a quad over [10, 16] by [23, 24] as columns 9 to 15 and both
  rows: its edges are inclusive, so test text and thin quads by their box,
  not a pixel count. Its lit textures can lose a step of a channel.
- TinyGL prints a warning for every `glEnable` it does not know, each time,
  which on the board goes to the serial log every frame: filter them in the
  wrapper.
- Pipes (issue #44, `hacks/pipes_gl.c`) skips work no other GL hack can: with
  its camera fixed (`*rotatepipes: False`), when the clear, both matrices and
  the lists called so far match last frame, it skips the clear and those lists
  and draws only the new piece. Two traps, each caught by
  `test_pipes_incremental_redraw_matches_a_full_redraw`: a new screen
  recompiles the same list names, so any `glNewList` must end the skipping; and
  the next screen must be generated at the start of a draw, as upstream's fade
  does, because `generate_system` clears, and a clear after the swap erased the
  frame just shown in a full redraw. `pipes_full_redraw` turns the skipping off
  for the tests.
- Pipes compiles every display list of a screen when the screen starts, and a
  list holds each vertex expanded (a 16 by 16 sphere from `glDrawArrays` is
  about 17 KB). Board PSRAM sets the screen size (`-DPIPES_CYCLES`,
  `-DPIPES_SIZE`), and TinyGL does not check a list's allocation, so running
  out crashes: 3 systems of 150 left 1.9 MB free. Host heap for the same lists
  is about twice the board's (8-byte list words).
- Pipes' helpers: `teapot.c` is built with `HAVE_JWZGLES` (a triangle table,
  `teapot2.h`, instead of evaluators TinyGL lacks). That table was 228 KB of
  flash; since #104 `firmware/teapot_table.py` writes `generated/teapot/`, a
  byte-identical copy of `teapot.c` beside a `teapot2.h` holding the floats
  deflated (37 KB), so the copy's quoted include finds ours first.
  `glshim/glx_teapot.c` renames upstream's `unit_teapot` and unpacks the table
  into 227 KB of PSRAM only while it runs (Pipes calls it once, compiling a
  list); with no memory the teapot is an empty list. `test_glshim` compares
  the unpacked floats with upstream's. `buildlwo.c` maps `GL_POLYGON` to
  `GL_TRIANGLE_FAN` in its own wrapper only (issue #80 is the general
  question).
