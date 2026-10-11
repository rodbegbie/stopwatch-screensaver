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
  `hacks/pacman_loop_ai.c`): it always plays the fixed level, because the
  random generator recursed to ~315 KB and boot-looped the board, and the
  ghosts' route search is a loop (it was 453 levels deep). Each patch checks
  what it replaces. Its sprite sheet is built from `vendor/` into the build dir
  by `firmware/pacman_sprites.py` and never committed (3 MB as C text).
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
  `srandom(atoi(getenv("SEED")))`. A device boot is deterministic too (Halo
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
  in float and took Starfish from 8.5 to 20 fps and Goop from 7.2 to 12; Goop
  then draws straight onto the window (its `XCreatePixmap` is the window and
  the `XCopyArea` is skipped), 16 fps. It is not wired into the shim: the
  shared `canvas_fill_polygon` is still double (Hexadrop has its own convex
  float fill). Goop draws opaque layers (`goop_direct.c` takes the
  `HAVE_JWXYZ` path); upstream's translucent overlap needs colour planes.
- Fireworkx renders at 1/`FIREWORKX_SCALE` of the width and height
  (`hacks/lowrez_put.h`: it is told the window is smaller and `XPutImage`
  draws each pixel as a square): 2.4 fps as copied, 11 at a scale of 3, 12.4 at
  4. Its glow, flash pass and 565 conversion are full-frame walks over PSRAM.
  Its header says "(GPL)" but the text is the standard permission notice.
- Loop asks for 100 ms between generations (8.7 fps with a 3.5-7 ms step); the
  registry overrides it to 50 ms. Hypnowheel is 1.4 fps and stays under the bar:
  about 70 percent of its step is additively blended pixel fill (about 1.2 us a
  pixel) and 30 percent is per-triangle setup (about 35 us), so `*layers` and
  `*count` cannot reach 15 fps.
