# Banded GL renderer: design

Issue #43. Agreed with Rod on 2026-10-11. This document is a planning
artefact: it lives on the feature branch and does not merge to `main`.

## Why

Rod wants Unknown Pleasures and Hypnowheel to run visibly faster on the
StopWatch. Hypnowheel was dropped from the rotation at 1.4 fps (issue #73).
Success is judged by Rod watching the board ("I know it when I see it"), not
by an fps target, so the work must stop at points where he can look.

Board profiles (`GL_PROFILE` builds of `feature/43-gl-speed-experiments`,
pinned, rotation off; stage times nest):

| Hack | Step | Fill | Triangle setup outside fill | Lines | Clear | Other |
| --- | --- | --- | --- | --- | --- | --- |
| Unknown Pleasures | 174 ms | 69 ms (5,442 tris) | 40 ms | 30 ms (3,960) | 25 ms | push 32 ms + 11 ms sync |
| Hypnowheel | 706 ms | 358 ms (5,949 tris) | 61 ms | 0 | 12 ms | about 275 ms in the hack's `double` trig |
| Gears (boot layout) | 58 ms with lazy lighting | 28 ms (1,600 tris) | 2 ms | 0 | 6 ms | vertex work about 17 ms |

The cause of slow fill is PSRAM. A boot benchmark measured a new 32-byte PSRAM
cache line at 129 cycles to read and about 220 to read and write back, against
17 for internal RAM. Lines a row apart cost the same as lines in address
order, so hardware prefetch cannot help (measured: `PSRAM_AUTOLOAD` changed
Gears by under 1 ms). A small triangle touches new z-buffer and canvas lines on
every row. The Arduino SDK fixes the data cache at 32 KB with 32-byte lines.

The fix is to rasterise into internal RAM: hold a frame's primitives, then
draw the screen in horizontal bands whose colour and depth live in internal
RAM, copying each finished band to the canvas.

## Goals

- Unknown Pleasures and Hypnowheel visibly faster on the board, with Rod
  looking at each step.
- One rendering path for every GL hack, with an automatic fallback (below).
  No per-hack flag decides whether a hack is banded.
- Every registered GL hack keeps working and looks the same to the eye:
  Gears, Morph3D, CubicGrid, StonerView, Unknown Pleasures, HexTrail.

## Non-goals

- Pixel-identical output. Rod accepts small differences, so the pinned frame
  hashes in `test_hacks.c` will be re-pinned.
- Pipes. It is a separate piece of work built on the incremental redraw in
  `docs/pipes-lab/` (branch `feature/44-pipes`). This design must not break
  that redraw, and the fallback is what protects it.
- Vertex and lighting cost. Lazy lighting already landed on the experiments
  branch; per-vertex work is left alone here.
- Moving the push to the other core, a 64 KB data cache, or a faster PSRAM
  clock.

## Anti-goals

- A slower frame for any registered GL hack. The fallback exists so that
  frames the banded path cannot help draw as they do today.
- Taking internal RAM from non-GL hacks. Band memory is allocated only while
  a GL hack runs.
- Editing copied hacks. They stay byte-identical (AGENTS.md).

## Constraints

- ESP32-S3, 240 MHz. About 305 KB of internal heap is free while a GL hack
  runs (`heap=` in the `run` line). Canvas, z-buffer and display lists are in
  PSRAM (about 6 MB free).
- The canvas is 466 by 466 RGB565 with bytes swapped (display order). TinyGL
  draws straight into `canvas->px`, and the z-buffer is 16-bit.
- TinyGL under `firmware/src/tinygl/` stays byte-identical. All changes go
  through `tools/tinygl_patch.py` (checked anchors) or `firmware/src/glshim/`.
- The dirty-rectangle contract (`.claude/rules/gl.md`): TinyGL reports the box
  around what it draws to `glshim_note_box`, `glClear` clears only the box
  drawn since the last clear, and a swap marks only what was drawn and erased.
  Nothing else may draw on the canvas while a GL hack runs.
- Behaviour already patched into TinyGL must survive banding: polygon offset
  with its snapping margin, smooth (Wu) lines with fractional endpoints,
  `SRC_ALPHA, ONE_MINUS_SRC_ALPHA` treated as opaque, additive blending, the
  depth clear skipped while the depth test is off, and lazy lighting.
- Host tests first, under AddressSanitizer; mutation-check new tests.
- Rod has given standing permission to flash for this session. Report
  "compiles", "host tests pass" and "runs on the device" separately.

## Decided

### Order of work

1. **Hypnowheel's trig, first and separately.** A single-precision wrapper for
   Hypnowheel, like Galaxy and Substrate (`.claude/skills/port-hack/`,
   "Single precision"). The hack stays byte-identical. Profile it on the board,
   which needs a temporary registry entry, as Hypnowheel is not registered.
   Rod looks.
1. **The banded renderer**, below. Profile Unknown Pleasures and Hypnowheel,
   and A/B every registered GL hack against the experiments branch with
   `tools/ab_flash.py`. Rod looks.
1. **Half resolution for Hypnowheel, as an option.** Render its GL frame at
   233 by 233 and scale up (as `hacks/lowrez_put.h` does for Fireworkx). Flash
   it and the full-resolution build pinned, and give Rod the numbers side by
   side; he chooses. Not for Unknown Pleasures, whose look is its thin lines.
1. **Whether Hypnowheel goes back into the rotation** is Rod's call after he
   has seen it.

### How a frame is drawn

- **Held frames.** A frame is held when it begins with a `glClear` of both the
  colour and the depth buffer (or of the colour buffer with the depth test
  off). From then until the swap, each triangle, line and point is transformed
  and lit as now and then stored, not drawn. A stored primitive keeps its
  screen-space vertices (`ZBufferPoint`, including the fractional line
  positions), its kind, and the state that affects rasterising: blend on and
  its function, depth test, depth write, polygon offset, shade model, smooth
  lines, and the dirty-box margin rule.
- **At the swap** (`glXSwapBuffers`, and any call that must see the pixels),
  the screen is drawn band by band. Each band starts in internal RAM as the
  clear colour and clear depth, gets every stored primitive that touches its
  rows, and is then copied to the canvas. Only rows inside the dirty box are
  copied; the dirty-box bookkeeping and the push are unchanged.
- **Fallback.** A frame draws straight to the canvas, as today, when it does
  not begin with such a clear (Pipes' incremental redraw), or when the store
  fills up. On overflow, the rest of the frame falls back. The builder may
  instead flush part-way, provided the result is correct.
- **Depth is not kept between frames.** A banded frame's depth lives only in
  the band and never reaches the PSRAM z-buffer; only colour goes to the
  canvas. A hack that draws against the depth of earlier frames (Pipes'
  incremental redraw) turns banding off for its context with a `glshim` call
  from its own wrapper, which is our code, not the copied hack. Rod chose this
  over writing depth back every frame (about 12 ms on a full-screen hack, an
  estimate). It is the one deliberate exception to "no per-hack flag".
- **Memory.** The bands and the store are allocated when a GL context opens
  and freed when it closes (`glshim_open`, and the release hook).
- **Inner loops.** The band rasteriser includes the two-pixel additive blend
  from issue #73 (a saturating RGB565 add with masks, two pixels per 32-bit
  word) and smooth lines that write to the band.

### Tests

- Banded frames compared with direct-drawn frames of the same scene on the
  host, within a small tolerance. The tolerance must be tight enough that a
  band seam, a missing band or a lost primitive fails.
- Exact tests for each fallback case: no clear, colour-only clear, store
  overflow.
- Tests for each state a stored primitive carries (blend, offset, depth test
  off, smooth lines), since each changes on the fly in Unknown Pleasures.
- Re-pin the frame hashes in `test_hacks.c` once the banded output is right,
  then check every GL hack on the board.

## Calls made by Claude, not Rod

- A frame counts as cleared if it clears colour and depth, or clears colour
  with the depth test off (HexTrail).
- On overflow the rest of the frame falls back; flushing part-way is allowed.
- Band memory is allocated only while a GL context is open.
- The #73 two-pixel additive blend goes into the band rasteriser.
- Half resolution is offered for Hypnowheel only.

## Left to the builder

- Band height, from what fits in the internal RAM left over (probably 32 to
  64 rows), and whether bands are rows of the whole canvas or of the dirty box.
- The store's layout and size cap, and whether it lives in internal RAM or
  PSRAM (written once and read once per band, so sequential).
- How primitives are binned to bands (a list per band, or a sorted pass).
- How the rasterisers are limited to a band's rows: patching `ztriangle.h`'s
  row loop through `tinygl_patch.py`, or a `ZBuffer` whose `pbuf` and `zbuf`
  point at the band with a y offset.
- Whether points get their own fast path.
- Which calls besides the swap force the held frame out (`glFinish`,
  `glDrawPixels`, text drawing, a context change).

## Risks

- The estimate is rough. If rasterising in internal RAM costs a quarter of what
  it costs in PSRAM, Unknown Pleasures goes from 174 ms to roughly 80 ms and
  Hypnowheel, after its trig fix, to roughly 150 ms. Neither is measured.
- Triangle setup is repeated for each band a triangle touches. Hypnowheel's
  large triangles span many bands, and setup was already 61 ms.
- Unknown Pleasures switches between filled quads and lines with an offset
  within a frame, so a state-capture bug would show there first.
