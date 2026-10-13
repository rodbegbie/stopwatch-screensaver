---
paths:
  - "firmware/src/hacks/**"
  - "firmware/patch_*.py"
  - "firmware/pacman_sprites.py"
  - "tools/maze_patch.py"
  - "tools/pacman_patch.py"
---

# Per-hack gotchas

Moved verbatim from the Gotchas section of AGENTS.md.

- Maze keeps a 1000 by 1000 maze and three 1,000,000-entry move lists in one
  `calloc` (about 20 MB; the board has 8 MB of PSRAM). `hacks/maze/maze.c`
  stays byte-identical: `firmware/patch_maze.py` (a PlatformIO `pre:` script
  in every env) writes a copy with both limits at 80 into the build dir, using
  `tools/maze_patch.py`, which fails loudly if upstream's `#define`s change.
  `hacks/maze_small.c` includes that copy and the original is excluded from
  `build_src_filter`. A `gridSize` below 7 would overflow the 80 by 80 arrays.
- Pacman has two build-time patches (`tools/pacman_patch.py`, run by
  `firmware/patch_pacman.py`; wrappers `hacks/pacman_stdlevel.c` and
  `hacks/pacman_loop_ai.c`): the random level generator recursed to ~315 KB
  and boot-looped the board, so a generated level is a copy from
  `hacks/pacman_pool.h` (24 levels, made on the host by
  `tools/pacman_levels.py` with the cells outside the round display walled
  off; the fixed level is still half of them), and the ghosts' route search is
  a loop (it was 453 levels deep). Re-run the tool after changing its mask or
  the generator's tile data. Each patch checks what it replaces. The two host
  pins that follow the game (`kBaseline`'s Pacman and the ghost-route chain)
  depend on how many `random()` values the level pick takes: re-take the chain
  with `pacman_loop_ai.c` temporarily including `pacman/pacman_ai.c`, then
  restore it and check the loop gives the same value. Its sprite sheet is
  built from `vendor/` into the build dir by `firmware/pacman_sprites.py` and
  never committed (3 MB as C text).
- Braid runs at 3-9 fps (0.8-2 before four rounds of work): it redraws every
  segment of the braid each frame to spin the colours. A host profile put its
  disc fill at 6% and the board spent most of its time there, because the Mac
  does `double` at full speed. Split a slow hack's step on the device with a
  temporary flag that returns early from the expensive call.
- Maze fills the whole 466 by 466 square, so its corners and the exit marker
  fall outside the round display's visible circle.
- `check_notices.py` reads 100 lines of header: Maze's licence follows a long
  modification history.
- A hack can be freed before its first draw: two quick presses, or a press
  with the 90 s rotation. `test_every_hack_can_be_stopped_before_its_first_frame`
  runs every hack through it. Blaster's free dereferenced NULL (it sizes a loop
  in init but allocates in draw), so `hacks/blaster_safe.c` includes the
  unmodified hack with `XSCREENSAVER_MODULE` emptied and registers it with a
  free that zeroes `NUM_ROBOTS` when `robots` is NULL.
- Deluxe is built through `hacks/deluxe_opaque.c`, which includes the unmodified
  hack with `XSCREENSAVER_MODULE` emptied and registers it with overrides:
  `*transparent: False` (upstream's plane masks need colour planes, which
  `allocate_alpha_colors` reports none of) and `*doubleBuffer: False` (it was
  added when the shim drew only on the canvas, so the `XCopyArea` from its
  blank pixmap erased the picture; pixmaps work now, but two 434 KB back
  buffers and a full-frame copy each frame would cost more than they give).
  The wrapper then gives each throbber's GC an alpha of 26
  of 32 (upstream's translucent weight) and sends full wide circles to
  `stroke_circle`. A plain screenhack's `HackEntry` has no overrides unless a
  wrapper builds it. Its step is 45-63 ms on the device against a 32.5 ms push,
  so about 11-13 fps; ideas for more are in issue #54.
- Eight of the S-hack batch (Anemone, Anemotaxis, Compass, FuzzyFlakes,
  Halftone, Interaggregate, LMorph, Wormhole) are built through
  `hacks/<name>_direct.c` (pixmap double buffering, see `techniques.md`;
  LMorph's is `lmorph_single.c` because it needs no `HAVE_JWXYZ`). All but
  Anemotaxis and Wormhole, and Euler2D (`euler2d_single.c`), also use
  `single_precision.h` plus build-time float literals (`patch_float_literals.py`
  lists them). `single_precision.h` leaves `floor` and `ceil` on the double
  versions: Euler2D's wrapper maps them itself. After the literal pass, `nm`
  still shows soft-double helpers in Euler2D, Halftone, Interaggregate and
  Anemone; their source was not chased.
- `test_frames_of_every_hack_but_maze_match_main` stops at the first hash that
  differs, in registry order, so repinning several hacks takes one test run
  each (about 85 s), earliest first. A hack whose frames do not change under a
  wrapper (Compass and Halftone in single precision) keeps its hash.
- Anemone's step moves between 31 and 56 ms as its arms grow and shrink, so its
  fps is 11 to 15.5 whatever one 20 s window says. Forest waits 500 ms between
  trees by design (about 2 fps, and the push is 88 rows).
- A hack that unpacks pixel values itself (Substrate and Interaggregate, in
  their `point2rgb`) assumes ordinary RGB565, but the canvas holds the bytes
  swapped. Its wrapper swaps pixels where they cross `XAllocColor` and
  `XSetForeground`. Interaggregate shipped without this: its alpha blends
  mixed the wrong bit fields and drew rainbow noise instead of green and brown
  strokes. `test_interaggregate_blend` includes the wrapper to check one blend.
- `dump` never seeds `random()`, so a hack that picks a mode at start (Truchet,
  12 modes) shows the same one in every dump. To see others, build the hack
  alone (`probe_hacks.registry_source` and `shim_sources`) from a copy outside
  `vendor/`, with a `__attribute__((constructor))` that calls
  `srandom(atoi(getenv("SEED")))`. Cheaper: drop a temporary
  `src/hacks/zz_seed_tmp.c` with that constructor, guarded by `SEED` being set
  (`-e dump` builds every file under `src/`), run
  `SEED=n .pio/build/dump/program <index> <frames> out.raw`, and delete the
  file before running the tests. A device boot is deterministic too (Halo
  chose Ramp on five boots in a row), so a hack that picks a mode with
  `random()` always starts the same one: force another with a temporary
  `*mode:` override to measure it. Rotation reaches other modes because the
  random state differs by then.
- Hexadrop (`hexadrop_single.c`) runs at about 10 fps, under the 15 fps bar.
  Splitting its step on the device showed the maths and the edge walk cost
  0.1 ms and all the rest is canvas writes: about 860 KB of 16-bit stores
  into the PSRAM canvas a frame. Single precision, a float convex fill and
  two-pixel stores took it from 2.4 to 10.2 fps with pixel-identical frames.
  More needs a shared canvas change or rasterising in row bands.
- Halo (`halo_direct.c`, `halo_planes.h`, `halo_circles.h`) draws whole circles
  into 1-bit pixmaps and XORs two of them. Its pixmaps are packed bit planes
  (27 KB each), not colour pixmaps: 434 KB colour stand-ins made Seuss mode
  (three runs in four) 5-9 fps, because window-sized circles are millions of
  PSRAM writes, and a float circle fill alone changed nothing. A device boot
  is deterministic, so every boot picked Ramp mode and hid that: force
  `*colorMode: seuss` temporarily to measure the other mode. It overrides
  `*delay` to 10 ms (upstream asks for 100 ms), so the circles grow faster.
- StonerView's seven files are built as one unit by `stonerview_gl.c`.
  Upstream never frees its oscillator list ("how do we free this?"), so the
  wrapper frees it through the `next` links before upstream's release.
- XMatrix needs `matrix_glyphs.py` (a `pre:` script in every env), which
  converts four PNGs from `vendor/` into blobs in the build directory: they
  are never committed (1.5 MB of C text each for the large sheets). It
  registers through `xmatrix_direct.c` with `*trace: False`, which skips the
  typed "connecting" sequence it would open with (the rare "Knock, knock,
  Neo" event, `knockKnock`, is still on); `*matrixFont: small`
  gives 46 columns instead of 23. Its text modes read no text: the
  `textclient.h` stand-in never opens a source, and Xt timers never fire.
- Flurry is not ported. It draws 3,600 blended, textured quads a frame and
  needs `glAlphaFunc`, `glRectd`, `gluBuild2DMipmaps` and luminance-alpha
  textures, none in TinyGL here. With `gl_budget.py`'s per-vertex and
  per-pixel costs that is well under 1 fps (arithmetic, not a measurement:
  the tool cannot read a multi-file hack).
- Qix and Fireworkx both leaked on every stop, found by the restart leak test.
  Qix's `qix_free` skips each segment's `lines[i].p` array (about 48 KB per
  stop); Fireworkx below 24 bits never frees its own 565 image buffer (48 KB).
  Their wrappers (`qix_direct.c`, `fireworkx_single.c`) free the missing piece
  before calling upstream's free. LeakSanitizer does not run on macOS, but
  `MallocStackLogging=1 leaks --atExit -- .pio/build/dump/program <index> 0
  out.raw` names the allocating line in seconds (rebuild `-e dump` first: a
  `pio test` deletes the program).
- Starfish and Goop spent most of a frame in the shim's polygon fill (double
  maths per edge per row). `hacks/float_polygon.h` fills any even-odd polygon
  in float, optionally mixed in at an alpha in 32nds (all three colour
  channels with one multiply), and took Starfish from 8.5 to 19 fps. Goop draws
  straight onto the window (its `XCreatePixmap` is the window and the
  `XCopyArea` is skipped) and blends each blob in at the alpha upstream's jwxyz
  path uses. Its full-frame clear cost 15 ms, so `hacks/span_erase.h` erases
  only the bounding boxes the blobs filled last frame (pixel-identical frames):
  14.6-15.2 fps translucent. The shared `canvas_fill_polygon` is still double
  (Hexadrop has its own convex float fill). Goop is built through
  `goop_direct.c` with `HAVE_JWXYZ`; the colour-plane path needs planes the
  canvas lacks.
- A wrapper that does `#define double float` must include any header with a
  `double` in a struct the shim's `.c` files also use BEFORE the define: the
  hack includes it again, the guard keeps the copy seen through the macro, and
  the two sides disagree on the layout. `spline.h` did this to Starfish and
  Goop: they drew garbage (flat colours) at 16-20 fps and their first pinned
  hashes were of it. Look at a frame after any change that is not
  pixel-identical by construction;
  `test_hacks_that_use_the_spline_draw_a_real_picture` now fails on a flat
  screen.
- Fireworkx renders at 1/`FIREWORKX_SCALE` of the width and height
  (`hacks/lowrez_put.h`: it is told the window is smaller and `XPutImage`
  draws each pixel as a square): 2.4 fps as copied, 11 at a scale of 3, 12.4 at
  4. Its glow, flash pass and 565 conversion are full-frame walks over PSRAM.
  Its header says "(GPL)" but the text is the standard permission notice.
  Everything it works in is in pixels, tuned for a window about 1024 wide: at
  152 pixels a burst filled the picture and the glow blur (energy kept, output
  doubled) saturated, so the screen was white. `tools/fireworkx_patch.py`
  (run before the float literal pass; the copy stays byte-identical) scales the
  spark speed, gravity, flash reach and velocity jitter by the render width
  over 1024, and the wrapper throws 120 sparks a shell instead of 500 and fires
  two of its four shells. 12.4-12.6 fps, with a dark background between
  bursts. The shell count cannot change (it is baked into the SSE layout), so
  the idle shells are given an endless life and no sparks.
- Loop asks for 100 ms between generations (8.7 fps with a 3.5-7 ms step); the
  registry overrides it to 50 ms. It also took `malloc(0)` returning NULL, which
  the board does and the host does not, for out of memory: square mode asks
  for a rectangle list for each of eight colour states, most empty, and it
  freed its cells and drew nothing more (a black screen at 18 fps, half the
  starts, never seen on the host). `loop_direct.c` asks for one byte, and a test
  swaps in an allocator that returns NULL for zero. Hypnowheel was dropped from
  the rotation after Rod saw it, twice (issue #73; its source, notices row and a
  `failed_ports.txt` entry stay). `hypnowheel_single_gl.c` registers it only
  with `-DHYPNOWHEEL_PROBE`, as the last entry, for measuring. A board profile
  showed about 275 ms of its 706 ms step was `double` trig in every vertex, not
  fill as first thought; single precision gives 2.2 fps. The rest is fill
  (about 334 ms): `-DHYPNOWHEEL_HALF` renders at half resolution
  (`glshim_set_render_scale`) for 4.0 fps, still too slow for Rod.
- CWaves (`cwaves_single.c`) fills 233 columns, each a 2 by 466 rectangle, so
  every store lands on a different cache line of the PSRAM canvas: after single
  precision took the step from 95 to 61 ms, 57 of those were the fills and 4
  the cosines. The picture depends on x alone, so the wrapper writes each
  full-height column into canvas row 0 and the draw wrapper copies that row
  down (frames identical to the unmodified hack): 16 ms, 20 fps. Any hack that
  paints thin full-height strips can do the same.
- A single-precision wrapper must watch for a hack that passes the address of
  one of its own `double` locals to a shim function (`rgb_to_hsv (..., &fgs,
  &fgv)` in Moire): the prototype still says `double *`, so the float local is
  written eight bytes and the frames come out as colour noise, with no
  compiler warning. `moire_single.c` defines an adapter before
  `single_precision.h` and maps the name to it. Compare the wrapper's frames
  with the unmodified hack's: they were all different here, and identical once
  the adapter was in. A hack with no decimal literals (Moire) must not be in
  `patch_float_literals.py`, which refuses a file without any.
- Moire and Cynosure are paced by their own delays (Moire draws a 20-row chunk
  every 50 ms and holds each picture 5 s; Cynosure waits 500 ms a frame, which
  is upstream's default), so their fps is low whatever the step is. Cynosure's
  `XCreateBitmapFromData`, which the scorer lists as a gap, is inside a
  `DO_STIPPLE` block that is never defined.
- Moire2 (`moire2_direct.c`, `moire2_planes.h`, built on Halo's planes) keeps
  its four depth-1 pixmaps as bit planes and adds an outline circle, an XOR
  rectangle, an OR/XOR/copy between planes (the GC's `function`, which the shim
  now remembers) and a faster expansion to the window. Upstream's free leaves
  its pixmaps to the X server, and the runner frees only the shim's own, so the
  wrapper frees the planes. The first version ran at 9.6 fps: the plane copy
  and the expansion were 26 and 27 ms, because 64-bit index maths is several
  instructions an operation on a 32-bit core and the expansion branched on
  every pixel. After clipping everything fits an int; a byte table of eight
  pixels written four bytes at a time did the rest (28 ms). The 50 ms delay is
  overridden to 10 ms: 12.6 fps became 16.2.
- Boing was dropped after Rod saw it (too slow, and plain on a small screen);
  its source, notices row and `boing_gl.c` stay, the wrapper out of the build
  filter. Its 208 scanlines are blended full-width lines and cost half its
  step: 5.0 fps with them, 8.3 without; the ball size barely matters (0.3
  saves 13 ms). HexTrail (`hextrail_single_gl.c`) draws
  every hexagon it has built, so its step climbs from about 19 ms to a ceiling
  and it restarts: use a mean over a cycle (150 s). The grid is `count * 2`
  squared, and a smaller count makes fewer, larger hexagons over the same area:
  mean 5.7 fps at upstream's 20, 9.1 at 10, 12.7 at 6 (registered,
  `-DHEXTRAIL_COUNT=<n>`). Rod found 12.7 too slow once the picture filled
  (under 10 fps). Single precision gave 13.0; skipping the depth clear it
  never needs (see `gl.md`) gave 15.3, worst window 10.8. The single-precision
  wrapper adapts `get_position` and `get_rotation`: the hack passes `double`
  locals' addresses, which a float rename would write eight bytes (as Moire).
- Celtic (`celtic_single.c`, issue #37) is registered after HexTrail and takes
  a median step of 32 ms against 832 as copied (`assessment_measured.md.in`).
  The wrapper is single precision (`rint` and `floor` mapped to float: the
  header maps only some libm calls) and frees the colour table that
  `celtic_init` callocs. `tools/celtic_patch.py` does three things at build
  time. It works `t*n - floor(t*n)` out once and clamps it: in float a fused
  multiply-add made it slightly negative and the hack's `assert()` aborted
  (the 3000-frame test found it). It sizes the graph to the screen: upstream's
  margins of -600 to -900 left most of a pattern off the screen, one held 1.1
  MB on the host, and 44 of 300 random starts needed more than the board's free
  heap by a model; grid and Kennicott now use minus one edge, and the triangle
  width/2 plus two edges (its incircle is half its circumradius). And it
  scales the 0.0001 animation tick by `CELTIC_STEP_SCALE` (default 4,
  `-DCELTIC_STEP_SCALE=<n>`), which cuts the shadow look-ahead's work and a
  picture's draw calls to 100 / scale. A test bounds a pattern's host bytes
  over 150 starts. Do not shorten `*delay2`: it is a 5 s dwell on the finished
  picture upstream. It looked like a pause on black while the shim's
  `erase_window` cleared first (issue #86); now the first call leaves the
  picture up and the wipe follows the wait. When forcing a graph type in a
  scratch copy to compare, set the margin for that type too, or the triangle
  comes out too small to fill the screen.
- A full-frame write to the PSRAM canvas costs about 25 ms whatever does it
  (memcpy, a 2 by 2 blow-up, 32-bit stores). Eruption at half size
  (`eruption_direct.c`, `lowrez_put.h`, `-DERUPTION_SCALE`) spent 26 ms of a
  39 ms step putting its image and 13 ms on the fire. `lowrez_put.h` wants
  ordinary RGB565, so Eruption's palette is swapped at `XAllocColor`.
  Interference changes every pixel each frame; telling it the window is half
  the size changes its look (its wave table is in window pixels), so it stays
  at 10.2 fps.
- Kumppa rotates its picture with `XCopyArea` from the window onto itself.
  The fine checkerboard in its trails is the hack's: copies are checked
  against a read-everything-first reference in every overlap direction.
- Piecewise (`piecewise_direct.c`) is single precision with `single_frand.h`
  (67 to 42 ms) and frees each circle's high fringe, which upstream's free
  forgets (32 blocks a stop; `leaks` named `fringe_add_intersection`).
- BinaryRing (`binaryring_single.c`) is told the display is 24 bits deep: its
  16-bit path drops every hit below one 5-bit step, so only green builds up
  and the picture stays dim. Its pixels go straight to the window as it plots
  them, and the frame's `XPutImage` into a pixmap and copy back are skipped:
  the 32-bit image was put a pixel at a time, 130 of a 240 ms step.
  `tools/binaryring_patch.py` makes `frand1`'s exponent literal a float.
- GraphStat's word list (`graphstat.h`) is written from `vendor/` at build
  time (`firmware/graphstat_strings.py`). Its timing is in frames tuned for
  30 fps, so at 10 fps a picture takes about 36 s instead of 12.
- EtruscanVenus (`etruscanvenus_single_gl.c`) runs on a 48 by 32 grid
  (`tools/etruscanvenus_patch.py`, upstream's 192 by 128) at half resolution;
  `-DETRUSCANVENUS_NUMU`, `-DETRUSCANVENUS_NUMV` and `-DETRUSCANVENUS_FULL`
  try others. Its modes differ: those that fill the screen are fill-bound.
  `curlicue.h` is copied from `vendor/` at build time (`vendor_data.py`).
- BlockTube was dropped after Rod saw it (too slow, and not much going on);
  its source, notices row, `blocktube_single_gl.c`, `gl_textures.py` and
  `blocktube_patch.py` stay, the wrapper out of the build filter. To bring
  it back, add `pre:gl_textures.py` to the three `extra_scripts` and
  `blocktube` (with its `count_the_blocks`) to `patch_float_literals.py`
  again. It drew 400 blocks
  (`tools/blocktube_patch.py`, upstream's 1,000) at half resolution with a
  10 ms delay (`"*delay: 10000"` in its registry entry, now gone);
  fog is applied per block from the modelview's z when the hack
  sets the block's colour, and a block the fog hides is not drawn. There is
  no sphere map, so each face shows the whole texture. Its texture is
  converted from `vendor/` at build time (`gl_textures.py`). It also filters
  `glEnable` of fog and texture generation, which TinyGL warns about every
  frame.
- Companion Cube and Floppy are not ported (Rod dropped both, listed in
  `failed_ports.txt`). Each Companion Cube face draws a 9,588-vertex disc:
  about 71,000 vertices a cube, three cubes by default. Floppy's model is
  26,340 vertices (about 630 KB of floats, which would not fit beside the
  current firmware in the 3 MB app partition).
- Rocks and DecayScreen were wired, looked at and dropped in the 2026-10-12
  second batch, then brought back after Rod asked for the shim to be extended.
  Rocks stamps every rock bigger than 3 pixels with `XCopyPlane` from a
  depth-1 pixmap it filled with `XFillPolygon`, and the shim drew nothing into
  a bitmap, so only the far dots showed (a 3,000-frame dump looked nearly
  black): `XFillRectangle` and `XFillPolygon` now fill bitmaps. On the board it
  runs at 17.2 fps against its own 50 ms delay, and holds 2.66 MB of PSRAM and
  all but 32 KB of the internal heap while it runs (about 180 of its 400
  bitmaps are under the 4 KB internal limit); a rotation soak that left it for
  DecayScreen read heap and PSRAM back at their idle values. DecayScreen smears
  the picture `load_image_async_simple` paints (see `x11shim.md`); without one
  it stays black. Both were host-only findings from a frame dump: look at later
  frames (3,000) as well as 400 before pinning, and read what the hack draws
  with.
- Penrose (`penrose_short_pause.c`, `tools/penrose_patch.py`) restarts after
  900 frames (about 10 s at 87 fps) instead of 3,141 once the screen is full
  and 31,415 after an error it thinks it can recover from, about six minutes
  at the board's rate with nothing moving. Upstream counts those pauses in
  frames, tuned for a fast X server, so other devices show the same stall.
- Triangle's palette is `make_smooth_colormap`, which the shim now builds the
  way upstream picks it (two to five random colours, any saturation, never
  black): its first version was always a bright rainbow, and every hack that
  asks for a smooth map changed colour with the fix (24 pinned hashes moved,
  all looked at).
- Abstractile holds about 6.1 MB of PSRAM while it runs (the free PSRAM read
  883 KB), so it leaves little for anything else: it stops and restarts clean
  in the leak test, but watch it in a rotation soak. Lisa takes 87 ms a step
  as copied (9.6 fps) and 52 ms in single precision (`lisa_single.c`, 14.3
  fps) with frames that look the same but hash differently. Worm's worms are
  one pixel wide and there are only a few on a 466 pixel screen: plain, for
  Rod to judge. XRaySwarm's host frames are white bugs and trails; whether
  that is its default look or a colour-table gap in the shim is unchecked.
- XJack (`xjack_xft.c`) types "All work and no play makes Jack a dull boy" in
  Special Elite on cream paper, through the Xft stand-in. Its first host frames
  were black (it clears the window, and the shim cleared to black) and then
  half-drawn lines scrolled in from the bottom (`xjack_init` sizes the page
  before its 40 pixel margins, and xscreensaver reshapes it after): the wrapper
  sets the window background and calls `xjack_reshape`, and each has a test
  that fails without it. Upstream's `font2` of 12 is small on a 466 pixel
  panel: Rod found 16 small and 20 good, so the wrapper overrides it to 20
  (`-DXJACK_FONT_PX=<12|16|20|24>`; about 28 columns). The margins swing
  (modes 1 to 4 walk them diagonally on purpose), and the round bezel cuts the
  corners of the page. Rod has seen it on the panel. It runs at 8-16 fps, set
  by its own delay (`wait` is 46-130 ms): typing is a 0.1 ms step, scrolling
  3.9-13.8 ms with pushes of 74-241 rows. It holds no PSRAM, and a 6-minute
  capture lost 352 bytes of internal heap in three steps (see
  `assessment_measured.md.in`).
