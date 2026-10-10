---
paths:
  - "firmware/src/glshim/**"
  - "firmware/src/tinygl/**"
  - "firmware/src/xs_support/glx/**"
  - "firmware/src/hacks/gears/**"
  - "firmware/src/hacks/gears_gl.c"
  - "firmware/src/hacks/morph3d/**"
  - "firmware/src/hacks/morph3d_gl.c"
  - "firmware/patch_tinygl.py"
  - "tools/tinygl_patch.py"
  - "tools/gl_budget.py"
---

# GL layer and TinyGL gotchas

OpenGL hacks run on TinyGL, a software OpenGL (C-Chads fork, commit
`36a7987`, zlib-style licence). Issues #42 to #46 hold the numbers and ideas.

- `src/tinygl/` is byte-identical to upstream and excluded from every env.
  `firmware/patch_tinygl.py` runs `tools/tinygl_patch.py` and writes the
  patched tree, plus one unity file, into `.pio/build/<env>/generated/`;
  `glshim/tinygl_build.c` compiles it. Each patch checks its anchor and fails
  with "has upstream changed?". TinyGL's README allows building it as one
  unit, and it compiles cleanly that way.
- A GL hack gets `USE_GL` from a one-line wrapper: `hacks/<name>_gl.c` for
  the hack, `glshim/glx_<name>.c` for each helper copied to
  `xs_support/glx/`. Exclude the copies in all three envs' `build_src_filter`
  and keep `-Isrc/xs_support/glx` in `build_flags`. Never define `HAVE_GL`:
  it adds `glx_context` to `ModeInfo`, which `xlockmore.c` (built without
  it) allocates smaller. Build each helper as its own unit: `tube.h` and
  `normals.h` define clashing types.
- TinyGL draws straight into `canvas->px` at the canvas size, with its
  pixel packer patched to the canvas's byte-swapped RGB565. Test colours
  with exact `rgb565()` values: white looks the same either way round.
- Display list name 0 means "invalid" to hacks (`if (!glGenLists(1)) abort()`)
  and TinyGL handed it out first; the patch starts at 1. Its list buffers
  are 64 slots, not 4,096: each list took 16 KB and 500 lists overflowed
  PSRAM (boot loop, `StoreProhibited` at `0x4000` in `alloc_list`).
- TinyGL counts vertex-array stride in floats and reads the array when a
  display list is replayed; GL counts bytes and reads at the draw call, and
  hacks free the array straight after (`tube.c`, `sphere.c`). Hacks'
  `glVertexPointer` and friends are renamed to `glshim_*` by macros in
  `glshim.h`, and `glDrawArrays` expands at once. The symptom of the old
  behaviour was needles through the tubes.
- The context is freed by the shim: `glshim_open` registers
  `xshim_set_release_hook`, which `xshim_release_pixmaps` (the runner's
  `stop()`) runs. A second `init_GL` replaces the first, which hides a
  missing hook, so test `glshim_is_open()` right after the hack stops.
- `glshim_open` returns NULL for an empty canvas or a failed z-buffer
  allocation only. `glInit`'s own allocations (lists, matrix stacks) are
  unchecked upstream, so running out of PSRAM after the z-buffer crashes there.
- `glDrawArrays` reads every client array as `GL_FLOAT` and ignores the type
  argument: check a new hack's `glVertexPointer` calls before trusting it.
- TinyGL leaves specular lighting off (`zEnableSpecular = 0`) unless the
  application calls `glSetEnableSpecular(1)`, so Gears has no highlights
  and `glMateriali` is invisible without it. Enabling it costs speed (#43).
- TinyGL lacks `glTexGeni`, `glFog*` and `glInterleavedArrays` (#46). The
  header declares `glFog*` and the `GL_SPHERE_MAP` enum; the library defines
  neither.
- Speed is about 6 us per lit vertex plus about 80 ms a frame (clear 25 ms,
  push 32 ms): Gears runs at 4 to 5 fps for its heaviest layout and about 9
  for light ones. `-O2` for TinyGL and specular lookup tables changed
  nothing; whole-firmware `-O2` crashes the compiler in `braid_single.c`.
  `CLIP_EPSILON` was an exponent literal (`1E-5`), which `float_literals.py`
  leaves alone, so `gl_clipcode` ran a software `double` multiply on every
  vertex; `tinygl_patch.py` now makes it `1E-5f`, which took 5.6 ms off
  CubicGrid's 52 ms step (0.7 us a vertex) and about 3 ms off Morph3D's.
  `gl_shade_vertex` has not been checked for `double` the same way.
- Gears picks a random layout on each start, so a running Gears holds 0.9 to
  2.5 MB. Measure leaks while stopped, not by comparing two running moments.
  `free_gears` never frees `bp->gears`: 40 to 1,500 bytes a start, upstream.
- The host stack figure for Gears (21 KB, unoptimised and under
  AddressSanitizer) is not the board's (2.4 KB used in the spike); the host
  test is a tripwire and the board is the proof.
- `dump_main.c` raw frames are already un-swapped; `rgb565_to_png.py` reads
  them as plain RGB565. Swapping them again gives psychedelic stripes.
- `tools/gl_budget.py` counts vertices, triangles and fill in a private,
  instrumented copy of TinyGL (`instrument()` adds the counters after
  `tinygl_patch.patch_tree`, with the same checked anchors). The firmware's
  TinyGL never has them. Its fill cost is fitted to one Gears scene, so it
  over-predicts few-large-triangle scenes, and lines and points cost nothing
  beyond vertex setup. Add a second calibration point after each GL port.
- TinyGL's `glLightModelfv` copied four floats whatever the parameter, so
  `glLightModelfv(GL_LIGHT_MODEL_TWO_SIDE, one_float)` (Morph3D) read past the
  array: AddressSanitizer's global-buffer-overflow. `tinygl_patch.py` now copies
  four only for `GL_LIGHT_MODEL_AMBIENT`. `glopLightModel` still reads the
  value back as an int from a float slot, which only works because any
  non-zero float has non-zero bits; leave it unless a hack passes 0.
- Morph3D is small and wanders by design: a window (not iconic) build scales
  the object to 0.3 and moves it on a Lissajous path, so it is about 70 px
  across on the 466 px canvas and its position differs frame to frame. It
  cannot be enlarged without patching the hack. It enables two lights and
  two-sided lighting; `docs/gl-budget.md` predicts 6 to 11 fps.
