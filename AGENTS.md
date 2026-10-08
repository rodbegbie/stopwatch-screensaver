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
- jwz.org returns 403 to Python's default User-Agent.
- `esptool` reads of the 16 MB flash need `--baud 921600` (about 3.5 minutes)
  and show no progress when piped.
- A push to GitHub is sometimes rejected once and succeeds on an immediate
  retry; never force-push for this.
- Board details (pins, power, recovery) are in
  `.claude/skills/m5stack-stopwatch/`.
