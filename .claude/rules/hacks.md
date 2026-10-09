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
