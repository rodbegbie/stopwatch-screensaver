<!-- entire-graph:begin -->
This repo has the entire-graph code graph installed. Before exploring code with
grep/find/whole-file reads, read .entire/graph-agent.md — resolution-first guidance
for using graph retrieval, focused source inspection, and verification.
@.entire/graph-agent.md
<!-- entire-graph:end -->

# stopwatch-screensaver

xscreensaver "hacks" running on an M5Stack StopWatch (SKU C152, ESP32-S3,
466×466 round AMOLED) through a small X11 shim. A learning project: explain
embedded-specific choices as you make them. See `README.md` for setup and
`docs/porting-assessment.md` for what to port next.

## Layout

- `firmware/src/core/`: RGB565 `Canvas` and clipped drawing primitives, which
  also record the dirty span of each row, and `push_plan` (C), which decides
  what to send the display.
- `firmware/src/x11shim/`: Xlib look-alike drawing into the canvas, resource
  lookups from each hack's defaults table, and `include/` stand-ins for
  xscreensaver's `screenhack.h`, `utils.h`, `hsv.h`, `erase.h`, `colors.h`.
- `firmware/src/hacks/<name>/`: hack sources copied from xscreensaver.
  `hacks/registry.[ch]` (ours) lists them in button order.
- `firmware/src/xs_support/`: other copied xscreensaver files (`hsv.c`).
- `firmware/src/runner/` and `main.cpp`: start, step and switch hacks; the
  device loop. `firmware/native/dump_main.c` renders frames on the Mac.
- `tools/`: fetch, notices check, assessment scorer, PNG converter (Python).
  `tools/failed_ports.txt` lists abandoned ports (`name: reason`); the scorer
  marks them ❌, and ✅ comes from `g_hacks[]` in `registry.c`.
- `.claude/skills/port-hack/`: the order of work for porting a hack, and a
  recipe per trap (`techniques.md`). Use it when porting or fixing a ported
  hack. `m5stack-stopwatch/` is the board guide.
- `vendor/`, `backups/`, `.venv/`, `.platformio/` are git-ignored and local.

## Commands

Run `source tools/env.sh` first (keeps PlatformIO inside the repo), then from
`firmware/`:

- `pio test -e native`: host tests (Unity, AddressSanitizer). Run these first.
- `pio run -e stopwatch`: build for the device. Add `-t upload` to flash.
  The device starts on a random hack; to pin one while examining it, prefix
  `PLATFORMIO_BUILD_FLAGS='-DSTART_HACK=\"galaxy\"'` (a name from `g_hacks[]`,
  any case; an unknown name falls back to random and logs it). Rebuild without
  it afterwards, since the define sticks to the build.
  It rotates to the next hack every 90 s (a button press restarts the count);
  `-DROTATE_SECONDS=5` shortens that for leak testing and `=0` turns it off. A
  pinned build still rotates, so add `=0` to measure one hack.
  The log prints `rotate -> <hack> heap= psram=` at each change. Combine flags
  in one `PLATFORMIO_BUILD_FLAGS` string.
- `pio run -e dump`, then `.pio/build/dump/program <index> <frames> out.raw`,
  then `uv run tools/rgb565_to_png.py out.raw 466 466 out.png` to see a frame.
  The index is the hack's 0-based position in `g_hacks[]` in
  `hacks/registry.c` (Hopalong is 13, Galaxy 21, Drift 22, Maze 24, Blaster
  25, Substrate 26): look it up, and check the dumped hack by name before
  trusting a comparison. With 0 frames it starts and stops the hack, which
  shows at once whether one survives being freed before its first draw (an
  exit code of 139 is a crash).
- To compare frames side by side, write a small `uv run` script with an inline
  `pillow` dependency; the system Python has no PIL.
- Serial log: read `/dev/cu.usbmodem112401` at 115200 for N seconds into a file
  (a pyserial script run with `.venv/bin/python -I`). Run it in the background
  with absolute paths, since background shells ignore `cd`. Lines to read:
  `run <hack> fps= step= push= wait= rows=` (ms per frame in the hack, the
  push and the wait; mean canvas rows sent per frame, 466 being a full push,
  though overlay redraws made while waiting can push it higher)
  and `switch -> <hack> press_waited=`. An empty capture, or "Device not
  configured", means the USB port went away and came back (a flash, a crash):
  reopen it in a loop.
- Flash with `pio run -e stopwatch -t upload > file 2>&1`, never piped through
  `head`, and check for "Hash of data verified". The button steps backwards
  (Pyro, then Lightning, then Drift...). Work out how long a restart takes
  from the hack's cycle count times its frame time (Substrate: 10,000 cycles
  at about 160 ms is 27 minutes) and capture that long, with rotation off.
- `cc -O1 -fstack-usage -c src/hacks/<n>/<n>.c` (with `-Isrc
  -Isrc/x11shim/include -Isrc/xs_support -DXSHIM_NATIVE -DSTANDALONE`) lists
  stack frames; keep each well under the 16 KB loop stack.

From the repo root:

- `uv run --with pytest --with pillow pytest tools/tests` (without Pillow the
  logo converter's tests are skipped, not run)
- `uv run tools/check_notices.py`
- `uv run tools/score_hacks.py` regenerates `docs/porting-assessment.md`.
- `markdownlint <files>` (config in `.markdownlint.json`). Run it from here:
  from `firmware/` it misses the config and reports line-length errors.

Set `NO_COLOR=1` on `pio` output you parse.

## Rules

- **Copied hacks stay byte-identical** to `vendor/xscreensaver-*/hacks/`. Make
  them compile by extending the shim, never by editing the hack. Verify with
  `cmp`.
- **Licensing:** every copied file keeps its original header and gets a row
  in `THIRD_PARTY_NOTICES.md`; `tools/check_notices.py` must pass. Read an
  unfamiliar file's licence before copying it (seven xscreensaver hacks do
  not use the standard jwz notice). Our own code is MIT.
- **Shim code is ours.** Do not copy xscreensaver's `utils/` files; write
  small equivalents in `x11shim/`.
- **Test first**, on the host. Clip arithmetic must use `int64_t`: the ESP32's
  `long` is 32 bits, so host tests cannot see 32-bit overflow.
- Mutation-check new tests: break the rule under test and watch the test fail.
  Race fixes need a failing two-thread stress test first (see `test_buttons`).
  A test can pass for the wrong reason: through Pyro, "starts black" passed
  with the runner broken (Pyro clears its own window), and "20 non-black
  pixels" passed on Substrate's white canvas before it drew. Test shared
  behaviour with stub hacks, and compare with the canvas the hack started on.
  Counting distinct colours could not see a consistent byte swap (a swap maps
  colours one to one): pin a hash of the frame instead, taken from a build you
  trust, and call `srandom(1)` first, because `random()` carries over between
  tests.
- Changing what every hack sees (the runner, a default, a shared helper) is not
  filling a shim gap: ask Rod first.
- Verify a delegated port yourself: `cmp` the hack, run the full suite, build
  for the device. Agents' reports have been wrong (a test count; "orange looks
  orange in a host frame" taken as proof of byte order).
- **Never flash, erase or write the device without asking Rod.** The
  conference firmware was backed up to `backups/` (git-ignored, one copy).
  Keep the `app3M_fat9M_16MB` partition scheme and never touch `ffat`. Serial
  port is `/dev/cu.usbmodem112401`; leave `/dev/cu.usbmodem14201` alone.
- Report "compiles", "host tests pass" and "runs on the device" separately.
  Only Rod can confirm what the screen shows.
- Python: 3.13+, `uv`, ruff, pytest. Markdown: invoke the `/markdown` skill.
- Commits end with the `Co-Authored-By` trailer from the session reminder.
  Use named paths with `git add`, never `-A`.

## Adding a hack

The `port-hack` skill orders this work, says what to read for before copying a
hack, and holds a recipe per trap. The steps:

1. Check its row in `docs/porting-assessment.md` and read its licence header.
2. Update the expected list in `firmware/test/test_hacks/test_hacks.c` and see
   it fail.
3. Copy the file to `firmware/src/hacks/<name>/<name>.c`, add the notices row
   and an `extern` plus entry in `hacks/registry.c`.
4. Fill shim gaps test-first in `firmware/test/test_xshim/`. The compile
   check in `score_hacks.py` shows what is missing.
5. Host tests pass, dump a frame and look at it, then (with Rod's go-ahead)
   flash and measure fps and free heap/PSRAM over serial.
6. Regenerate the assessment and add measurements to
   `tools/assessment_measured.md`.
7. If a hack is slow and `double`-heavy (many `double`s, `sqrt`, `sin`/`cos`,
   `pow`), try a single-precision wrapper like `hacks/galaxy_single.c` (for a
   plain screenhack, `hacks/substrate_single.c`; the recipe is in the skill's
   `techniques.md`), and compare double and float frames at several frame
   counts. A resource
   override can skip a hack's restart cleanup: Galaxy leaked at `count: 2`, so
   leak-test any override.

## Delivering a branch

- `entire trail create --title ... --type feature --body ...` pushes the branch
  and opens a linked DRAFT PR. The PR body is not synced from the trail: update
  both (`entire trail update --body`, `gh pr edit N --body-file`). Put
  `Fixes #N` in the body, then read the PR back (`gh pr view N --json body`):
  #27 merged with the generic body, so `Fixes #23` never reached GitHub and
  the issue stayed open until it was closed by hand.
- Rod approves and merges (merge commit). Afterwards delete the merged branch
  (remote and local) without asking and fast-forward `main`. Confirm the merge
  after a fresh `git fetch`: a stale `origin/main` says "not merged". Remove
  the branch's worktree first (a checked-out branch cannot be deleted), and
  note that `git branch -d` refuses while local `main` is behind, so check
  `git merge-base --is-ancestor <tip> origin/main` and then use `-D`. `main` is
  usually checked out in Rod's main checkout, so `git fetch origin main:main`
  is refused there: run `git merge --ff-only origin/main` in it, only if
  `git status` shows no tracked changes.
- Merge with `gh pr merge N --merge` once Rod approves. His approval is
  enough: do not wait for the Entire Gates check to finish.
- `gh pr edit N --body-file` replaces the whole body, including the
  `<!-- entire-trail-link-start -->` ... `-end -->` block at the top. Keep that
  block in the file, or the PR loses its trail.
- Findings: `entire trail finding list N`, then `... resolve N <id> -m "..."`.
  Bot reviews can lag about 20 minutes. `N` is the trail number (PR #12 was
  trail 6), not the PR number. For a false positive or upstream behaviour in a
  byte-identical hack, use `entire trail finding dismiss N <id> -m "<reason>"`.
- `entire trail update --body` takes no number and acts on the current
  branch (`entire trail update 8` errors).
- Stacked PR: `entire trail create --base <parent-branch>`. When the parent
  merges, `gh pr edit N --base main` before deleting its branch, then
  `git rebase --onto origin/main <old parent tip>`. The rebased push needs
  `--force-with-lease` (ask Rod first), unless the branch was never pushed.
- `docs/porting-assessment.md` conflicts on rebase: `git checkout --theirs`
  it to continue, then rerun `tools/score_hacks.py` and amend the result in.
- Two ports in flight both append to `g_hacks[]`, so whichever merges second
  conflicts in `registry.c`, the expected list in `test_hacks.c`,
  `THIRD_PARTY_NOTICES.md`, `tools/assessment_measured.md` (rows, paragraphs and
  the hack count) and the three `build_src_filter` lines in `platformio.ini`,
  as well as the generated assessment. Keep both sides, put the newer hack
  last, and regenerate the assessment. A rebased published branch needs
  `--force-with-lease` (ask Rod), and GitHub can reject that push once too.
- A finding that says the code "does not exist" can be stale after a rebase
  (the skill PR was reviewed before the Blaster and Substrate code it referred
  to had landed). Check it against `main` before dismissing, and put the
  evidence in the dismissal. Entire's approvals gate once showed "no reviewers
  have approved" right after a force push, though Rod had approved; his word is
  enough, so say so and go ahead.

## Gotchas

- M5GFX reads a plain `uint32_t` colour as RGB888. `TFT_RED` and friends are
  RGB565 constants. The canvas holds pixels already byte-swapped into the
  display's order (`rgb565()` returns them that way), and the display runs
  with `setSwapBytes(false)`. A swap done by M5GFX on each push cost 10 ms of
  every frame (#23: pushes of 41-45 ms became 31 ms). `px_swap()` turns a
  canvas pixel into ordinary RGB565 and back: only code that reads colour bits
  needs it (the logo loader, `dump_main.c`, tests). A hue-sweeping palette
  stays a rainbow with its bytes swapped (red, green and blue rotate), so only
  pure black and white, or a colour you know (Maze's red flame, its green
  solving path), expose a wrong order on the device. Host frames cannot,
  since the swap happens in the push; compare them with `px_swap` applied.
- A hack that takes colour bits out of pixel values itself (Substrate's alpha
  blend, in `point2rgb`) breaks on swapped pixels without a compile error and
  without a crash. `substrate_single.c` swaps at `XAllocColor` and
  `XSetForeground` so the hack sees ordinary RGB565; its golden-frame test
  fails if either is missing. Searching for `XGetPixel` finds none of this:
  dump every hack before and after a pixel-format change and `cmp` the frames.
  Pyro differs by a few hundred pixels, because it sorts projectiles by pixel
  value and so draws overlaps in another order.
- The display only gets the rows a hack drew. Every canvas write must go
  through `canvas_clear`, `canvas_point`, `hspan` or `canvas_paste_rect`
  (which record a span per row), or call `canvas_mark_dirty`, or the screen
  keeps the old pixels. The overlay text in `main.cpp` is the one deliberate
  bypass: it is written straight into `px`, marked by hand, and restored with
  `canvas_paste_rect`, which marks it again so the next push erases it.
  `test_push_present` replays every hack into a shadow display to catch a
  missed mark. M5GFX keeps a framebuffer for this panel and `endWrite` flushes
  one bounding box around everything written in a `startWrite` batch, so rows
  far apart must not share a batch (two corner pixels cost 11.7 ms together,
  0.1 ms apart). `push_plan` groups rows with a cost model fitted to device
  probes; its calibration shapes must include scattered rows, since compact
  rectangles cannot tell the models apart. Hacks that redraw unchanged pixels
  (CloudLife, Pedal) dirty nearly every row and gain little.
- In `platformio.ini` use `platform = platformio/native`; plain `native`
  breaks `pio run`. Native tests need `test_build_src = yes`.
- Quoted includes resolve beside the including file first, so compile hack
  copies from a temp dir when checking them against the shim.
- Hacks run on the Arduino `loopTask`, now set to a 16 KB stack in `main.cpp`.
  A hack with big local arrays can still overflow it (Rorschach's 9.6 KB did
  at 8 KB): the device reboots on that hack's first frame, and host tests
  cannot see it. Check the serial log for "Stack canary".
- `M5.update()` samples the buttons only when it runs and keeps no edge, so a
  press during a long hack step vanishes. `main.cpp` runs a 5 ms polling task
  on core 0 into `button_latch`, which needs the pin to hold a level for 30 ms.
  Reading the pin level inside a GPIO interrupt did not work: the release
  bounced and was counted as a second press. Check `press_waited` in the log.
- The runner caps a hack's delay at 10 s, and the loop credits only the push
  (31 ms; it was 41-45 ms between the byte-order fix and #23) against it,
  because a hack's delay is its pause after drawing. Compare device numbers
  with a baseline from the same build, not with old rows: Pyro fell from 30 to
  23 fps with no change to Pyro.
- `runner_start` paints the canvas in the hack's `background` resource before
  `init`, as `screenhack.c` paints the window, or black if it has none.
  Substrate is white; every other hack asks for black or nothing.
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
- Wide lines (`line_width` above 1), arcs and pixmap writes live in
  `x11shim/stroke.c`, `arc.c` and `pixmap.c`. Width 0 and 1 keep the old
  `canvas_line` path, so hacks that set no wide width are pixel-identical (every
  hack's frame is pinned in `test_hacks.c`). No registered hack draws a wide
  line yet: Maze's width 2 is under `HAVE_JWXYZ`.
- Host tests cannot prove the 16 KB loop stack: `ulimit -s` sees only the
  shallow first level of a random recursion, and AddressSanitizer inflates
  frames. Use `stack_used_by` in `test_hacks.c` (a painted pthread stack) and,
  on the device, `uxTaskGetStackHighWaterMark` in the stats line (temporary).
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
- Don't declare `xrealloc` or `xmalloc` in the shim: cloudlife defines its own
  static `xrealloc`, which would clash.
- `score_hacks.py` rates hacks by call sites, not loop trips: Flame (all
  `double` maths) is rated S but runs at 2-7 fps. Measure on the device.
- `pio test` runs every registered hack for 3000 frames under ASan (about 15 s);
  a hack that is slow on the host slows the whole suite.
- Host leak tests: LeakSanitizer does not run on macOS, so compare
  `__sanitizer_get_current_allocated_bytes()`
  (`<sanitizer/allocator_interface.h>`) before and after many restarts, as in
  `test_hacks.c`.
- `pio test` hides `printf` and stderr from tests. To see two values, assert
  `TEST_ASSERT_EQUAL_UINT64(a, b)` temporarily: the failure line prints both.
- In a git worktree the ignored `vendor/`, `.venv/` and `.platformio/` are
  missing. Symlink them from the main checkout. `.gitignore`'s trailing slashes
  don't match symlinks, so they would show as untracked, but the shared
  `.git/info/exclude` already lists the three names. That file is shared by
  every worktree: ask Rod before adding to it.
  The harness refuses `source tools/env.sh` and a computed `PATH` there: run
  `PLATFORMIO_CORE_DIR=<worktree>/.platformio ../.venv/bin/pio ...` instead,
  and keep commands plain (no scripts, loops, `$VAR` in a command or `&&`
  chains with a computed path). A worktree an agent works in (`isolation:
  "worktree"`) starts from `origin/main`, not from your branch, and has no
  vendor or toolchain until you link them.
- zsh does not word-split `$var`: loop over file lists with `bash -c`. It also
  stops on a glob that matches nothing, so quote `--include='*.c'`, and
  `grep` needs `-e` for a pattern that starts with a dash (`-e '->px'`).
- jwz.org returns 403 to Python's default User-Agent.
- `esptool` reads of the 16 MB flash need `--baud 921600` (about 3.5 minutes)
  and show no progress when piped.
- A push to GitHub is sometimes rejected once and succeeds on an immediate
  retry; never force-push for this.
- Overlay text (`main.cpp`) is stamped into the canvas, pushed, then the
  pixels under it are restored. Drawing on the display after `pushImage`
  flickered badly, because the next push erases it. The canvas must end each
  frame exactly as the hack left it.
- Read `push=` from a hack's second 5 s window on: the name overlay is stamped
  inside the push timing while it shows, so the first window reads high.
- Leak-testing with `-DROTATE_SECONDS=5`: internal heap falls about 26 KB
  during the first lap and then stays flat. Compare lap 2 with lap 3, not
  with lap 1. PSRAM should match exactly on every visit.
- Mutation-checking wrap-around code: a mutation that just reorders unsigned
  arithmetic can be equivalent and survive. Use `int64_t` to break it for real.
- Board details (pins, power, recovery) are in
  `.claude/skills/m5stack-stopwatch/`.
