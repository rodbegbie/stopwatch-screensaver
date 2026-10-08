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
  `0.81` stay double, and a header cannot change them.
- Compare double and float frames at several counts. Early frames match; a
  chaotic hack diverges later but should keep its look.
- Results: Galaxy 5 to 14 fps. Substrate step 119-121 ms to 37-38 ms, 6.2 to
  12.5 fps.

## Stack check

When: locals over a few KB. Compile the hack with `-O1 -fstack-usage` (flags in
AGENTS.md) and keep each frame well under the 16 KB loop stack. Host tests
cannot see an overflow: Rorschach's 9.6 KB array rebooted the device on its
first frame, and only the serial log's "Stack canary" showed it.

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

The shim's pixmaps are read-only sources for `XCopyArea` and `XCopyPlane`.
There is no PNG decoder: `image_data_to_pixmap` reads a raw RGB565 blob and
mask (format in `ximage-loader.h`), made by `tools/make_logo_blob.py`.
`XS_LOGO=<image>` (relative to the repository root) swaps the logo for a local
image at build time. Keep a third-party logo under `vendor/`, out of git.

## Restart leak test

When: the hack restarts itself. Override its cycle resource in the test (for
example `*maxCycles: 3`), compare `__sanitizer_get_current_allocated_bytes()`
before and after many restarts, and prove the test sees a leak by removing a
`free` in a temporary copy or leaking from a shim function. Never leave the
hack changed: `cmp` it afterwards.

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
