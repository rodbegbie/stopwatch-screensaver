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
  so about 11-13 fps (14 with 64-byte cache lines); overlapping the push with
  the next step (`repaints_all`) takes it to 19 fps. Other ideas are in issue
  #54.
- Eight of the S-hack batch (Anemone, Anemotaxis, Compass, FuzzyFlakes,
  Halftone, Interaggregate, LMorph, Wormhole) are built through
  `hacks/<name>_direct.c` (pixmap double buffering, see `techniques.md`;
  LMorph's is `lmorph_single.c` because it needs no `HAVE_JWXYZ`). All but
  Anemotaxis and Wormhole, and Euler2D (`euler2d_single.c`), also use
  `single_precision.h` plus build-time float literals (`patch_float_literals.py`
  lists them). `single_precision.h` leaves `floor` and `ceil` on the double
  versions: Euler2D's wrapper maps them itself. After the literal pass the
  objects still showed soft-double helpers: Interaggregate's were `frand` in
  `build_field` (fixed with `single_frand.h`); Halftone's, Anemone's and
  Euler2D's are in `*_init` only, once a start. `tools/softdouble_report.py`
  lists them by function.
- To see which hacks are worth a look, flash a build with `-DROTATE_SECONDS=20`,
  log the serial port for one lap (about 40 minutes), and run
  `uv run tools/triage_hacks.py lap.log --soft` (after `pio run -e stopwatch`).
  It sorts each hack into step-bound, push-bound or paced (its own delay is the
  biggest part of the frame) and puts the soft-double calls in its object
  beside it. A hack that is step-bound with calls is a wrapper candidate;
  step-bound with none is in its drawing, not its maths (Braid, Marbling,
  Discrete, BinaryRing); paced ones gain only what the step saves, down to the
  delay (Forest 500 ms, Vines 200 ms). A wrapper that moves nothing is worth
  dropping: Bouboule built single stayed at 14.5 ms, because its cost is
  ellipse fills.
- `test_frames_of_every_hack_but_maze_match_main` stops at the first hash that
  differs, in registry order, so repinning several hacks takes one test run
  each (about 85 s), earliest first. A hack whose frames do not change under a
  wrapper (Compass and Halftone in single precision) keeps its hash.
- Anemone's step moved between 31 and 56 ms as its arms grew and shrank, so its
  fps was 11 to 15.5 whatever one 20 s window said. Upstream opens every frame
  by filling the whole window black, which is a full frame of PSRAM writes
  (229,000 pixels a frame on the host, the strokes only about 16,000 of them),
  so `anemone_direct.c` erases only where last frame's lines were
  (`span_erase.h`, each line's box grown by its round caps): 31,000 pixels, the
  same frames, and only those rows dirty. On the board, over one 20 s window
  per lap: 30.5 fps mean (worst 20.4), step 25.2 ms. Counting pixels and
  spans written a frame on the host (a temporary counter in `hspan`) is a
  quick way to see what a hack costs before the device does. Forest waits
  500 ms between trees by design (about 2 fps, and the push is 88 rows).
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
  14.6-15.2 fps translucent. The shared `canvas_fill_polygon` is integer now
  (see `x11shim.md`; Hexadrop has its own convex float fill). Goop is built through
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
- GraphStat's word list (`graphstat.h`) is copied from `vendor/` at build
  time (`firmware/vendor_data.py`); since 6.17 upstream ships the header, each
  word wrapped in `_()` for `gettext` (`x11shim/include/nls.h`). Its timing
  is in frames tuned for 30 fps, so at 10 fps a picture takes about 36 s
  instead of 12.
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
- The 2026-10-13 sixth batch (Marbling, Deco, IMSMap, MetaBalls, Munch, CCurve,
  BinaryHorizon, RotZoomer, Twang, NerveRot, Flow, Bouboule, Ant, HyperBall,
  Demon, Droste, Polyominoes, Barcode) passed every host test and then four
  of them failed on the board, none of which a host frame could show: a stack
  overflow (Polyominoes), an allocation too big for PSRAM that the host's lazy
  memory allowed (Flow), 0.6 fps (Marbling) and 1.6 fps (Droste). Flash a
  batch before calling it done. A rotation build starting at the first new
  hack (`-DSTART_HACK=\"marbling\" -DROTATE_SECONDS=30`) measures all of them
  in one 10 minute capture. Rod then took MetaBalls, Twang, Flow and Droste
  out of `g_hacks[]` (slow or margin-bound, not worth tuning). Their sources,
  wrappers and wrapper tests stay in the tree, unregistered; to bring one
  back, re-add its registry entry, expected name, pin and leak test.
- Marbling does a 5-iteration fractal noise per pixel (12 ms a step on the
  host, 1,785 ms on the board, so the host suite takes about 3 minutes
  longer). `marbling_safe.c` overrides `*gridsize` to 6 and `*iterations` to 3
  (5.4 fps, cells of 6 pixels). Upstream's `marbling_free` never frees its
  colour table (5 KB a start): the wrapper does.
- Polyominoes (`polyominoes_iterative.c`, `tools/polyominoes_patch.py`) panicked
  the board with a double exception inside `count_adjacent_blanks`: a flood
  fill that calls itself once for each blank cell, up to 360 deep in several
  puzzles, 32 bytes a level on top of the solver's frames. The build now
  replaces it with a loop over an explicit stack (frames identical to the
  recursive build). Read a puzzle hack's grid sizes, not just its frame size.
- Flow's `csegs` is `(count + 36) * ncolors * taillen` segments of 8 bytes:
  upstream's 3,000 bees and 200 colours is up to 29 MB, which the host
  provided lazily and the board refused, so the hack drew one frame and
  nothing more. The registry gives it 120 bees and 8 colours. In single
  precision its flow dies out within about 600 frames (the screen stays
  black), so it is double: 3.0-4.8 fps, about 200 ms of every step being its
  search for the next flow. A single frame at 300 frames looked right in
  both: sweep several frame counts (300, 1000, 3000) for a hack that
  restarts itself or can die out.
- CCurve's `*limit` of 200,000 segments (each a malloc and a recursion) took
  740 ms in single precision and 2.9 s in double; the wrapper caps it at 30,000.
  Its 1.4 fps is its own pacing: 0.4 s between iterations, 42 ms of work.
- Droste (`droste_small.c`) hands back a loader token on the first call and
  finishes on the next, with the pixmap's size as the rectangle, so upstream's
  own "just finished" branch runs. It loads a picture the size of the window,
  not 2.5 times it (5.4 MB as the 32-bit image `XGetImage` makes), and renders
  at half width and height through `lowrez_put.h` (630 ms became 190 ms, 4.6
  fps). `XPutPixel` is mapped to swap the pixel, since `lowrez_put.h` wants
  ordinary RGB565 and the picture holds canvas order. MetaBalls takes all but
  28 KB of the internal heap while it runs (466 row buffers of 466 bytes).
- RotZoomer and Twang (`rotzoomer_single.c`, `twang_single.c`) start their
  picture and tiles the first time `draw` finds `load_image_async_simple`
  finished. The shim finishes at once and returns NULL, so that branch never
  ran and the first draw used a NULL zoom box (exit 139). Each wrapper does
  the finishing step straight after init. Changing the shim to return a loader
  would also change Boxfit and DecayScreen, so it is Rod's call. Both are
  single precision (they call `sin` and `cos` in double for every pixel).
  RotZoomer is unpinned in `test_hacks.c`: with no grab image every frame is
  black.
- BinaryHorizon (`binaryhorizon_swapped.c`) is told the display is 24 bits
  deep. Its 16-bit path adds less than a step of a channel per hit and rounds
  its random colour walk down, so every colour drifted to black or a dim
  green (as in BinaryRing). Pixels go straight to the window, and its image
  and pixmap are one pixel big, because nothing reads them. Every 30 s it
  restarts and callocs a new pixel buffer without freeing the old one (868 KB
  on the device): the wrapper frees it and clears the window.
- Munch defaults to `*xor: True`, so `XDrawPoint` honours `GXxor` now (only
  that primitive: lines and fills still copy). `pow2.h` is our own
  `i_log2` and `to_pow2`. IMSMap needed 1-bit `XYBitmap` images, `BitmapPad`
  is 32, and Deco needed `allocate_writable_colors` and `XStoreColors` (no
  writable cells). The scorer's gap lists were wrong twice: MetaBalls'
  `XListPixmapFormats` is under `#if 0`, and Ant needed only `automata.h`, a
  file in `hacks/` that is now copied to `xs_support/`.
- NerveRot passes the address of its own `double` locals to `rgb_to_hsv`, so
  its single-precision wrapper has an adapter (as Moire's). The restart test
  under AddressSanitizer found it as a stack-buffer-overflow, but a host frame
  looked right.
- Bouboule's upstream default is a red and a blue starfield for 3D glasses,
  drawn with `GXor`, which the shim does not do. The registry overrides
  `*use3d: False` (a single colour cycling through the palette) and
  `*cycles: 0`. Twang's 3 by 3 tiles of 120 pixels leave black margins and
  clip at the round bezel; `*tileSize` is the lever.
- A wrapper that finishes an image load itself (RotZoomer, Twang, Droste) must
  also handle the hack's timed reload, which the shim finishes at once too, so
  upstream's "just finished" branch never runs again. It must also match image
  layouts: `XGetImage` gives a 32-bit image, and a hack that memcpys it into
  its own 16-bit buffer shows alternating black pixels. RotZoomer did on the
  board until the trail review caught it. Test with a flat grab image and
  assert every pixel, then change the image before a timed restart.
- A wrapper that clears state before upstream's own timed check (BinaryHorizon
  frees its buffer) reads the clock separately from upstream, and the two reads
  can straddle a second. Read it once a frame and route upstream's `time()` to
  that reading with a function-like `#define time(p)`; test it with a fake
  clock whose second read is later.
- A hack that leaks on every in-hack restart (Munch's `get_string_resource`
  result) needs a restart-loop leak test of its own: `test_hacks` restarts only
  through start and stop.
- Deluxe's `draw_star` (ten sin/cos in software `double`, #54 idea 5): a
  single-precision build gave identical frames and no device gain (step
  45.06 vs 45.12 ms, pinned, 2 rounds). Skip it; the step is the blend, the
  mask and the strokes.
- Bubble3D is four files (`b_lockglue.c` holds `XSCREENSAVER_MODULE`, so the
  scorer knows it as `b_lockglue`; `SOURCE_STEMS` in `score_hacks.py` maps the
  registry name `bubble3d` to it), each built as its own `USE_GL` unit by
  `bubble3d_*gl.c`. Only `bubble3d.c` has a licence text; Rod chose to list the
  other four under its notice. The glue's `init()` builds a sphere and drops it
  (leaked on every start: the wrapper makes that call return NULL). A GL
  profile put 220 of a 300-390 ms step in a software-double `cos` per nudge
  axis per vertex: `bubble3d_bubble_gl.c` is single precision (frames
  byte-identical). Upstream's own `GLB_SLOW_GL` (set in `bubble3d_gl.c`) takes
  the sphere from 1,280 to 320 triangles: 4-5 fps became mean 11.5, worst
  window 8.2, with the push (12-24 ms, a full frame) now a fifth of the frame.
  It is flagged `repaints_all` since the GL layer follows the canvas swap
  (issue #118): 11.2 to 12.0 fps, worst window 8.0 to 8.8. Upstream's
  default `transparent` skips the depth test and expects a blend TinyGL's fill
  does not do, so back faces overwrote front faces as a dark wedge in each
  bubble (found by the trail review): the registry overrides `*transparent:
  False`, which enables the depth test. It cost nothing measurable on the
  board (mean 11.5 before, 11.4 after).
- Bubbles (`bubbles_safe.c`, `bubbles_sprites.py`) is three copied files and
  44 PNGs (four sets of eleven sizes: blood, blue, glass, jade) that the
  pre-script deflates into the build dir with `make_logo_blob.py --deflate`,
  never committed. Upstream's free leaks every live bubble in the mesh lists
  and one extra step pixmap: the wrapper frees both, and the leak test fails
  if either free is removed (checked by mutation). `bubbles.h` includes
  `<X11/Xlib.h>`, which the shim now answers with a header that pulls in
  `xshim.h`. A host dump always shows the same set, so seed `random()` to see
  the others (jade, blue and glass came up in six seeds, blood did not).
  Blood is teal with red mottling in the source art, not red: Rod saw cyan
  with red spots on the panel and it matches `blood11.png`, so it is not a
  byte swap. A single frame can show a bubble with a bite out of it while
  neighbours merge and are redrawn; the bite is gone a few frames later. On
  the board it ran at 54-62 fps (mean 57.4), a 5-7 ms step under its own
  9.5 ms wait, and held nothing in PSRAM: it is paced by its delay, not by the
  work.
- The 2026-10-18 GL batch (GLMatrix, CubeStack, Kaleidocycle, DangerBall,
  Cubenetic, DeepStars, Superquadrics, Menger) is eight `USE_GL` wrappers over
  unmodified sources. Most GL hacks wait 20-40 ms after a frame (`*delay`), and
  the loop credits only the push against it, so `kShortDelayOverrides` (10 ms)
  took CubeStack from 16.5 to 22 fps, Kaleidocycle 19.6 to 33 and DangerBall
  16.5 to 25 with no other change. Their motion is per frame, so it is faster
  too, nearer upstream's pace on a desktop.
- CubeStack (`cubestack_gl.c`) blends `GL_SRC_ALPHA, GL_ONE` with an alpha of
  opacity times each face's transition, and TinyGL ignores vertex alpha: the
  wrapper's `glColor4fv` scales its colour (the first frame is black, where
  10,883 pixels were lit without it), so the cubes are translucent as upstream
  draws them. Upstream wraps its palette index with `>` instead of `>=`, so
  after about 12,900 frames it reads `colors[32]` of 32 (an AddressSanitizer
  overflow in a 13,500 frame test): the wrapper wraps it before each draw.
  Kaleidocycle also blends but its faces are opaque, so it was not folded.
- Kaleidocycle starts with `count = 0`, so its first frame computes a piece's
  angle as 0 / 0: a NaN that reaches TinyGL's viewport conversion and is cast to
  an int (the trail review found it with `-fsanitize=float-cast-overflow`:
  `PLATFORMIO_BUILD_FLAGS='-fsanitize=float-cast-overflow
  -fno-sanitize-recover=all' pio run -e dump`, then dump one frame).
  `tools/kaleidocycle_patch.py` guards the denominator; `kaleidocycle_gl.c`
  includes the build's patched copy (`generated/kaleidocycle_patched.c`, written
  by `patch_float_literals.py` with no literal pass), and frames are unchanged.
- Superquadrics' `myrand` multiplies in `float` (upstream too), so the largest
  `LRAND()` rounds up and `myrand(4)` is 4, one past `pats[4][4]`.
  `tools/superquadrics_patch.py` clamps it at build time; its test compiles the
  real function with `LRAND()` fixed at 2147483647 (the trail review found it).
- GLMatrix (`glmatrix_single_gl.c`) keeps its look in alpha: the texture's
  alpha is the glyph image's green channel and each glyph's brightness is the
  vertex alpha, drawn `GL_SRC_ALPHA, GL_ONE`. TinyGL has none, and the shim's
  `glTexImage2D` drops alpha, so glyph cells were solid green squares. The
  wrapper premultiplies the texture as it uploads and has `glColor4f` scale its
  colour: an additive blend cannot tell the difference. `xlockmore.h` must come
  before the `#define glTexImage2D` (it brings in `glshim.h`, whose macro would
  win). `matrix3.png` is deflated by `gl_textures.py` (`--deflate`). The step
  grows as the rain fills the screen (9 to 140 ms over about 80 s at full size)
  and fill is 100 of 145 ms: it renders at half size (`-DGLMATRIX_FULL` keeps
  full size) with `*density: 12` (26 strips, not 44) and `*delay: 10000`, about
  17 fps once full. Single precision gave byte-identical frames and about 3 ms.
  Upstream's brightness wave index runs 1 to `WAVE_SIZE` and the table has
  `WAVE_SIZE` entries, so a zero remainder read the next screen's state, a
  glyph at a junk brightness now and then (the trail review found it);
  `tools/glmatrix_patch.py` wraps it to 0, the brightest entry, which follows 1
  in the cycle. Its test compiles the hack's own statement over every row and
  wave position.
  The vertex alpha is clamped to 1 (`hacks/fold_alpha.h`, tested directly): a
  spinner glyph gets 1.5 times its brightness, and TinyGL masks a channel past
  full instead of saturating it, so a reproduced alpha of 1.087 gave green 5/63
  and the leading glyphs went dark (a trail review finding). A frame metric
  could not see it (a handful of pixels), so the helper is a plain header.
  Its texture setup also packs `a << 24` with an `unsigned char` promoted to a
  signed `int`, which overflows for an alpha of 128 or more (a sanitizer sweep
  found it): the patch widens it to `unsigned long`, the same bits.
- Cubenetic (`cubenetic_single_gl.c`, `tools/cubenetic_patch.py`) works out a
  256 by 256 plasma texture every frame, a square root per wave per pixel: 847
  ms a step, 1.2 fps. At 64 by 64 (`-DCUBENETIC_TEXTURE_SIZE`) it is 12-18 fps.
  The restart leak test found that `free_cube` never frees the cube array
  (320 bytes a start); the wrapper frees it. It passes the addresses of
  `double` locals to `get_position` and `get_rotation`, so it has the adapters
  HexTrail has.
- Superquadrics and Menger draw every face as a `GL_POLYGON`, which TinyGL
  drew as nothing until ARS-18 (both were black, and each wrapper mapped it
  to a triangle fan). Both render at half size
  (`-DSUPERQUADRICS_FULL`, `-DMENGER_FULL`); Superquadrics is single precision
  (13 to 19 fps, 8 to 11 before). Menger's depth 3 is 36,000 triangles and 920
  ms a frame, so the registry caps `*maxDepth` at 2 (11 to 44 fps, the low end
  while a level's display list is built).
- DeepStars was dropped after Rod saw it: about three dim stars on screen and
  no smear. Its source, notices row and `deepstars_gl.c` stay, the wrapper out
  of the build filter. The wrapper draws each smear copy at a lower alpha
  folded into the colour (TinyGL's blend is right only for black and white,
  so the blend is never enabled) and swallows `glEnable(GL_POINT_SMOOTH)`,
  which TinyGL warns about every frame. Upstream's stars are colours 0.15 to
  0.45 and 0.5 to 3 pixels across, so with no smooth points or real alpha they
  are faint and few, and `smear` only starts to grow after a random 1 in 800
  frames. It grows to 400 copies of 2,700 stars, 3.4 fps at 20, so the
  registry capped it at 4 (`*smear: 0.01`, now gone). To bring it back, add its
  registry entry (with that override), expected name, pin and leak test again.
- Circuit (`circuit_gl.c`, ARS-55) is the first port to lean on the TinyGL
  `GL_POLYGON` fan (ARS-18): its seven-segment display is `GL_POLYGON`
  hexagons, so it needs no `glBegin` wrapper, and a test counts pure-red pixels
  over a run (3,166 at most; nothing else it draws is pure red). The first
  host run aborted in the 3,000-frame test on two over-reads (see `gl.md`).
  `free_circuit` never frees the components still on the board, up to ten, so
  every stop leaked them: the wrapper frees them (payload, a diode's band, the
  component) before upstream's free, and the leak test fails without it. At
  full size on the board, over 240 s pinned: 8.4-11.4 fps (mean 9.7), step
  60-93 ms growing as components arrive, push 24.8 ms (every frame is a
  full push: the grid fills the screen), no wait, PSRAM flat. `gl_budget.py`
  predicted 79-85 ms a frame (12 fps) against about 103 measured (step plus
  push). It counts lines as free, and they are not (below).
  A single-precision build (ARS-55, committed and reverted: `b32fe24`,
  `bc549b2`) took `tools/softdouble_report.py`'s count in the wrapper's object
  from 198 call sites to 14 (the hack has no `double` keyword; its literals,
  `0.02` and `0.84`, were the cause) with host frames byte-identical, and moved
  the board by nothing: double and float alternated over two rounds, 28
  windows each, 9.76 vs 9.79 fps and a 77.6 vs 77.3 ms step. A `-DGL_PROFILE`
  build says why. Of a mean 79.8 ms step, 40.0 ms is the 156 lines of the
  board's grid (0.256 ms a line, every frame), 15.0 ms is the full-screen
  clear (the grid fills the box), about 18 ms is vertex setup, lighting,
  triangles and fills (growing from 1 ms to 32 ms as components arrive), and
  the hack's own code is 6.3 ms: the most any arithmetic change could win is
  8 percent. The lever is the grid's lines, not Circuit's maths.
  `-DCIRCUIT_HALF` renders at half the width and height
  (`glshim_set_render_scale(2)`, as Menger does). Alternating it with full
  size on the board, two rounds each (about 57 windows a build): full 9.67
  fps (8.4-11.6, step 78.6 ms), half 14.78 fps (13.0-18.4, step 42.1 ms); the
  push is 24.9 ms either way. Rod found the half-size picture noticeably
  blocky against the fine grid lines (2-pixel lines, chunky resistor bands and
  display dot), so it is opt-in, not the default. It leaves 217 KB more PSRAM
  free (6,049,231 against 5,832,095; `psram=` is free memory, and an earlier
  note here said the opposite). A scale-2 hack cannot overlap its push, so the
  registry flags Circuit `repaints_all` only without `-DCIRCUIT_HALF`.
  The flag earned its place narrowly. Pinned, alternating with
  `-DPUSH_NO_OVERLAP`, two rounds each (about 57 windows a build): plain 9.50
  fps (step 80.5 ms, push 24.8 ms), overlapped 10.29 fps (step 97.2 ms, push
  0.1 ms), +8.3 percent; window by window the ratio averaged 1.08 (0.98 to
  1.14). The step grew 17 ms, three times the usual 5, because both cores
  contend for the PSRAM cache and Circuit's lines are miss-heavy (a cache
  simulation of its real line pixels gives about 33,000 misses a frame, 1.1
  us each, which matches the 40 ms), so the 25 ms push hid only 8 of its
  milliseconds. It also takes 8.7 KB of internal heap.
  Line batching (see `gl.md`) then took the registered build from 10.59 to
  13.55 fps over two alternating rounds (step 93.2 to 72.3 ms, worst window
  9.2 to 11.6, best 16.2), with frames byte-identical. That is full size and
  crisp, but still under the 15 fps bar. The profile has not been taken again
  with batching on; before it, the clear was 15 ms and vertex setup, lighting
  and fills about 18 ms of the step.
- Phosphor (`phosphor_tty.c`, ARS-58) is an unmodified `phosphor.c` and
  `ansi-tty.c` behind a wrapper. Upstream reads a program on a pty
  (`xscreensaver-text`); the wrapper renames the `textclient_*` calls to a
  text source of its own (`phosphor_text.h`, tested directly): it `cat`s one
  of seven public-domain poems, picked at random and never the one just shown,
  typed a character a frame. The shared `textclient.h` stays a NULL source,
  which XMatrix relies on. `firmware/assets/phosphor-poems.txt` is the source
  text; `tools/make_phosphor_poems.py` word-wraps it to 18 columns (4 in, the
  columns the round panel shows on rows 2 to 13: the corners of the 25 by 15
  grid are off the glass) and pages it into `phosphor_poems.h`. Nothing
  scrolls: Phosphor leaves a ghost of every character a scroll moves, which
  read as noise under the new text, so a page ends with `--More--`, a pause
  byte (`\001`) and then a clear. Only poems published before 1931 go in
  (`THIRD_PARTY_NOTICES.md`); the Raven, Ozymandias, two Frost poems, Williams's
  Red Wheelbarrow (1923), Blake and Dickinson are in. The wrapper overrides
  `*phosphorScale` to 3 (upstream's 6 is a 12 by 7 grid on 466 pixels) and
  `*cache: False` (a cache of every fade of every glyph is about 20 MB). The
  builtin 6 by 10 font is `images/gen/6x10font_png.h`, a blob made by
  `make_logo_blob.py --deflate` (1.3 KB). Upstream's glow is a fill through a
  clip mask, which `XFillRectangle` ignores, so the wrapper maps it to
  `xshim_fill_rect_clipped`: making the shared call honour the mask moved
  Bubbles' and Pacman's pinned frames, and without any such fill every
  letter was a solid cell (`test_phosphor_draws_letters_not_solid_cells`).
  Upstream's free leaves the tty grid, the Xft handle, three of its four
  glyph sets' structs and the first font pass's three GCs (it runs the pass
  twice and overwrites its GCs); the wrapper frees all of them. On the board:
  13.0-17.6 fps (mean 16.2), paced by its own 50 ms delay while typing (step
  1.4-2 ms) and up to 22 ms when a page clears and every cell fades at once.
  `ansi-tty.c` writes to `stderr` only with `debug_p` set, and on the board
  that expands to `_impure_ptr`, which the toolchain lacks: `blurb.h` gives
  the file a null `stderr` there. `pio test` and `pio run -e stopwatch` must
  not overlap: the test run clears shared build state and the flash fails
  with a missing `firmware.elf`.
