# Porting techniques

Recipes for the traps listed in `SKILL.md`. Each says when it applies, what to
do, and what it did the last time.

## Build-time patch

When: the hack's own constants make it too big for the board. Maze keeps a
1000 by 1000 maze and three 1,000,000-entry move lists in one `calloc`, about
20 MB, and the board has 8 MB of PSRAM.

- The copy in `hacks/<name>/` stays byte-identical to upstream (`cmp` it).
- A PlatformIO `pre:` script (`firmware/patch_maze.py`) writes a patched copy
  into the build directory, using a tested function (`tools/maze_patch.py`)
  that raises if upstream's lines change. A wrapper (`hacks/maze_small.c`)
  includes that copy, and the original is excluded in `build_src_filter` of
  all three environments (`stopwatch`, `native`, `dump`).
- Maze's limits went from 1000 to 80: about 125 KB.
- A patch can switch off a branch (Pacman's `NRAND (2) == 0` becomes `0`) or
  replace a whole function (its route search becomes a loop). Check the
  original's SHA-256 so an upstream edit fails the build. Before a rewrite, pin
  a long-run fingerprint taken with the original (30,000 frames, every ghost's
  position every 100), show the code ran, and mutate the order to watch the pin
  fail.

## Single precision

When: a hot loop does `double` maths or calls `sin`, `cos`, `sqrt` or `pow`.
The ESP32-S3 has a single-precision FPU only, so double runs in software.

- Find the hot loop first and count the calls. Substrate makes four `sin` calls
  per sand grain, with 64 grains a crack and up to 100 cracks: about 25,000
  software `sin`s a frame. Its step grew from 5 ms to 121 ms as the picture
  filled.
- Wrapper `hacks/<name>_single.c`: include the framework headers first, then
  `hacks/single_precision.h`, then the unmodified hack. Exclude the original in
  `build_src_filter` of all three environments.
- `M_PI` is a double literal, so the angle arithmetic stays double: put
  `#undef M_PI` and a float definition after the header. Literals such as
  `0.81` stay double too, and a header cannot change them: `0.5 * x` with a
  float `x` is software double maths. Check the wrapper's object for soft-double
  helpers with `xtensa-esp32s3-elf-nm braid.o | grep ' U __.*df'` (compile with
  `-Os -mlongcalls` and the shim's include paths). If any remain, rewrite the
  literals at build time as Braid does: `tools/float_literals.py` (tested, skips
  strings and comments) run by `firmware/patch_float_literals.py`, and the
  wrapper includes `<name>_floatlit.c` from the build directory. GCC's
  `-fsingle-precision-constant` does the same, but Apple clang ignores it, so
  host tests would run other arithmetic than the board.
- Galaxy, Drift, Discrete, Flame and Substrate were wrapped before this check
  and have not been looked at for double literals.
- Hand-written `sin` and `cos` (`hacks/fast_trig.h`) moved Braid by 5%. Do the
  literals first.
- Compare double and float frames at several counts. Early frames match; a
  chaotic hack diverges later but should keep its look.
- Results: Galaxy 5 to 14 fps. Substrate step 119-121 ms to 37-38 ms, 6.2 to
  12.5 fps.

## Slow hack

When: a ported hack runs under about 10 fps and the push is not the cause
(`step` in the serial line is large).

- Do not trust a host profile (`sample` on the dump program) for where the
  time goes. A Mac does `double` at full speed, so software-double code looks
  cheap there: Braid's disc fill was 6% on the host and the largest cost on the
  board.
- Split the step on the device with a temporary build flag that makes the
  expensive call return at once (Braid: `XDrawLine`). The step left is the
  hack's own cost; the difference is the drawing. Remove the flag before
  committing.
- Count what is called per frame (segments, calls, rows) before guessing,
  and look at the object file for soft-double helpers (see "Single precision").
- Change one thing, pin that the pixels did not move (`cmp` frames at several
  counts, or the pinned hash), flash, and compare second windows. Braid's four
  changes were worth 2.7-3.2 times, 1.05, and 1.3.
- Wide lines (`line_width` above 1) are the costly path: a quad filled a row at
  a time, plus two discs for round caps. `stroke.c` keeps a table of disc rows
  by width up to 16.
- A hack that redraws its whole picture every frame to animate colour (Braid)
  costs its full draw each frame whatever the push does. Check this before
  porting; the scorer does not flag it yet.

## Stack check

When: locals over a few KB. Compile the hack with `-O1 -fstack-usage` (flags in
AGENTS.md) and keep each frame well under the 16 KB loop stack. Host tests
cannot see an overflow: Rorschach's 9.6 KB array rebooted the device on its
first frame, and only the serial log's "Stack canary" showed it.

Recursion: replay it, don't guess. Run the hack on a pthread with a painted
1 MB stack over several seeds and thousands of frames (Pacman's generator: 236
KB median, 315 KB worst; `ulimit -s` saw only the first, shallow level). Read a
frame's size from the ELF (`xtensa-esp32s3-elf-objdump -d`, `entry a1, N`), and
decode a panic backtrace with `xtensa-esp32s3-elf-addr2line` against an ELF
rebuilt with the same flags. A recursion over a grid is bounded by its cells:
replay it from every start.

## Large allocations

When: one allocation of 100 KB or more, or the hack calls `exit()` if it fails.

- `unsigned long` is 4 bytes on the device and 8 on the host, so host leak
  numbers are double the device's.
- Plain `malloc` and `realloc` of two 868 KB buffers (Substrate) landed in
  PSRAM: free PSRAM fell by 1,742,096 bytes, which is 2 x 466 x 466 x 4 plus a
  few KB. Internal heap barely moved.
- On restart the buffers were freed and allocated again, and free PSRAM
  returned to the same value. Read heap and PSRAM in the log to confirm this
  for each large hack.

## Images

Only `XCopyArea` writes into a pixmap; `XCopyPlane` reads one. There is no PNG
decoder: `image_data_to_pixmap` reads a raw RGB565 blob and
mask (format in `ximage-loader.h`), made by `tools/make_logo_blob.py`.
`XS_LOGO=<image>` (relative to the repository root) swaps the logo for a local
image at build time. Keep a third-party logo under `vendor/`, out of git.

A big blob (Pacman's is 3 MB as C text) is built from `vendor/` by a `pre:`
script and never committed; write it to a `.partial` file and rename it.

## Restart leak test

When: the hack restarts itself. Override its cycle resource in the test (for
example `*maxCycles: 3`), compare `__sanitizer_get_current_allocated_bytes()`
before and after many restarts, and prove the test sees a leak by removing a
`free` in a temporary copy or leaking from a shim function. Never leave the
hack changed: `cmp` it afterwards.

Measure the growth first, then set the tolerance just above it (Pacman: exactly
0, so 512 bytes). A loose one passes a leaked GC; mutation-check by leaking one.

On the device, work out how long a restart takes: cycles times the frame time.
Substrate's 10,000 cycles at about 160 ms is 27 minutes, and the restart showed
at about 25. Capture that long, with `-DROTATE_SECONDS=0`.

## Stopping before the first draw

When: the hack allocates on its first `draw` (look for a lazy `initted` flag)
but sizes its `free` from values set in `init`. Two quick button presses, or a
press with the 90 s rotation, free a hack before it has drawn once. Blaster
sets `NUM_ROBOTS` in init, allocates `robots` in draw, and its free loops over
`robots[i].lasers`, so freeing it first dereferenced NULL. The dump tool with 0
frames exited 139.

- `test_every_hack_can_be_stopped_before_its_first_frame` starts and stops every
  hack with no frame, so a new port that has this problem fails the host suite.
- To fix it without editing the hack, write a wrapper that includes the
  unmodified source with `XSCREENSAVER_MODULE` emptied and registers the hack
  itself, with a `free` that makes the loop safe (`hacks/blaster_safe.c`).
- A real press is rarely handled in that window: Rod's presses on the device
  all landed after the first step. Rely on the test, not on pressing buttons.

## Tests that cannot be fooled

A test that runs a real hack can pass for the wrong reason. Pyro clears its own
window in `init`, so a "starts black" test through Pyro passed with the runner
deliberately broken. Test shared runner behaviour with stub hacks
(`test_runner_xsft.c`) and mutate the code to see the test fail.

A check can also be true before the hack has run. "At least 20 non-black
pixels" proved a hack drew, until Substrate started on a white canvas and
passed without drawing at all. Compare with the canvas as the hack started, and
keep a stub hack that draws nothing, on the colour that fooled the old check, to
show the test can fail. When a change alters what every hack starts with (the
background colour, an initial canvas), re-read the older tests that assume the
old start.

## Background colour

The runner paints the canvas in the hack's `background` resource before `init`,
as `screenhack.c` paints the window. Substrate (`.background: white`) depends
on it. Every earlier hack asks for black or nothing.

## Skipping a hack

Threads, GL, Xft and shared memory are not worth porting. Record the name and
the reason in `tools/failed_ports.txt` (`name: reason`); the scorer marks it as
abandoned.

## Worktree setup

A fresh worktree lacks the ignored `vendor/`, `.venv/` and `.platformio/`:
symlink them from the main checkout. They show as untracked, so `git add`
named paths, never `-A`. Adding them to `.git/info/exclude` changes state shared
with every worktree: ask Rod first.

A worktree session refuses `source`, `$VAR` in a command and `&&` chains with a
computed path. Run plain commands with literal paths, for example
`PLATFORMIO_CORE_DIR=<worktree>/.platformio ../.venv/bin/pio test -e native`.

## Device capture

- Pin the hack with `-DSTART_HACK=\"name\"` and add `-DROTATE_SECONDS=0`, or the
  board rotates to the next hack after 90 s.
- Send upload output to a file, never through `head`, and look for "Hash of
  data verified".
- Capture 150 s or more of serial in the background, longer to see a restart.
  Read `step`, `push`, `wait`, heap and PSRAM, and any "Stack canary".
- Compare with a baseline from the same build, not with old rows: pushes became
  41-45 ms on every hack after the byte-order fix, against 31 ms before (#23).
- Pinning a frame hash: set the new hack's `kBaseline` entry to `1`, run
  `pio test -f test_hacks`, read `Was N` in the failure and convert with
  `python3 -c "print('0x%016xull' % N)"`. Pin only after looking at the frame
  (and never type the hex by hand: one was wrong).
- Showing Rod a hack: flash `-DSTART_HACK=\"first\" -DROTATE_SECONDS=25` with
  `XS_LOGO=...` so the new ones come up in order, then tell him which, and that
  the button steps backwards (from Pyro it wraps to the last hack).
