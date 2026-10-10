---
paths:
  - "firmware/src/x11shim/**"
  - "firmware/src/hacks/registry.*"
  - "firmware/src/hacks/*_single.c"
  - "firmware/src/hacks/single_precision.h"
  - "firmware/platformio.ini"
  - "firmware/logo_override.py"
  - "firmware/patch_float_literals.py"
---

# X11 shim and framework gotchas

Moved verbatim from the Gotchas section of AGENTS.md.

- `unsigned long` is 4 bytes on the device and 8 on the host, so a hack that
  allocates arrays of it (Substrate's two 466 by 466 buffers) is twice the size
  on the host, and host leak numbers are twice the device's. On the device,
  plain `malloc`/`realloc` of about 868 KB reached PSRAM (free PSRAM fell by
  1.74 MB) and came back on restart; internal heap barely moved.
- xlockmore hacks (the 40 that include `xlockmore.h`) need `-DSTANDALONE`,
  which every PlatformIO env and `score_hacks.py` set; without it they
  include `xlock.h` instead. The envs also define `HAVE_MOBILE`, because
  otherwise each hack's `XSCREENSAVER_LINK` defines the same global
  `xscreensaver_function_table` and two hacks fail to link; the only other
  effect is an inert `*ignoreRotation: True` default. Register one in
  `hacks/registry.c` with `XLOCKMORE_HACK(<name>, "<Class>")`. The runner runs
  the hack's `setup_cb` once and passes `setup_arg` as `init_cb`'s hidden third
  argument, as xscreensaver's `screenhack.c` does. Its table is empty until
  then. Resources the framework reads but a hack does not define
  (`delta3d`, `size`, ...) fall back to `kFrameworkDefaults` in
  `x11shim/resources.c`. A `HackEntry`'s `overrides` list beats the hack's own
  defaults (Galaxy runs with `count: -3`, at most three galaxies; a count of
  -2 or above skips the hack's restart cleanup and leaks); register it with
  `XLOCKMORE_HACK_WITH`. Galaxy, Drift, Discrete and Flame are built through
  `hacks/<name>_single.c`, which includes the framework headers, then
  `hacks/single_precision.h` (`double` and the libm calls become `float` and
  the `f` versions; the S3's FPU is single-precision only), then the unmodified
  hack; the original `.c` is excluded from each env's `build_src_filter`.
- Wide lines (`line_width` above 1), arcs and pixmap writes live in
  `x11shim/stroke.c`, `arc.c` and `pixmap.c`. Width 0 and 1 keep the old
  `canvas_line` path, so hacks that set no wide width are pixel-identical (every
  hack's frame is pinned in `test_hacks.c`). Braid is the one registered hack
  that draws wide lines (random width 1-7; Maze's width 2 is under
  `HAVE_JWXYZ`). A round-capped segment draws a disc at each end, so
  `stroke.c` keeps each width's rows in a table (widths to 16) rather than
  calling `canvas_fill_ellipse`, whose `double` maths is software on the S3. A
  test pins the pixels to `canvas_fill_ellipse`.
- `single_precision.h` renames the keyword `double`, not literals: `0.5 * x`
  is still software double. Braid's wrapper includes a copy of the hack with
  every decimal literal suffixed `f`, written at build time by
  `firmware/patch_float_literals.py` (`tools/float_literals.py`). Check a
  wrapper's object for `__muldf3` and friends with `xtensa-esp32s3-elf-nm`.
  The older wrappers (Galaxy, Drift, Discrete, Flame, Substrate) have not been
  checked.
- Only `XCopyArea` writes into a pixmap (`XCreatePixmap`, same depth); drawing
  primitives given a pixmap still draw on the canvas. The display tracks every
  pixmap a hack holds and the runner frees what a stopped hack left behind
  (Pacman leaked 514 KB per start). The GC keeps its own copy of a clip mask
  because hacks free the pixmap right after `XSetClipMask`. There is no PNG
  decoder: `image_data_to_pixmap` reads
  the raw blob described in `ximage-loader.h`, made by
  `tools/make_logo_blob.py` (`uv run`, needs Pillow). The `logo_180`/`360`
  headers are aliases of the 50 px data, as a 466 px screen only picks 50.
  `XS_LOGO=<image>` (relative to the repo root) swaps the logo for a local
  image, shrunk to fit 50 px, without committing it: `firmware/logo_override.py`
  writes the header into the build dir, where `maze_patched.c` finds it
  before the committed one. Unset, it deletes that header. Needs `uv`. A
  third-party logo kept under `vendor/` stays out of git that way. The WorkOS
  icon is `vendor/workos/workos-icon-256.png`. A plain flash with it is
  `unset PLATFORMIO_BUILD_FLAGS` then `XS_LOGO=vendor/workos/workos-icon-256.png
  pio run -e stopwatch -t upload`: build flags stick, so a leftover pin from
  an earlier probe would survive. The logo shows in Maze only.
- A wide circle through `XDrawArc` is a polyline of about a thousand wide
  segments, each filled a row at a time: 40 times the fills of a ring, and the
  polyline leaves gaps in it. `stroke_circle` (`x11shim/stroke.c`) fills the
  ring in two spans a row. Only Deluxe's wrapper calls it, and it does full
  circles only: Pacman's wall arcs are quarters and Celtic's wide strokes are
  lines, so they still take the stroker. Issue #56 asks whether the stroker's
  gaps and overpaint touch them. Changing shared stroke code can move their
  pinned frames: ask Rod.
- `XshimGC.alpha` (0 is opaque, 1 to 31 is the weight out of 32) makes the
  wide-line code in `stroke.c` blend over the canvas instead of overwriting it;
  fills, `canvas_line` and the other primitives ignore it. The canvas is
  byte-swapped RGB565, so the blend swaps each pixel and back. A shape of
  overlapping pieces (a polyline, a round cap and its segment) sets bits in a
  mask the size of the canvas (a 27 KB `calloc` per shape, freed at the end)
  and blends it once, or the overlaps blend twice and a star's corners show
  as bright diamonds. A failed `calloc` falls back to blending piece by piece.
  Setting `alpha` takes a wrapper, as in `hacks/deluxe_opaque.c`.
- Don't declare `xrealloc` or `xmalloc` in the shim: cloudlife defines its own
  static `xrealloc`, which would clash.
