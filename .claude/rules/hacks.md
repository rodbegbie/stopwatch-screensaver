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
  `srandom(atoi(getenv("SEED")))`. Each boot on the device picks a new mode
  too, so measure each mode from separate boots.
