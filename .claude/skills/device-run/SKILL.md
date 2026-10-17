---
name: device-run
description: Use when flashing the StopWatch, pinning a hack or setting build flags (START_HACK, ROTATE_SECONDS, BADGE_NAME, QR_URL, toaster flags, GL_PROFILE), capturing the serial log, A/B testing builds with ab_flash.py, or reading fps, push, heap and psram on the device.
---

# Flashing and measuring on the device

Moved verbatim from AGENTS.md.

- `pio run -e stopwatch`: build for the device. Add `-t upload` to flash.
  The device starts on a random hack; to pin one while examining it, prefix
  `PLATFORMIO_BUILD_FLAGS='-DSTART_HACK=\"galaxy\"'` (a name from `g_hacks[]`,
  any case; an unknown name falls back to random and logs it). Rebuild without
  it afterwards, since the define sticks to the build. Unescaped quotes fail
  with "error: 'truchet' was not declared in this scope".
  It rotates to the next hack every 90 s (a button press restarts the count);
  `-DROTATE_SECONDS=5` shortens that for leak testing and `=0` turns it off. A
  pinned build still rotates, so add `=0` to measure one hack.
  The log prints `rotate -> <hack> heap= psram=` at each change. Combine flags
  in one `PLATFORMIO_BUILD_FLAGS` string.
  `-DBADGE_NAME=\"Rod\ B.\"` sets the name badge text (default "Rod" over
  "Begbie"; `\n` starts a new line). Check a
  flag reached the binary with
  `strings -a -n 3 .pio/build/stopwatch/firmware.elf | grep -c '^Rod B\.$'`:
  plain `strings` skips anything under 4 characters.
  `-DQR_URL=\"https://example.com\"` sets the QR overlay's target (default the
  LinkedIn profile). Check it reached the binary with
  `strings -a .pio/build/stopwatch/firmware.elf | grep -c 'example.com'`.
  In a worktree session the harness refuses backslash-escaped flag values and
  `$(cat file)`. Use `PLATFORMIO_BUILD_FLAGS="'-DBADGE_NAME=\"Rod B.\"'
  '-DSTART_HACK=\"gears\"' -DROTATE_SECONDS=0"` with `.venv/bin/pio run -d
  firmware -e stopwatch -t upload` from the worktree root.
- Serial log: `.venv/bin/python -I tools/serial_capture.py <out-file>
  <seconds> [port]` reads `/dev/cu.usbmodem112401` at 115200 into a file and
  reopens the port when it goes away. Run it in the background with absolute
  paths, since background shells ignore `cd`. Lines to read:
  `run <hack> fps= step= push= sync= wait= rows=` (ms per frame in the hack,
  the push, the wait for the panel's TE edge before a synced push (only
  Unknown Pleasures; not counted in `push=`) and the wait; mean canvas rows
  sent per frame, 466 being a full push,
  though overlay redraws made while waiting can push it higher)
  and `switch -> <hack> press_waited=`. An empty capture, or "Device not
  configured", means the USB port went away and came back (a flash, a crash):
  reopen it in a loop. Opening the port resets the board
  (`rst:0x15 USB_UART_CHIP_RESET`, even with DTR/RTS held low): the overlay
  returns to nothing and a random hack starts. To measure one hack, flash a
  pinned build (`START_HACK` plus `ROTATE_SECONDS=0`), start the capture, then
  tap during it: the early windows are the baseline, the later ones have the
  overlay.
  `serial_capture.py` sets `dtr` and `rts` to False on an unopened
  `serial.Serial()` before `open()`, as any script of your own must: a reopen
  loop without that once parked the chip in its bootloader
  (`boot:0x3 ... waiting for download`, screen frozen). Recover, or reset just
  before a capture to log from boot (a hack's `fprintf(stderr)` reaches the
  log), with `.venv/bin/python .platformio/packages/tool-esptoolpy/esptool.py
  --chip esp32s3 --port /dev/cu.usbmodem112401 --after hard_reset read_mac`.
- Flash with `pio run -e stopwatch -t upload > file 2>&1`, never piped through
  `head`, and check for "Hash of data verified". The button steps backwards
  (Pyro, then Lightning, then Drift...). Work out how long a restart takes
  from the hack's cycle count times its frame time (Substrate: 10,000 cycles
  at about 160 ms is 27 minutes) and capture that long, with rotation off.
- `ad-flyingtoasters` force flags (add to `PLATFORMIO_BUILD_FLAGS`):
  `-DAD_TOASTERS_ART=0|1` (0 is 1-bit, 1 is colour) and
  `-DAD_TOASTERS_DONENESS=0|1|2` (light, medium, dark) and
  `-DAD_TOASTERS_SCALE=1|2` (2 is the default, with 6 flyers; 1 is the
  original size, with 10); a number outside those is an `#error`. It logs
  `ad-flyingtoasters: <art>, doneness=<level>` to stderr at start.
- To see where a GL hack's frame goes, add `-DGL_PROFILE`: a `glprof` line
  follows each `run` line with time per stage (clear, list, vertex, shade,
  tri, fill, point, line; nested, so vertex includes tri and fill). Time left
  over is the hack's own code. To compare builds, `.venv/bin/python -I
  tools/ab_flash.py --hack Gears --seconds 60 --rounds 2 --out <dir>
  "a=-DGL_PROFILE" "b=-DGL_PROFILE -DFOO"` builds each variant pinned, flashes
  them in turn and tabulates the windows. It flashes: run it only with Rod's
  go-ahead. It builds each hack's variants from the working tree when that
  hack's turn starts, so leave `src/` and `tools/tinygl_patch.py` alone until
  the last hack has built. It compares flag variants of one tree: to A/B a
  change, put it behind a temporary `-D` switch (as `-DPUSH_NO_DISC` does).
  It takes one `--hack` per run: loop several from a scratchpad script run
  with `run_in_background`, and `touch` a done file at the end.
- `-DDCACHE_LINE32` keeps the SDK's 32-byte data-cache lines for every hack
  (the default is 64 bytes, see `device-display.md`), so it is the A side of an
  `ab_flash` comparison. `-DPSRAM_BENCH` boots into `diag/psram_bench.h`
  instead of the runner and prints cost per cache miss, sequential rates and a
  working-set sweep; `-DPSRAM_BENCH_VARIANT=0..3` picks the line size and
  prefetch setting.
- `esptool` reads of the 16 MB flash need `--baud 921600` (about 3.5 minutes)
  and show no progress when piped.
- The port name changes with the USB socket the board is plugged into
  (`/dev/cu.usbmodem112401` became `/dev/cu.usbmodem101` on 2026-10-14). When
  it is missing, find the board without touching it:
  `ioreg -p IOUSB -l -w0 | grep -E "\+-o|idVendor|idProduct|USB Product
  Name|USB Serial Number"` lists USB devices, and
  `ioreg -r -c IOSerialBSDClient -l | grep IOCalloutDevice` lists the serial
  ports. The StopWatch is the "USB JTAG/serial debug unit" from Espressif
  (`idVendor` 12346, `idProduct` 4097). Its USB serial number is its MAC:
  Rod's board is `28:84:85:43:B2:70`, so check that before flashing a port
  you have not used before. Pass `--upload-port <port>` to `pio run -t upload`
  and read the same port for the serial log, and leave `platformio.ini`'s
  `upload_port` alone. Still ask Rod before each flash. The auto-mode
  classifier refused a flash to an overridden port once: give Rod the command
  to run with `!`.
- Another session may be using the board. Before flashing, run `lsof
  /dev/cu.usbmodem112401`: a "multiple access on port" upload error was
  another session's serial capture, and esptool's connect resets the board
  under it. Ask Rod before flashing over someone else's run.
- PlatformIO cleans the build directory, image and all, when
  `PLATFORMIO_BUILD_FLAGS` changes: an `-t nobuild -t upload` with other flags
  finds no `firmware.bin`. To keep several images, give each its own
  `PLATFORMIO_BUILD_DIR` (the build scripts all use `$BUILD_DIR`).
- Read `push=` from a hack's second 5 s window on: the name overlay is stamped
  inside the push timing while it shows, so the first window reads high.
- `heap=` and `psram=` are the reading at the end of a window, not a minimum:
  a hack with a long pause and nothing held in it (Celtic's 5 s `*delay2`)
  reads idle in every window. Sample while it works, or read `rotate ->`.
- Report a hack's lowest `fps=` window as well as its mean. HexTrail's step
  grows over a 65 s cycle: mean 15.3, worst window 10.8, and Rod saw an earlier
  build sag under 10 fps before the rotation moved on. Capture a full cycle and
  say which build the board holds when you stop.
- A boot is deterministic (same hack, layout and windows), so to compare two
  builds flash them alternately and compare window by window; one reading in
  three captures was an unexplained outlier, so never trust a single pair.
- Leak-testing with `-DROTATE_SECONDS=5`: internal heap falls about 26 KB
  during the first lap and then stays flat. Compare lap 2 with lap 3, not
  with lap 1. PSRAM should match exactly on every visit.
- To prove a device-only path the host cannot run (the ROM's `tinfl`), add a
  temporary `#ifdef` block after `printStats("boot")` in `main.cpp` that logs
  a checksum, compare it with one computed in Python, then `git checkout
  firmware/src/main.cpp` before pushing.
