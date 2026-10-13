# Pipes lab (issue #44)

A host-only experiment, not a port. It builds Pipes and its helpers straight
from `vendor/` against the shim and `tools/gl_budget.py`'s instrumented
TinyGL, and prints one line per frame: vertices, lit vertices, filled pixels,
the rows spanned by what was drawn (`drawn_rows`; the swap also marks the rows
a clear erased, so a fade or a new screen marks more), and a hash of the
canvas.

```bash
python3 -I docs/pipes-lab/build.py /tmp/pipes --incremental \
    '*delay: 10000' '*ncolors: 64' '*cycles: 1' '*size: 100' \
    '*rotatepipes: False'
/tmp/pipes/prog 3000 1
```

Without `--incremental` it is Pipes as written. `HEAP=1` adds the host heap in
use after each frame; `ASAN=1` builds
with AddressSanitizer.

## What it changes, and why

- `pipes_incremental.h` is the experiment: an exact incremental redraw. Pipes
  clears and replays every piece's display list each frame, in order, under
  one camera, and adds one piece a frame. When the clear, both matrices and
  the lists so far match last frame, the canvas and z-buffer already hold
  them, so the wrapper skips the clear and those lists and replays only the
  new one. Anything else falls back to a real clear and a full replay.
- `teapot.c` is built with `HAVE_JWZGLES`, which picks its triangle table
  (`teapot2.h`) over GL evaluators (`glMap2f`, `glEvalMesh2`), which TinyGL
  lacks.
- `buildlwo.c` is built with `GL_POLYGON` mapped to `GL_TRIANGLE_FAN`. TinyGL
  is built without `GL_POLYGON`, and an unhandled primitive keeps counting
  vertices past `c->vertex[3]` and overwrites the context: the lab crashed on
  frame 16 in `first_light`.

## Results (host, seed 1)

Frames are identical with and without the wrapper: every frame hash matched
over 600 frames, with the camera turning (the default, where the wrapper never
skips) and with it fixed.

| Settings | Extra host heap | Steady vertices a frame | Fade vertices a frame |
| --- | --- | --- | --- |
| As written (`cycles` 5, `size` 500), full redraw | about 44 MB | up to 977,000 | 977,000 |
| As written, incremental, camera fixed | about 44 MB | 380 | 977,000 |
| `cycles` 1, `size` 100, incremental, fixed | 1.2 to 1.7 MB | 307 | 32,200 |
| `cycles` 1, `size` 200, incremental, fixed | 2.7 to 3.6 MB | 327 | 69,800 |
| `cycles` 2, `size` 150, incremental, fixed | 4.4 to 4.9 MB | 321 | 100,400 |

- Extra heap is over a `size` 10 run (3.2 MB), and is mostly display lists.
  A list word (`GLParam`) holds a pointer, so it is 8 bytes on the host and 4
  on the board: halve these for the board. Pipes as written needs about 22 MB
  there, and the board has about 7 MB of PSRAM free. Pipes builds every list
  of a screen when the screen starts.
- A screen ends with a 25-frame fade that scales the whole picture, so every
  fade frame is a full redraw. At the #43 costs (4 us a vertex, 2.8 us more
  for each light on each lit vertex) a 32,200-vertex fade frame is about
  0.2 s.
- With the default turning camera every frame is a full redraw, so the
  wrapper never helps.

None of this has run on the board.

## Decisions, and the port

Rod's answers on issue #44: a fixed camera, smaller screens, no fade (fades
later), and copying the header-less helpers into the repo (`xscreensaver.spec`
says "License: BSD"). The firmware port is `firmware/src/hacks/pipes_gl.c`,
which carries this experiment over with two fixes the lab did not need:
recompiling a list ends the skipping (each new screen reuses the list names),
and the next screen is generated at the start of a draw. On the board it runs
at about 60 fps with three systems of 150 (`tools/assessment_measured.md.in`).
This lab leaves out the firmware's helper wrappers and builds its own copies.
