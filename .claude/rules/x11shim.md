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
- The board's `malloc` is `CONFIG_SPIRAM_USE_MALLOC=y` with
  `SPIRAM_MALLOC_ALWAYSINTERNAL=4096`: a block up to 4 KB comes from internal
  RAM and falls back to PSRAM when that is full, and a bigger one goes to PSRAM
  first. An allocation fails only when PSRAM is gone, so a hack's `abort()` on
  a failed allocation is not the risk it looks. The cost of a big pattern is
  internal heap drained to near zero (Celtic read 1,792 B free) and its data
  in slower PSRAM: compare the log's `psram=` with its idle value for the
  overflow.
- `erase_window` (`erase.c`) follows upstream's contract: the first call draws
  nothing and returns a state, so a hack that waits after it (Celtic's 5 s
  `*delay2`) waits on the finished picture. The second call starts a one-second
  wipe (blinds, circle, fizzle or random lines, picked at random), each later
  call draws the next step, and the call after the second is up clears the
  window and returns NULL. Progress comes from `double_time()`, not from the
  call count, so the wipe lasts a second at any step rate. A test or tool
  that does not sleep for a hack's delay must install a clock
  (`xshim_set_clock`), or a wipe never ends. Six hacks never call
  `eraser_free`, so the display tracks live states and the runner releases
  them when a hack stops.
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
  `uv run tools/softdouble_report.py <name>` does that for every object in
  the device build, by function: a count in a per-frame function is a literal
  or a macro the header could not reach. It lists none in Galaxy, Discrete,
  Drift, Flame, Substrate or Interaggregate now.
- Four things hide soft double from `single_precision.h`, each found by that
  report. `frand` (`single_frand.h`) and `MAXRAND`, `M_PI`, `M_PI_2` and
  `M_PI_4` (`single_constants.h`) are macros with a decimal literal in their
  body, past the literal pass; both headers are opt-in, since each moves the
  wrapper's frames and pins. Exponent literals (`1e-6`, `1.0E5`) were skipped
  by `float_literals.py` until Drift and Flame showed one each. A struct
  declared with `double` fields before the rename keeps them, so a float
  `corners[j].x * size` plus a `double` `pos.x` is a software add: HexTrail's
  `XYZ` (from `normals.h`) was 94 calls in `draw_hextrail`. Give the hack its
  own float struct under the same name, and `normals.h` another name for the
  double one (`hextrail_single_gl.c`; frames byte-identical). And a trig
  argument that is a big whole number cannot be a float: XSpirograph's second
  angle is millions of degrees and Vines' is an `int` in the billions, so a
  plain `double` to `float` turned both pictures to noise. XSpirograph reduces
  its angles mod 360 in integers at build time (`tools/xspirograph_patch.py`);
  Vines builds `(double) a` as `(long) a`, so its `cos` macro gets the exact
  integer and takes the phase as a 64-bit multiply by 2^64 / 2 pi, wrapping
  into whole turns (`vines_single.c`). Look at a frame before trusting a
  float build of anything whose angles grow. To find the line of a stray
  promotion, add `-Wdouble-promotion -fsyntax-only` to a file's compile
  command in `firmware/compile_commands.json` (`pio run -e stopwatch -t
  compiledb`; delete the file afterwards, it upsets the editor).
- A colour pixmap is a `Canvas`, and every drawing primitive draws on the
  drawable it is given (`xshim_target` in `pixmap.c`); `None` draws nothing,
  and a bitmap takes only `XCopyArea`, `XCopyPlane` and filled rectangles and
  polygons (see below). One exception still draws on the window whatever
  it is given: Deluxe's wrapper, which calls `stroke_circle(dpy->canvas,
  ...)`. No
  registered hack drew into a pixmap before this (the `HAVE_JWXYZ` wrappers
  and Deluxe's `doubleBuffer: False` turn double-buffering off), so keep
  those: a back buffer costs 434 KB and a full-frame copy, and dirties every
  row. Without a
  clip mask, `XCopyArea` copies colour rows whole with `canvas_paste_rect`. A
  pixmap wider or taller than `INT16_MAX` is `None` (the canvas's dirty spans
  are `int16_t`). Truchet draws into one (issue #61). The display tracks every
  pixmap a hack holds and the runner frees what a stopped hack left behind
  (Pacman leaked 514 KB per start). The GC keeps its own copy of a clip mask
  because hacks free the pixmap right after `XSetClipMask`. There is no PNG
  decoder: `image_data_to_pixmap` reads
  the raw blob described in `ximage-loader.h`, made by
  `tools/make_logo_blob.py` (`uv run`, needs Pillow). Pacman's sprites,
  XMatrix's large glyph sheets and the grab image are built with `--deflate`
  ("565Z", issue #102): 1.39 MB of flash saved, unpacked into a temporary
  buffer while the image loads (Pacman's is 522 KB of PSRAM, briefly), by the
  ROM's `tinfl` on the device and zlib on the host, so host builds link `-lz`
  (`platformio.ini`, `probe_hacks.py`, `gl_budget.py`). Host tests cover only
  zlib. XMatrix's small sheets are never drawn at 466 px: their generated
  headers `#define` them to the large ones. The `logo_180`/`360`
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
  lines, so they still take the stroker. The polyline's gaps were float
  rounding, not the joins or the point spacing: a pixel centre exactly on an
  edge shared by a segment quad and the join wedge at its end (any 45 degree
  edge through integer vertices) fell out of both, because float32 put the
  quad's right edge at 57.499996 for 57.5. `fill_convex` now takes a right
  edge inclusive to within `EDGE_SLACK` (1e-3; 1e-7 fails the test, 1e-5
  passes), and `test_wide_full_arc_fills_every_pixel_in_its_band` pins it. It
  moved five pinned frames by 0 to 33 pixels each (Deluxe, Forest, Rotor,
  Compass, Celtic). Pacman is not affected: its `wallwidth` is 1 at this
  size, which takes the thin-line path. Celtic's overpaint (about 50 paints a
  pixel a step on the host, 46% of its line draws zero-length) is still open
  in #56. Changing shared stroke code can move pinned frames: look at them and
  ask Rod. The pin test hashes steps 50, 100, 150 and 200 with a fake clock
  that carries across hacks, so a dump program does not reproduce its frames:
  write them from the test itself.
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
- `XGetImage` and `XPutImage` (`ximage.c`) move 32-bit ZPixmap images (the raw
  RGB565 value in each) to and from the window or a colour pixmap; a bitmap
  format, `None` or an empty region gives NULL. XMatrix flips its glyph sheets
  through them. Overrunning a row's right edge wraps onto the next row without
  AddressSanitizer seeing it, so a clip test must look at that next row.
- `XPutImage` copies a 16-bit image a row at a time with `memcpy`; other
  depths keep the per-pixel loop, since they keep only the low 16 bits (#82).
  ad-flyingtoasters at 2x: step 29.3 to 6.7 ms, 22.3 to 28.5 fps; at 1x: step
  15.9 to 3.7 ms. Fireworkx draws its own squares and did not change.
- `image_data_to_pixmap` accepts a NULL width, height or mask pointer (XMatrix
  passes no mask, and a NULL mask used to crash it on start); with no mask
  pointer no mask is made.
- `XClearWindow` clears to the colour `XSetWindowBackground` last set
  (default 0), and `xshim_release_pixmaps` resets it when a hack stops: the
  display outlives the hack, and Hexadrop's colour once turned Pyro's clear
  into a solid fill. No key events ever arrive, so `XLookupString` finds no
  characters, and an Xt timer (`X11/Intrinsic.h`) is never made.
- The shim still draws nothing into a depth-1 pixmap and draws every GC as
  `GXcopy`; it only remembers the function in `gc->function` now (GXcopy, GXor,
  GXxor). Halo and Moire2 handle both inside their own wrappers
  (`halo_planes.h`, `moire2_planes.h`).
- `screenhack.h`'s `frand` has a `1.0` inside its macro body, so the float
  literal pass cannot reach it and a single-precision wrapper still divides in
  double (software on the S3; `nm` shows `__divdf3`). `hacks/single_frand.h`
  redefines it in float; it is opt-in, because the older wrappers' frame hashes
  are pinned with the original.
- Added for the second batch, each test-first: 16-bit ZPixmap `XImage`s
  (canvas-order pixels, like every other value the shim hands out: `XAllocColor`
  -> `XPutPixel` -> `XPutImage` keeps the pixel; `XPutImage` does not swap.
  Only `lowrez_put_image` swaps, because Fireworkx writes ordinary RGB565
  bytes straight into the image data) and
  `ImageByteOrder`; `CoordModePrevious` in `XFillPolygon`, `XDrawLines` and
  `XDrawPoints`; `XCreatePixmapFromBitmapData` (X bitmap data is least
  significant bit first); a spline of our own (`spline.c`, not xscreensaver's);
  `XQueryColor`; `DisplayOfScreen` (the display opened last); the fill-rule,
  fill-style and stipple constants (accepted, ignored: fills are solid and
  even-odd); and `XSetFunction` and `jwxyz_XSetAlphaAllowed` (no-ops).
- Added for the third batch, each test-first: `ya_random` (the C library's
  `random()`, so `ya_rand_init` seeds it); `make_color_ramp` (the short way
  round the hue wheel, there and back when `closed_p`; ours, not
  xscreensaver's); `visual_depth` (always 16); `xshm.h` with
  `create_xshm_image`, `put_xshm_image` and `destroy_xshm_image`, which are a
  zeroed 16-bit `XImage` and `XPutImage` (`x11shim/xshm.c`); the arrow and page
  keysym constants (X's values; no key event ever arrives); and `glOrtho` (see
  `gl.md`). The Edit tool drops a trailing space from `old_string` and
  `new_string`: an edit that ends at a space in `platformio.ini`'s
  `build_src_filter` glued two entries together (it still built, which hid it).
- `allocate_alpha_colors` still finds no colour planes, so a hack that wants
  transparency (Qix, Goop) falls back to opaque layers and prints "couldn't
  allocate any color planes" on the host.
- The comment in `xshim.h` saying `XPutImage` byte-swaps a 16-bit image has
  been stale since commit 272ad6a: an `XImage` holds canvas-order pixels. A
  hack that builds its own 16-bit image must `px_swap` its pixels first.
- Added for the 2026-10-12 batch, each test-first: `XCopyArea` from the
  window (onto itself, rows moved with `memmove` in the order that reads each
  first, or into a pixmap); `XSetGraphicsExposures`; `make_color_loop` (ours);
  `grabclient.h` with `load_image_async_simple`, which finishes at once, with
  the grab image painted if one is set (below); `thread_util.h`, a pool that
  runs each thread in turn on the caller, as upstream does without pthreads;
  `visual_pixmap_depth`; `XCharStruct`; and `image_data_to_ximage` (RGBA
  bytes, bottom row first). `screenhack.h` now includes `visual.h` and
  `grabclient.h`, as upstream's does.
- `tools/wire_hack.py` puts a new hack before the After Dark toasters'
  `#ifdef` in the registry, the expected list and `kBaseline`.
- Added for the 2026-10-12 second batch, each test-first: `XClearArea` (the
  window background, clipped; a width or height of 0 runs to the window's
  edge; Worm), `XMaxRequestSize` (65535, so Lisa batches about 32,000 points a
  call) and the `LineOnOffDash` and `LineDoubleDash` constants (accepted;
  lines still draw solid; Penrose uses them only in its monochrome path). The
  scorer's gap list was wrong for Triangle (`free_colors` and
  `make_smooth_colormap` exist; its xlockmore include chain just does not
  reach `colors.h`, so `hacks/triangle_colors.c` includes `screenhack.h`
  first) and XRaySwarm (`initTime` is the hack's own, behind
  `HAVE_GETTIMEOFDAY`, which upstream takes from `config.h`:
  `hacks/xrayswarm_time.c` defines it). Putting `colors.h` in the shim's
  `screenhackI.h` would have fixed Triangle for all but changes what every
  xlockmore hack sees, so it is Rod's call.
- `XFillRectangle` and `XFillPolygon` fill a depth-1 pixmap now (Rocks): a bit
  is set where the GC's foreground is odd and cleared where it is even, and the
  clip mask and the GC's function are ignored. The polygon is even-odd, a pixel
  being in when its centre is, in float. Other primitives still draw nothing on
  a bitmap. Halo, Moire2 and Goop keep their own planes or layers and did not
  move.
- `xshim_set_grab_image` (`grabclient.h`) gives `load_image_async_simple` a
  picture to paint over its target the next time a load starts: scaled to cover
  and cropped evenly at the sides (a 4:3 screenshot on the square screen loses
  its left and right edges), mask ignored, finishing at once as before. `main.cpp`
  sets it from the header `firmware/grab_image.py` writes into the build
  directory: `firmware/assets/grab-image.png` (Rod's choice: the Windows 95
  screenshot from guidebookgallery.org, committed at his say-so, with a
  notices section), or `XS_GRAB_IMAGE=<file>`, and `XS_GRAB_IMAGE=none` builds
  without. It is stored at up to 466 pixels (a 466 by 350 blob was 340
  KB of flash, 48 KB since it was deflated in #102: the build had gone from
  2.39 MB to 2.74 MB, 87.0% of the app partition). The host tests set their
  own picture, so a pin never depends on the local file; `dump` sets it when
  the header exists. DecayScreen is unpinned for that reason
  (`test_decayscreen_melts_the_grab_image`). Boxfit's `grab` mode, off by
  default, would get the picture too.
- `make_smooth_colormap` follows upstream's behaviour (written from scratch, not
  copied): two to five random points in hue, saturation and value, repicked if
  adjacent ones are close or the average is too grey or dark, joined in a closed
  path with colours spaced by edge length. The first version always spanned the
  hue wheel at high saturation and value, so Triangle and 23 other pinned hacks
  were a bright rainbow.
- The Xft stand-in (`xft.h`, `xft.c`, `xft_font.h`; ours) draws 8-bit coverage
  glyphs, blended over the canvas pixel by pixel (the canvas is byte-swapped
  565, so each pixel is swapped, weighed in 8-bit channels and packed again),
  clipped in 64 bits and marked dirty once a glyph. It ignores the GC's clip
  mask, as Xft does. The glyph data is baked from `assets/fonts/` at 12, 16,
  20 and 24 pixels (`font_atlas.py`): the tables cost about 57 KB of flash
  (87.0% of the app partition became 88.8% with XJack), and a size is picked
  by the number at the end of a font name. `screenhack.h` does not include
  `xft.h`: a hack gets it through a wrapper (`hacks/xjack_xft.c`). With no
  font installed a hack gets a blank one (advance, no ink), because XJack
  `abort()`s on none and `test_pyro` and `test_push_present` run every hack.
- The runner never sends a hack a reshape, and `XClearWindow` clears to
  black unless the hack calls `XSetWindowBackground`, where xscreensaver's
  `screenhack.c` sets the window background from the `background` resource and
  sends a reshape after init. A hack that depends on either needs a wrapper
  (XJack's `xjack_init_on_paper`); doing it in the runner would change what
  every hack sees, so it is Rod's call. Any other registered hack with a
  light `background` that calls `XClearWindow` may have the same fault.
- The shim's `screenhack.h` does not include `screenhackI.h` (upstream's
  does), so plain hacks and xlockmore hacks see different headers. Put a
  macro upstream defines in `screenhackI.h` for every hack into a small
  header both include, as `nls.h` (`_()` and `gettext`) is.
