# Gears on the StopWatch: a GL layer over TinyGL

Status: draft for Rod's review. Issue #42. Planning artifact: this file stays
on the feature branch and is not merged to `main`.

## Goal

Land Gears, the first OpenGL hack, on `main`. It runs on the board through a
small GL layer built over TinyGL (a software OpenGL), with the hack and its
helpers byte-identical to xscreensaver 6.16.

Landed means:

- Gears is registered, starts from the button cycle, and runs on the device.
- Every new piece has a test that was seen to fail first, and the whole host
  suite passes.
- Licences and notices are complete and `tools/check_notices.py` passes.
- The gotchas a future GL port needs are in `.claude/rules/`.

## What is already proven

A throwaway spike (branch `worktree-spike-tinygl`, commit `2115083`, write-up
in `docs/tinygl-spike.md` there) runs the unmodified `gears.c` on the board.
Light layouts reach about 9 fps and the heaviest about 4 to 5 fps. The
per-frame cost of the heavy layout is 25 ms clear, about 40 ms fill and
100 to 120 ms vertex and triangle setup, plus the 32 ms push. Speed work
belongs to #43.

## Non-goals

Speed work (#43), Pipes (#44), other GL hacks (#45, #46), textures, fog, and
changing the runner or push path.

## Decisions

| Question | Decision | Who |
| --- | --- | --- |
| How is TinyGL carried? | Pristine copy plus a build-time patch | Rod |
| Gears' random layouts | Land as is; lighter layouts come with #43 | Rod |
| Where do copied files go? | Gears in `hacks/gears/`, helpers in `xs_support/glx/` | proposed |
| How is the context freed? | A release hook in the shim, no runner change | proposed |
| Sweep length for Gears | Cap if slow under ASan; decide from a measurement | proposed |

## Design

### 1. TinyGL, patched at build time

`firmware/src/tinygl/` holds a byte-identical copy of the C-Chads fork at
commit `36a7987` (`src/*.c`, `src/*.h`, `include/`, and its `LICENSE`). It
is excluded from every environment's `build_src_filter`, as Galaxy and Braid
are.

`tools/tinygl_patch.py` holds the patches as small functions, in the style of
`tools/pacman_patch.py`. Each replaces one anchor text, checks that the anchor
occurs exactly once, and raises "has upstream changed?" otherwise.
`firmware/patch_tinygl.py` is a PlatformIO pre-script, added to all three
environments, that writes the patched tree to `generated/tinygl/` in the
build directory, plus one wrapper `tinygl_unity.c` that includes every patched
source in sorted order. TinyGL compiles cleanly as a single unit (checked).
`src/glshim/tinygl_build.c` includes that wrapper, and the script adds the
generated `include/` to the include path.

The patches, all proven in the spike:

- 16-bit pixels, and `RGB_TO_PIXEL` writes the canvas's byte-swapped RGB565,
  so TinyGL draws straight into the canvas with no copy.
- The custom allocator sends allocations of 192 bytes or more to PSRAM.
  `memory.c` is replaced, and the patch also fixes its include order.
- `glGenLists` starts at name 1: hacks treat 0 as invalid.
- `OP_BUFFER_MAX_SIZE` drops from 4096 to 64. Each display list otherwise
  takes a 16 KB buffer, and 500 lists overflow PSRAM.
- The buffer width is no longer rounded down to a multiple of 4.
- `memset_s` is renamed (it clashes with macOS headers).
- Single precision: `tools/float_literals.py` on the files that use decimal
  literals, and `sqrt`, `pow`, `sin`, `cos` and `floor` mapped to their `f`
  versions by a small generated header. Stock TinyGL does software `double`
  maths here.

### 2. The GL layer

`firmware/src/glshim/` is ours (MIT):

- `glshim.h` adds the prototypes TinyGL's header lacks, the GLU and GLX
  stand-ins, and the renames that reroute the six vertex-array calls.
- `init_GL` opens TinyGL at the canvas size over `canvas->px`.
  `glXMakeCurrent` is a no-op. `glXSwapBuffers` marks the whole canvas dirty.
- `gluPerspective`, `gluLookAt`, `glMateriali`, `glIsEnabled`,
  `current_device_rotation` and the framework's GL hooks (FPS drawing and
  visual picking, which mean nothing here) are small stand-ins.
- The array shim reads arrays at the draw call, with the byte stride GL
  defines, and expands `glDrawArrays` into `glNormal3f` and `glVertex3f`.
  TinyGL counts stride in floats and reads arrays at list replay, after the
  hack has freed them.
- The context is freed when the hack stops. `x11shim` gains one optional
  release hook (`xshim_set_release_hook`), which `xshim_release_pixmaps`
  calls once and clears. `init_GL` sets it to a function that closes TinyGL.
  The runner is untouched, and `x11shim` stays free of GL.

The spike's seed, instrumentation counters and experiment flags are dropped.

### 3. Gears and its helpers

- `firmware/src/hacks/gears/gears.c` and `firmware/src/xs_support/glx/` hold
  byte-identical copies of `gears.c` and its helpers (`involute`, `tube`,
  `normals`, `rotator`, `gltrackball`, `trackball`, `quaternion`, with their
  headers). `cmp` against `vendor/` proves it.
- One-line wrappers set `USE_GL` for one file each and include the copy, one
  translation unit per helper as upstream builds them (`tube.h` and
  `normals.h` define clashing types). `hacks/gears_gl.c` wraps Gears itself;
  the helper wrappers sit in `src/glshim/` as `glx_<name>.c`. `HAVE_GL`
  stays undefined: it changes the layout of `ModeInfo`, which `xlockmore.c`
  is built without.
- Both copy directories are excluded from `build_src_filter` in all three
  environments.
- `registry.c` registers Gears last, with
  `XLOCKMORE_HACK_WITH(gears, "Gears", kGearsOverrides)`. The overrides give
  `ncolors` and `cycles` the framework's own default values, which silences
  the "no default for resource" warnings without touching the shared table.

### 4. Shared-header changes

Additions only, and none is visible to a hack that does not set `USE_GL`:

- `XEvent` gains `xany`, `xmotion` and more `xbutton` fields, plus
  `ButtonPress`, `ButtonRelease`, `MotionNotify` and `Button1` to `Button5`.
  `gltrackball.c` needs them.
- `screenhackI.h` includes `utils.h` (for `countof`) and `glshim.h` only under
  `USE_GL`. `yarandom.h` defines `frand` only under `USE_GL`.

Rod: this touches headers every hack includes, though additively. Say if you
would rather keep it out of `xshim.h`.

## Error handling

`init_GL` returns `NULL` if the z-buffer cannot be allocated, and Gears then
draws nothing (its own check). The PSRAM cost is the z-buffer (434 KB) plus
display lists (about 200 KB for the largest layouts), against 6.5 MB free.

## Testing, in this order

Each test is written first, watched failing for the right reason, and then
mutation-checked by breaking the code it covers.

1. `tools/tests/test_tinygl_patch.py`: each patch applies once, and fails
   loudly if its anchor is missing or doubled.
2. A new `firmware/test/test_glshim` over a `Canvas`:
    - the first list name is at least 1 (mutation: revert the patch);
    - a flat red triangle gives `rgb565(255, 0, 0)` in the canvas, and green
      likewise (mutation: drop the byte swap; distinct-colour counts cannot
      see a consistent swap, so this compares exact values);
    - a display list built from an interleaved array with a 32-byte stride
      draws the same pixels as immediate mode, after the array has been
      overwritten (mutations: stride in floats, deferred read);
    - `gluLookAt` and `gluPerspective` put a known point at a known pixel
      (mutation: transpose);
    - after the hack stops, the sanitizer's allocated bytes return to the
      baseline (mutation: remove the hook).
3. `test_xshim`: the release hook runs once and is cleared.
4. `test_hacks`: Gears is registered and runs a capped sweep (decide the cap
   from a measurement; the suite is about 95 s now), and a golden frame hash
   is pinned after `srandom(1)`. Start and stop before the first frame must
   not crash.

The host cannot prove the 16 KB loop stack, so that check is on the device.

## Verification on the device

Flash only with Rod's say-so (he has granted it for this work). Report
"compiles", "host tests pass" and "runs on the device" separately.

- Pinned build: fps, step and push from the serial log, for several layouts.
- Loop stack high-water mark (temporary `uxTaskGetStackHighWaterMark` in the
  stats line), start-up time to the first frame, and `press_waited`.
- Leak test with `-DROTATE_SECONDS=5`: PSRAM equals on every visit to Gears,
  internal heap flat from the second lap.
- Rod looks at the screen for gear colours and motion. Byte order is proven
  by the exact-value test, not by eye.

## Documentation and bookkeeping

- `THIRD_PARTY_NOTICES.md`: TinyGL (zlib-style, with the acknowledgment its
  first clause requires); a row for each copied Gears file; a separate
  Silicon Graphics section for `trackball.c` and `trackball.h`, whose notice
  differs from jwz's.
- `tools/check_notices.py` also scans `src/tinygl/`, with a test.
- `README.md` credits TinyGL; `.claude/rules/gl.md` records the gotchas
  (list name 0, array stride and timing, `HAVE_GL` and `ModeInfo`, the missing
  functions, `-O2` and specular tables not helping).
- `tools/assessment_measured.md` gets a measured row for Gears.
- Issue #41 (stale xlockmore wording in the scorer) stays separate.

## Delivery

Branch `feature/42-gears-gl`, commits per step with named paths, a draft PR
through `entire trail create` whose body lists what is unverified, `Fixes #42`
read back after creation. Rod approves and merges, then the branch is
deleted. After a rebase, a force push needs Rod's ask each time.

## Risks

- Gears under ASan may be slow in the sweep (cap it).
- The unity build is checked for syntax but not yet on the device; the spike
  built the files separately.
- Gears' start-up does `double` maths in `involute.c`; the time to the first
  frame is not yet measured.
- Heavy layouts at 4 to 5 fps will appear in the rotation (agreed).
