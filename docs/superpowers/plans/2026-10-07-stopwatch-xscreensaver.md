# xscreensaver on the StopWatch: implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use
> superpowers:subagent-driven-development (recommended) or
> superpowers:executing-plans to implement this plan task-by-task. Steps use
> checkbox (`- [ ]`) syntax for tracking.

**Goal:** Run xscreensaver's Pyro on the M5Stack StopWatch through a reusable
X11 shim, and produce a porting assessment of every hack in the tree.

**Architecture:** A pure-C RGB565 `Canvas` holds the frame. An Xlib-style
shim draws into it and serves resource lookups from each hack's defaults
table. `pyro.c` is compiled unmodified against the shim's `screenhack.h`.
The same code builds natively on the Mac (for tests and PNG dumps) and for
the ESP32-S3 (where `main.cpp` pushes the canvas to the display).

**Tech Stack:** PlatformIO (Arduino framework, `native` env with Unity),
M5Unified, M5GFX, C/C++, Python 3.13+ with `uv` and pytest for `tools/`.

**Spec:** `docs/superpowers/specs/2026-10-07-stopwatch-xscreensaver-design.md`

## Global Constraints

- Board: M5Stack StopWatch, SKU C152, ESP32-S3, 466×466 round AMOLED.
- Arduino framework on PlatformIO with M5Unified (0.2.15+) and M5GFX
  (0.2.21+). Board Manager equivalent is 3.3.7+ (arduino-esp32 3.x).
- Partition scheme stays `app3M_fat9M_16MB`. Never touch the `ffat`
  partition (no `FFat.begin(true)`).
- Canvas is 466×466 RGB565 (about 434 KB) in PSRAM, pushed once per frame.
- Hacks use their default settings only; resources come from each hack's
  built-in defaults table.
- Button A selects the next hack, button B the previous one.
- Everything installed inside the project folder: `.venv/`,
  `PLATFORMIO_CORE_DIR=<repo>/.platformio`. Both git-ignored.
- `vendor/` is untracked and never edited. Hack files are copied into
  `firmware/src/hacks/<name>/` with their original header intact; patches go
  below it, marked as modifications.
- Our code is MIT licensed. Every copied hack is listed in
  `THIRD_PARTY_NOTICES.md`.
- Serial port: `/dev/cu.usbmodem112401`. Never touch
  `/dev/cu.usbmodem14201`.
- Download mode: hold the power button about 2 seconds until the green LED
  lights. An unexplained flash failure means stop and diagnose; no repeated
  full-chip erases.
- Report "compiles" and "runs on device" separately.
- Work on branch `feature/pyro-m5-stopwatch`. Check with
  `git branch --show-current` before each commit. Commit messages end with
  `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.
- Never `git add -A`; add named paths.
- Python scripts run with `uv run`; Python 3.13+, formatted with ruff.
- Shell output: `NO_COLOR=1`; commands expected to exit non-zero end in
  `|| true`.

## Review Focus

1. Drawing partly or wholly off-canvas (negative or huge coordinates and
   sizes) must clip, never crash or wrap. Pyro's particles leave the
   screen. Pinned in Task 5.
2. Colours: `XAllocColor` 16-bit channels must convert to the right RGB565
   (white stays white, pure red stays red) and the device must show them
   with correct byte order, not red and blue swapped. Pinned in Tasks 5, 6
   and 9.
3. A hack whose `draw` returns a delay of 0 or a very long delay must not
   starve button handling or spin the CPU. Pinned in Task 8.
4. Switching hacks repeatedly (init, free, init) must not leak or corrupt
   memory. Pinned in Task 8 (host, with AddressSanitizer) and Task 9
   (device heap readout).
5. Failures at the edges of the tooling: PSRAM allocation failing at boot;
   the download page having no tarball link; a tarball with `../` or
   absolute paths. Each must produce a clear message, not a crash or a
   partial extraction. Pinned in Tasks 2 and 9.

---

## File structure

```text
.gitignore  LICENSE  README.md  THIRD_PARTY_NOTICES.md
tools/
  env.sh                       exports PLATFORMIO_CORE_DIR, activates .venv
  fetch_xscreensaver.py        Task 2
  check_notices.py             Task 7
  rgb565_to_png.py             Task 8
  score_hacks.py               Task 10
  tests/                       pytest for the above
firmware/
  platformio.ini               envs: stopwatch, native, dump
  src/
    core/canvas.h canvas.c     RGB565 canvas + clipped primitives
    x11shim/xshim.h xshim.c    Xlib-style types and drawing calls
    x11shim/resources.c        defaults-table lookups
    x11shim/include/screenhack.h   replaces xscreensaver's header
    runner/hack_runner.h .c    start/step/stop one hack
    hacks/registry.c           list of hacks
    hacks/pyro/pyro.c          copied unmodified from vendor
    xs_support/hsv.c           copied from vendor utils
    main.cpp                   device entry (stopwatch env only)
  native/dump_main.c           host entry: run N frames, dump raw RGB565
  test/test_*/                 Unity tests (native env)
docs/porting-assessment.md     Task 10 output
```

---

### Task 1: Repo scaffolding and licence

**Files:**

- Create: `.gitignore`, `LICENSE`, `README.md`, `THIRD_PARTY_NOTICES.md`

**Interfaces:**

- Produces: ignore rules every later task relies on; an empty
  `THIRD_PARTY_NOTICES.md` with a section per included hack (filled in
  Tasks 7 and 8).

- [ ] **Step 1: Write `.gitignore`**: `vendor/`, `.venv/`, `.platformio/`,
  `.pio/`, `__pycache__/`, `*.bin` backups under `backups/`, `*.raw`,
  `.DS_Store`.
- [ ] **Step 2: Write `LICENSE`**: MIT, copyright "Rod Begbie", year 2026.
- [ ] **Step 3: Write `README.md`** with a one-paragraph purpose and a
  "Status: in progress" line. Invoke the `/markdown` skill first. Full
  setup instructions are added in Task 11.
- [ ] **Step 4: Write `THIRD_PARTY_NOTICES.md`**: intro stating that copied
  xscreensaver files keep their own notices and are not relicensed, plus
  the jwz permission notice text, verbatim from `vendor/.../hacks/pyro.c`
  lines 1-9.
- [ ] **Step 5: Verify** `git status --short` shows `vendor/` is no longer
  listed; `markdownlint` (if installed) passes on both `.md` files.
- [ ] **Step 6: Commit** the four files.

---

### Task 2: `fetch_xscreensaver.py`

**Files:**

- Create: `tools/fetch_xscreensaver.py`, `tools/tests/test_fetch.py`,
  `tools/tests/fixtures/download.html`

**Interfaces:**

- Produces (importable, pure functions, stdlib only):
  - `find_versions(html: str) -> list[str]`: versions from links shaped
    `xscreensaver-<X.YY>.tar.gz`, sorted ascending by numeric tuple.
  - `pick_version(html: str, wanted: str | None) -> str`: highest, or
    `wanted` if present; raises `FetchError` if none or not found.
  - `safe_extract(tar_path: Path, dest: Path) -> Path`: extracts with
    `filter="data"`, returns the single top-level directory; raises
    `FetchError` on any path escape.
  - `main(argv: list[str]) -> int` with `--version`, `--force`, `--dest`
    (default `vendor`; added so tests and a dry run never touch the real
    `vendor/`).
  - `class FetchError(Exception)`.

- [ ] **Step 1: Save fixture** `download.html` as a trimmed copy of the
  real page (include the 6.16 `.dmg`, `.apk` and `.tar.gz` links plus an
  older-version link).
- [ ] **Step 2: Write failing tests**:
  - `test_find_versions_ignores_dmg_and_apk`:
    `find_versions(fixture) == ["6.16"]` (or whatever the fixture holds).
  - `test_versions_sort_numerically`: `["6.9", "6.16"]` sorts to `6.16`
    last.
  - `test_pick_version_no_link_raises`: `pick_version("<html></html>",
    None)` raises `FetchError`.
  - `test_pick_version_unknown_wanted_raises`.
  - `test_safe_extract_rejects_traversal`: tarball containing
    `../evil.txt` raises `FetchError` and nothing is written outside
    `dest`.
  - `test_safe_extract_rejects_absolute_and_symlink_escape`.
  - `test_safe_extract_ok`: a tiny tarball with `xscreensaver-9.9/a.txt`
    returns `dest/xscreensaver-9.9` and the file exists.
  - `test_main_skips_existing_dir_without_force`: with `fetch` patched out,
    an existing `dest/xscreensaver-X` is left untouched and exit code is 0.
  - `test_main_warns_when_not_6_16`: stderr mentions re-checking the
    survey.
- [ ] **Step 3: Run** `uv run --with pytest pytest tools/tests/test_fetch.py
  -v`. Expected: all FAIL (module missing).
- [ ] **Step 4: Implement** the functions above. Parse links with
  `re`; download with `urllib.request` into a `tempfile.TemporaryDirectory`;
  print version, URL and SHA-256; move the extracted directory into `dest`.
  Network failure becomes a `FetchError` message, exit code 1.
- [ ] **Step 5: Run the tests** again. Expected: all PASS.
- [ ] **Step 6: Real run, no overwrite**:
  `uv run tools/fetch_xscreensaver.py` against the existing `vendor/`.
  Expected: prints version, SHA-256, and "already present, skipping". If the
  shell hook blocks network access, ask Rod to run it with `!`.
- [ ] **Step 7: Real run into a scratch dir**:
  `uv run tools/fetch_xscreensaver.py --dest <scratchpad>/vendor-check`.
  Expected: extraction succeeds and the tree's `hacks/pyro.c` exists.
- [ ] **Step 8: Commit** the script, tests and fixture.

---

### Task 3: Isolated toolchain

**Files:**

- Create: `tools/env.sh`, `firmware/platformio.ini`,
  `firmware/src/main.cpp` (placeholder `setup(){} loop(){}`)

**Interfaces:**

- Produces: `source tools/env.sh` gives a shell where `pio` runs with
  `PLATFORMIO_CORE_DIR` inside the repo. Envs `stopwatch` (device),
  `native` (host tests), `dump` (host frame dumper; defined in Task 8).

- [ ] **Step 1: Create the venv**: `uv venv .venv` then
  `uv pip install platformio` (with the venv active).
- [ ] **Step 2: Write `tools/env.sh`**: exports `PLATFORMIO_CORE_DIR` as
  `<repo>/.platformio` (derived from the script's location), puts
  `.venv/bin` on `PATH`.
- [ ] **Step 3: Read the vendor recipe** at
  <https://docs.m5stack.com/en/core/StopWatch> (PlatformIO section; fetch
  with the ctx fetch tool). Use its platform, board and library entries. The
  official `espressif32` platform may be pinned to arduino-esp32 2.x, which is
  older than the 3.3.7+ the board needs; if the recipe or version floors
  require the community `pioarduino` platform, use that. If no PlatformIO
  route meets the floors, stop and report to Rod; the fallback is
  `arduino-cli`, also installed inside the folder.
- [ ] **Step 4: Write `firmware/platformio.ini`** with:
  - `[env:stopwatch]`: the recipe's platform and board, `framework =
    arduino`, 16 MB flash, octal PSRAM enabled, `board_build.partitions =
    app3M_fat9M_16MB.csv`, `lib_deps` with M5Unified and M5GFX pinned to
    exact versions at or above the floors, `monitor_speed = 115200`,
    `upload_port` and `monitor_port` = `/dev/cu.usbmodem112401`.
  - `[env:native]`: `platform = native`, `test_framework = unity`,
    `build_flags = -fsanitize=address -g`, `build_src_filter` excluding
    `main.cpp`.
  - `build_src_filter` for `stopwatch` excluding `native/`.
- [ ] **Step 5: Verify the partition file exists** in the installed
  framework package (`find .platformio -name 'app3M_fat9M_16MB.csv'`).
  Expected: one match. If absent, stop and report.
- [ ] **Step 6: Verify builds**: from `firmware/`, `pio run -e stopwatch`
  succeeds (compile only); `pio test -e native` reports "no tests" cleanly.
- [ ] **Step 7: Verify isolation**: `ls ~/.platformio` does not exist or is
  unchanged by this task. Record sizes of `.venv` and `.platformio` in the
  commit message so Rod knows what a cleanup frees.
- [ ] **Step 8: Commit** `tools/env.sh`, `firmware/platformio.ini`,
  `firmware/src/main.cpp`.

---

### Task 4: Board check, backup, hello world

**Files:**

- Modify: `firmware/src/main.cpp`

**Interfaces:**

- Produces: a verified flash/monitor workflow and a flash backup at
  `backups/stopwatch-original-16MB.bin` (git-ignored).

- [ ] **Step 1: Read-only board check**: using the esptool bundled in
  `.platformio` (`pio pkg exec -p tool-esptoolpy -- esptool.py
  --port /dev/cu.usbmodem112401 flash_id`). Expected: ESP32-S3, 16MB flash.
  If the port is busy or absent, ask Rod to replug; if it will not respond,
  ask Rod to enter download mode.
- [ ] **Step 2: Ask Rod before the backup**, then read the full 16 MB:
  `esptool.py ... read_flash 0 0x1000000 backups/stopwatch-original-16MB.bin`.
  Verify the file is exactly 16,777,216 bytes. Note the md5 in the commit
  message.
- [ ] **Step 3: Write hello world** in `main.cpp`: `M5.begin(cfg)`, fill
  the display with red, then green, then blue (one second each), print the
  chip's free heap and free PSRAM over serial each second.
- [ ] **Step 4: Build**: `pio run -e stopwatch`. Expected: success.
- [ ] **Step 5: Ask Rod before the first flash**, then
  `pio run -e stopwatch -t upload`, then `pio device monitor`. Expected:
  serial shows PSRAM of roughly 8 MB.
- [ ] **Step 6: Rod confirms** the screen shows red, green, blue in that
  order (this also checks colour byte order and that the circle shows).
  Record the result.
- [ ] **Step 7: Verify recovery path** by confirming with Rod that
  <https://workos.com/init/badge/install> is reachable in desktop Chrome.
  Do not run it unless needed.
- [ ] **Step 8: Commit** `main.cpp`.

---

### Task 5: Canvas primitives

**Files:**

- Create: `firmware/src/core/canvas.h`, `firmware/src/core/canvas.c`,
  `firmware/test/test_canvas/test_canvas.c`

**Interfaces:**

- Produces (C, `extern "C"` guarded):

```c
typedef struct { int w, h; uint16_t *px; } Canvas;
int  canvas_init(Canvas *c, int w, int h, void *(*alloc)(size_t));
void canvas_free(Canvas *c);
uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b);
uint16_t rgb565_from16(uint16_t r, uint16_t g, uint16_t b);
void canvas_clear(Canvas *c, uint16_t color);
void canvas_point(Canvas *c, int x, int y, uint16_t color);
void canvas_line(Canvas *c, int x0, int y0, int x1, int y1, uint16_t color);
void canvas_fill_rect(Canvas *c, int x, int y, int w, int h, uint16_t color);
void canvas_fill_ellipse(Canvas *c, int x, int y, int w, int h, uint16_t color);
void canvas_fill_polygon(Canvas *c, const int *xy, int npoints, uint16_t color);
```

`canvas_init` returns 0 on success, -1 if `alloc` returns NULL. Pixel at
`(x, y)` is `px[y * w + x]`. Pixels are stored in native RGB565
(R in the top 5 bits).

- [ ] **Step 1: Write failing tests** (Unity), each on a small canvas:
  - `test_rgb565_white_black_red`: `rgb565(255,255,255) == 0xFFFF`,
    `rgb565(0,0,0) == 0`, `rgb565(255,0,0) == 0xF800`.
  - `test_rgb565_from16_matches_8bit`: `rgb565_from16(0xFFFF,0,0) ==
    0xF800`; `rgb565_from16(0xFFFF,0xFFFF,0xFFFF) == 0xFFFF`.
  - `test_init_failing_alloc_returns_minus_one`.
  - `test_point_inside_sets_pixel`; `test_point_outside_is_ignored` for
    `(-1,0)`, `(0,-1)`, `(w,0)`, `(0,h)`, `(INT_MAX,INT_MAX)`.
  - `test_fill_rect_clips_negative_origin`: 8×8 canvas,
    `fill_rect(-5,-5,10,10)` sets exactly the 5×5 corner.
  - `test_fill_rect_entirely_outside_changes_nothing`;
    `test_fill_rect_huge_size_no_overflow`: `fill_rect(0,0,INT_MAX,INT_MAX)`
    fills the whole canvas; zero or negative size changes nothing.
  - `test_line_horizontal_vertical_diagonal` including both endpoints set;
    `test_line_with_offscreen_endpoint_draws_visible_part`;
    `test_line_far_offscreen_changes_nothing_and_terminates`
    (endpoints around ±1,000,000).
  - `test_ellipse_diameter_1_is_one_pixel`;
    `test_ellipse_zero_size_draws_nothing`;
    `test_ellipse_5x5_is_symmetric` (mirror-compare rows and columns);
    `test_ellipse_partly_offscreen_clips`.
  - `test_polygon_triangle_fills_interior_not_exterior` with a known
    triangle; degenerate polygons (fewer than 3 points) change nothing.
- [ ] **Step 2: Run** `pio test -e native -f test_canvas`. Expected: FAIL to
  build (header missing).
- [ ] **Step 3: Implement** `canvas.c`. Clip every primitive to the canvas
  using 64-bit arithmetic so extreme inputs cannot overflow. Lines use
  Bresenham after Cohen-Sutherland (or equivalent) clipping. Ellipse uses a
  scanline fill from the ellipse equation. Polygon uses even-odd scanline
  fill.
- [ ] **Step 4: Run** the tests. Expected: all PASS, with no ASan reports.
- [ ] **Step 5: Commit** the three files.

---

### Task 6: X11 shim API and resources

**Files:**

- Create: `firmware/src/x11shim/xshim.h`, `xshim.c`, `resources.c`,
  `firmware/src/x11shim/include/screenhack.h`,
  `firmware/test/test_xshim/test_xshim.c`

**Interfaces:**

- Consumes: `Canvas` and drawing functions from Task 5.
- Produces: Xlib-compatible declarations in `xshim.h` (C, `extern "C"`
  guarded). Names and argument orders exactly as Xlib:
  `XCreateGC`, `XFreeGC`, `XSetForeground`, `XGetWindowAttributes`,
  `XClearWindow`, `XDrawPoint`, `XDrawLine`, `XDrawLines`,
  `XFillRectangle`, `XFillArc`, `XFillPolygon`, `XAllocColor`,
  `XFreeColors`, `WhitePixel`, `BlackPixel`, `DefaultScreen`; types
  `Display`, `Window`, `Colormap`, `GC`, `XColor`, `XGCValues`,
  `XWindowAttributes`, `XEvent`, `XrmOptionDescRec`, `Bool`, and constants
  `True`, `False`, `GCForeground`, `DoRed`, `DoGreen`, `DoBlue`,
  `CoordModeOrigin`, `Complex`, `XrmoptionSepArg`.
  Plus:

```c
Display *xshim_open_display(Canvas *canvas);
void xshim_close_display(Display *dpy);
void xshim_set_defaults(const char *const *defaults);  /* NULL-terminated */
int  get_integer_resource(Display *, const char *name, const char *cls);
double get_float_resource(Display *, const char *name, const char *cls);
Bool get_boolean_resource(Display *, const char *name, const char *cls);
char *get_string_resource(Display *, const char *name, const char *cls);
unsigned long get_pixel_resource(Display *, Colormap, const char *name,
                                 const char *cls);
```

  `screenhack.h` also declares `hsv_to_rgb(int h, double s, double v,
  unsigned short *r, unsigned short *g, unsigned short *b)`; the
  implementation is the copied vendor `hsv.c`, added in Task 8.

  Pixel values are the RGB565 value of the colour. `WhitePixel` is
  `0xFFFF`, `BlackPixel` is `0`. `XAllocColor` fills `color->pixel` using
  `rgb565_from16` and returns nonzero. `XFreeColors` is a no-op.
  `XFillArc` draws only full ellipses (`angle2 >= 360*64`); partial arcs
  draw nothing. Defaults lines look like `"*count:\t600"` or
  `".background:\tblack"`; the key is the text between the leading `.`/`*`
  and the colon, whitespace trimmed. A missing integer resource returns 0
  and logs the name once. `get_pixel_resource` understands `black`, `white`
  and `#rrggbb`.
  `include/screenhack.h` includes `xshim.h`, defines `frand(f)` using the
  platform's random source, `random()` availability, and
  `XSCREENSAVER_MODULE(CLASS, PREFIX)` which defines
  `const HackEntry PREFIX##_hack = {CLASS, PREFIX##_defaults,
  PREFIX##_init, PREFIX##_draw, PREFIX##_free};`, with `HackEntry` declared
  in `runner/hack_runner.h` (Task 8 creates it; `screenhack.h` forward
  declares its own copy of the struct so Task 6 tests stand alone).

- [ ] **Step 1: Write failing tests** (Unity):
  - `test_white_and_black_pixel`.
  - `test_alloc_color_red_gives_f800` and white gives `0xFFFF`.
  - `test_foreground_via_gc_used_by_fill_rectangle`: set foreground, fill,
    read canvas pixel.
  - `test_get_window_attributes_reports_canvas_size`.
  - `test_clear_window_uses_black_background`.
  - `test_draw_lines_connects_points` (`CoordModeOrigin`).
  - `test_fill_arc_full_circle_draws_partial_arc_does_not`.
  - `test_resources_parse_star_and_dot_prefixes_and_tabs`.
  - `test_resources_missing_integer_returns_zero`.
  - `test_pixel_resource_black_white_hex`.
- [ ] **Step 2: Run** `pio test -e native -f test_xshim`. Expected: FAIL.
- [ ] **Step 3: Implement** the shim. Each Xlib function is a thin wrapper
  over a Task 5 function; keep a single canvas pointer in `Display`.
- [ ] **Step 4: Run** the tests. Expected: all PASS.
- [ ] **Step 5: Commit** the shim files and tests.

---

### Task 7: Notices tooling

**Files:**

- Create: `tools/check_notices.py`, `tools/tests/test_check_notices.py`
- Modify: `THIRD_PARTY_NOTICES.md`

**Interfaces:**

- Produces: `check(hacks_dir: Path, notices_md: Path) -> list[str]`
  returning problems (empty list means OK); CLI `main(argv) -> int`
  printing problems, exit 1 if any.
  A file passes if its first 40 lines contain the phrase "Permission to use,
  copy, modify, distribute, and sell this software" or "Permission is
  hereby granted, free of charge" together with a "Copyright" line, and its
  directory's name (or filename stem) is mentioned in the notices file.

- [ ] **Step 1: Write failing tests** with temp directories:
  - `test_file_with_jwz_notice_and_listing_passes`.
  - `test_file_without_notice_is_reported`.
  - `test_file_not_listed_in_notices_is_reported`.
  - `test_notice_text_past_line_40_is_not_accepted`.
  - `test_non_c_files_ignored` (e.g. `.md`).
- [ ] **Step 2: Run** `uv run --with pytest pytest
  tools/tests/test_check_notices.py -v`. Expected: FAIL.
- [ ] **Step 3: Implement** `check` and `main`. Scan `*.c`, `*.h`, `*.cpp`
  in `firmware/src/hacks` and `firmware/src/xs_support`.
- [ ] **Step 4: Run** the tests. Expected: PASS.
- [ ] **Step 5: Add a notices section template** to
  `THIRD_PARTY_NOTICES.md`: for each included file, name, author, copyright
  line, licence, source (xscreensaver 6.16, <https://www.jwz.org/xscreensaver/>).
  Entries for Pyro and `hsv.c` are added in Task 8 with the files.
- [ ] **Step 6: Commit.**

---

### Task 8: Pyro on the host

**Files:**

- Create: `firmware/src/runner/hack_runner.h`, `hack_runner.c`,
  `firmware/src/hacks/registry.c`, `firmware/src/hacks/pyro/pyro.c`,
  `firmware/src/xs_support/hsv.c`, `firmware/native/dump_main.c`,
  `tools/rgb565_to_png.py`, `tools/tests/test_png.py`,
  `firmware/test/test_pyro/test_pyro.c`
- Modify: `firmware/platformio.ini` (add `[env:dump]`),
  `THIRD_PARTY_NOTICES.md`

**Interfaces:**

- Consumes: the shim (Task 6), `check_notices.py` (Task 7).
- Produces:

```c
typedef struct HackEntry {
  const char *name;
  const char *const *defaults;
  void *(*init)(Display *, Window, void *closure);
  unsigned long (*draw)(Display *, Window, void *closure); /* delay in us */
  void (*free)(Display *, Window, void *closure);
} HackEntry;
extern const HackEntry *const g_hacks[];
extern const int g_hack_count;

typedef struct HackRunner HackRunner;
HackRunner *runner_create(Canvas *canvas);
void runner_destroy(HackRunner *r);
int  runner_start(HackRunner *r, int index);   /* stops any current hack */
unsigned long runner_step(HackRunner *r);      /* one draw; returns us delay */
int  runner_next(HackRunner *r);
int  runner_prev(HackRunner *r);
int  runner_index(const HackRunner *r);
```

  `runner_step` clamps the returned delay to `[1000, 1000000]` microseconds.
  `runner_start` clears the canvas to black, calls
  `xshim_set_defaults(entry->defaults)`, then `entry->init`.
  `rgb565_to_png.py` has `convert(raw: bytes, w: int, h: int) -> bytes`
  (PNG bytes, stdlib `zlib` only) and a CLI
  `rgb565_to_png.py in.raw w h out.png`. `dump` env builds `dump_main.c`:
  `dump <hack-index> <frames> <out.raw>`.

- [ ] **Step 1: Copy sources and notices.** Copy `hacks/pyro.c` and
  `utils/hsv.c` from `vendor/`; leave the headers byte-for-byte intact, and
  make only the include fixes needed to compile, marked
  `/* Modified for stopwatch-screensaver: ... */` below the header. Add both
  to `THIRD_PARTY_NOTICES.md`. Run `uv run tools/check_notices.py`. Expected:
  exit 0.
- [ ] **Step 2: Write failing PNG tests**: `test_convert_has_png_signature`;
  `test_convert_2x1_red_green_roundtrips` (decode with a tiny stdlib
  decoder in the test and compare to `0xF800`, `0x07E0` expanded to 8-bit);
  `test_convert_wrong_length_raises_value_error`.
- [ ] **Step 3: Implement** `convert`. Run the tests. Expected: PASS.
- [ ] **Step 4: Write failing Unity tests** in `test_pyro.c` (466×466
  canvas):
  - `test_pyro_compiles_and_is_registered`: `g_hack_count == 1` and
    `g_hacks[0]->name` is `"Pyro"`.
  - `test_pyro_draws_something_within_300_frames`: after 300 `runner_step`
    calls the canvas has at least 50 non-black pixels.
  - `test_pyro_does_not_draw_outside_canvas`: ASan clean (the test passing
    under the native env's sanitizer is the assertion).
  - `test_step_delay_is_clamped`: with a stub hack returning 0 and one
    returning `ULONG_MAX`, `runner_step` returns 1000 and 1000000.
  - `test_switching_100_times_leaves_no_asan_errors`: `runner_start(0)`
    then `runner_next()` 100 times, stepping 5 frames each; on the single
    hack list `runner_next` wraps to 0.
  - `test_prev_wraps_to_last`.
- [ ] **Step 5: Run** `pio test -e native -f test_pyro`. Expected: FAIL.
- [ ] **Step 6: Implement** the runner and registry; adjust the shim until
  `pyro.c` compiles unmodified (add any missing types, never edit the
  body of `pyro.c`). Add `test_hsv_to_rgb_pure_red_h0` to `test_pyro.c`
  (h=0, s=1, v=1 gives r=65535, g=0, b=0) to check the copied `hsv.c`.
- [ ] **Step 7: Run** the tests. Expected: all PASS.
- [ ] **Step 8: Dump and look.** Build `pio run -e dump`, run
  `.pio/build/dump/program 0 400 <scratchpad>/pyro.raw`, convert with `rgb565_to_png.py`, and open the PNG
  with the Read tool. Expected: visible firework bursts on black. Show it to
  Rod.
- [ ] **Step 9: Commit** by named paths.

---

### Task 9: Pyro on the device

**Files:**

- Modify: `firmware/src/main.cpp`

**Interfaces:**

- Consumes: `HackRunner` API (Task 8), `Canvas` (Task 5).

- [ ] **Step 1: Write `main.cpp`**:
  - `setup()`: `M5.begin(cfg)`; `Serial.begin`; `canvas_init(&canvas, 466,
    466, ps_malloc)`. On failure draw "PSRAM alloc failed" on the display,
    print it to serial, then halt (loop with `delay(1000)`), never
    dereference a null buffer. Create the runner and `runner_start(0)`.
  - `loop()`: `M5.update()`; `BtnA.wasPressed()` calls `runner_next`,
    `BtnB.wasPressed()` calls `runner_prev`; `runner_step`; push the canvas
    with `M5.Display.setSwapBytes(true)` then
    `pushImage(0, 0, 466, 466, canvas.px)`; wait out the returned delay in
    slices of at most 10 ms, calling `M5.update()` and checking buttons
    each slice so input stays responsive.
  - Every 5 seconds print frames per second, free heap and free PSRAM.
- [ ] **Step 2: Build**: `pio run -e stopwatch`. Expected: success.
- [ ] **Step 3: Ask Rod before flashing**, then upload and monitor.
- [ ] **Step 4: Rod confirms** fireworks display, with correct colours (a
  red burst looks red, not blue) and no flicker. If red and blue are
  swapped, toggle the `setSwapBytes` argument and re-test; record which
  setting was right.
- [ ] **Step 5: Record serial readout**: fps, and heap and PSRAM before and
  after Rod presses button A 20 times (only one hack, so this restarts
  Pyro each time). Expected: free memory after is within 1 KB of before.
- [ ] **Step 6: Commit** `main.cpp` with the measured fps in the message.

---

### Task 10: Porting assessment

**Files:**

- Create: `tools/score_hacks.py`, `tools/tests/test_score.py`,
  `docs/porting-assessment.md`

**Interfaces:**

- Consumes: `firmware/src/x11shim/xshim.h` as the single source of truth
  for which Xlib calls are provided.
- Produces:
  - `implemented_calls(header: str) -> set[str]`: identifiers starting
    with `X` followed by an uppercase letter and then `(`, found in
    prototypes in `xshim.h`.
  - `used_calls(source: str) -> set[str]`: the same pattern in a hack's
    `.c`, with comments and string literals stripped.
  - `classify(path: Path, source: str) -> str`: `"gl"` if the file lives in
    `hacks/glx/` or uses `glBegin`/`glVertex`/`glx` identifiers, else `"2d"`.
  - `score(path: Path, source: str, provided: set[str]) -> dict` with keys
    `name`, `kind`, `missing` (sorted list), `loc`, `flags` (list from:
    `pixmaps`, `readback`, `xor`, `text`, `clipmask`, `float-heavy`,
    `needs-xlockmore`), and `effort` in `{"S", "M", "L", "XL"}`.
  - Effort rules, written into the assessment doc as well: `S` = 2D and
    nothing missing; `M` = 2D with 1-4 missing calls and no readback or
    pixmap flag; `L` = 2D with 5+ missing calls or `readback`/`pixmaps`;
    `XL` = GL.
  - Flag rules: `pixmaps` if `XCreatePixmap` or `XCopyArea` used; `readback`
    if `XGetImage`/`XGetPixel`; `xor` if `XSetFunction`; `text` if any
    `XDrawString`/`XLoadFont`; `clipmask` if `XSetClipMask`;
    `float-heavy` if `sin(`+`cos(`+`sqrt(`+`pow(` calls total 20 or more;
    `needs-xlockmore` if it includes `xlockmore.h`.
  - CLI writes a Markdown table to a given path, with a header recording the
    xscreensaver version scanned.

- [ ] **Step 1: Write failing tests** with inline source strings:
  - `test_used_calls_ignores_comments_and_strings`.
  - `test_implemented_calls_reads_header` against a small header fixture.
  - `test_classify_gl_by_path_and_by_identifier`.
  - `test_score_pyro_like_source_is_S` (only provided calls).
  - `test_score_flags_readback_pixmaps_xor`.
  - `test_score_effort_L_for_pixmaps_and_XL_for_gl`.
  - `test_table_output_is_lint_clean_markdown` (blank lines around table,
    ends with newline).
- [ ] **Step 2: Run** the tests. Expected: FAIL.
- [ ] **Step 3: Implement** the script. Scan `hacks/*.c` and
  `hacks/glx/*.c` in the vendor tree; take the version from the directory
  name.
- [ ] **Step 4: Run** the tests. Expected: PASS.
- [ ] **Step 5: Generate** `docs/porting-assessment.md` from the real tree.
  Add hand-written sections above the table: how to read it, the effort
  rules, caveats (flags are heuristics, not measurements), and a
  "measured" box with Pyro's fps and heap numbers from Task 9. Also list
  the ten most-missed calls across all 2D hacks, sorted by how many hacks
  each would unlock, as the suggested order for shim stage 2. Invoke
  `/markdown` first.
- [ ] **Step 6: Sanity check** the output: Pyro is `S`; the count of rows
  equals the number of `.c` files scanned; at least one `L` and one `XL`
  exist. Report any surprises to Rod.
- [ ] **Step 7: Commit.**

---

### Task 11: README and final verification

**Files:**

- Modify: `README.md`

- [ ] **Step 1: Write README sections**: what this is; prerequisites
  (Xcode CLT, `uv`); setup (`uv run tools/fetch_xscreensaver.py`,
  `source tools/env.sh`, `pio test -e native`, `pio run -e stopwatch -t
  upload`); controls; how to restore the conference firmware
  (<https://workos.com/init/badge/install>) and from
  `backups/stopwatch-original-16MB.bin` with the `esptool` write command;
  how to clean up (`rm -rf .venv .platformio .pio vendor`); licence
  summary. Invoke `/markdown` first.
- [ ] **Step 2: Invoke `superpowers:verification-before-completion`**, then
  run in order and report real output:
  - `uv run --with pytest pytest tools/tests -v`
  - `uv run tools/check_notices.py`
  - `pio test -e native` (from `firmware/`)
  - `pio run -e stopwatch`
  - `git status --short` shows a clean tree (`vendor/` is git-ignored).
- [ ] **Step 3: Ask Rod** to confirm Pyro is still running on the device
  after a power cycle.
- [ ] **Step 4: Commit** the README, then invoke
  `superpowers:finishing-a-development-branch`.
