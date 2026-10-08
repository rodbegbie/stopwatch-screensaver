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

- `firmware/src/core/`: RGB565 `Canvas` and clipped drawing primitives (C).
- `firmware/src/x11shim/`: Xlib look-alike drawing into the canvas, resource
  lookups from each hack's defaults table, and `include/` stand-ins for
  xscreensaver's `screenhack.h`, `utils.h`, `hsv.h`, `erase.h`, `colors.h`.
- `firmware/src/hacks/<name>/`: hack sources copied from xscreensaver.
  `hacks/registry.[ch]` (ours) lists them in button order.
- `firmware/src/xs_support/`: other copied xscreensaver files (`hsv.c`).
- `firmware/src/runner/` and `main.cpp`: start, step and switch hacks; the
  device loop. `firmware/native/dump_main.c` renders frames on the Mac.
- `tools/`: fetch, notices check, assessment scorer, PNG converter (Python).
- `vendor/`, `backups/`, `.venv/`, `.platformio/` are git-ignored and local.

## Commands

Run `source tools/env.sh` first (keeps PlatformIO inside the repo), then from
`firmware/`:

- `pio test -e native`: host tests (Unity, AddressSanitizer). Run these first.
- `pio run -e stopwatch`: build for the device. Add `-t upload` to flash.
- `pio run -e dump`, then `.pio/build/dump/program <index> <frames> out.raw`,
  then `uv run tools/rgb565_to_png.py out.raw 466 466 out.png` to see a frame.
- Serial log: read `/dev/cu.usbmodem112401` at 115200 for N seconds into a file
  (a pyserial script run with `.venv/bin/python -I`). Run it in the background
  with absolute paths, since background shells ignore `cd`. Lines to read:
  `run <hack> fps= step= push= wait=` (ms per frame in the hack, `pushImage`
  and the wait) and `switch -> <hack> press_waited=`.
- Flash with `pio run -e stopwatch -t upload > file 2>&1`, never piped through
  `head`, and check for "Hash of data verified". The button steps backwards
  (Pyro, then Lightning, then Drift...), and a hack that restarts every ~70 s
  needs a capture of 150 s or more to show a restart.
- `cc -O1 -fstack-usage -c src/hacks/<n>/<n>.c` (with `-Isrc
  -Isrc/x11shim/include -Isrc/xs_support -DXSHIM_NATIVE -DSTANDALONE`) lists
  stack frames; keep each well under the 16 KB loop stack.

From the repo root:

- `uv run --with pytest pytest tools/tests`
- `uv run tools/check_notices.py`
- `uv run tools/score_hacks.py` regenerates `docs/porting-assessment.md`.
- `markdownlint <files>` (config in `.markdownlint.json`).

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
   `pow`), try a single-precision wrapper like `hacks/galaxy_single.c`
   (candidates in issue #13). A resource override can skip a hack's restart
   cleanup: Galaxy leaked at `count: 2`, so leak-test any override.

## Delivering a branch

- `entire trail create --title ... --type feature --body ...` pushes the branch
  and opens a linked DRAFT PR. The PR body is not synced from the trail: update
  both (`entire trail update --body`, `gh pr edit N --body-file`). Put
  `Fixes #N` in the body.
- Rod approves and merges (merge commit). Afterwards delete the merged branch
  (remote and local) without asking and fast-forward `main`.
- `gh pr edit N --body-file` replaces the whole body, including the
  `<!-- entire-trail-link-start -->` ... `-end -->` block at the top. Keep that
  block in the file, or the PR loses its trail.
- Findings: `entire trail finding list N`, then `... resolve N <id> -m "..."`.
  Bot reviews can lag about 20 minutes. `N` is the trail number (PR #12 was
  trail 6), not the PR number. For a false positive or upstream behaviour in a
  byte-identical hack, use `entire trail finding dismiss N <id> -m "<reason>"`.

## Gotchas

- M5GFX reads a plain `uint32_t` colour as RGB888. `TFT_RED` and friends are
  RGB565 constants. `pushImage` with a `uint16_t*` canvas wants
  `setSwapBytes(false)`.
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
- The runner caps a hack's delay at 10 s, and the loop credits only the 31 ms
  push against it, because a hack's delay is its pause after drawing.
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
  `XLOCKMORE_HACK_WITH`. Galaxy is built through `hacks/galaxy_single.c`, which
  includes the unmodified `galaxy.c` with `double` redefined as `float` (the
  S3's FPU is single-precision only); `galaxy.c` is excluded from each env's
  `build_src_filter`.
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
- zsh does not word-split `$var`: loop over file lists with `bash -c`.
- jwz.org returns 403 to Python's default User-Agent.
- `esptool` reads of the 16 MB flash need `--baud 921600` (about 3.5 minutes)
  and show no progress when piped.
- A push to GitHub is sometimes rejected once and succeeds on an immediate
  retry; never force-push for this.
- Board details (pins, power, recovery) are in
  `.claude/skills/m5stack-stopwatch/`.
