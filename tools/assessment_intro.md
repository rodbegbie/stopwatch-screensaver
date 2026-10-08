# Porting assessment

Rough effort to port each xscreensaver hack to the StopWatch X11 shim,
generated from xscreensaver {version} by `tools/score_hacks.py`. Regenerate
it with `uv run tools/score_hacks.py`.

## How to read this

- **{total} hacks** were scanned: every `hacks/*.c` and `hacks/glx/*.c`.
- The scan is static and heuristic. It counts Xlib calls in the source and
  compares them with the calls declared in `firmware/src/x11shim/xshim.h`.
  It does not run anything, and it does not see helpers that hacks reach
  through `xlockmore.h` or `utils/`, so treat the effort ratings as a
  prioritisation aid, not an estimate.
- Many files in `hacks/` are shared helpers or support code rather than
  hacks, so the totals overstate the number of distinct screensavers.

## Effort ratings

| Rating | Meaning | Count |
| --- | --- | --- |
| S | 2D, and every Xlib call is already in the shim | {n_s} |
| M | 2D, 1-4 missing calls, no pixmaps or pixel read-back | {n_m} |
| L | 2D, 5+ missing calls, or uses pixmaps or pixel read-back | {n_l} |
| XL | GL: needs a software rasteriser (see below) | {n_xl} |

## Flags

- `pixmaps`: uses `XCreatePixmap` or `XCopyArea`. Needs off-screen
  surfaces, which cost PSRAM (466×466 at 16 bits is about 434 KB each).
- `readback`: uses `XGetImage` or `XGetPixel`. Needs pixel read-back, and
  is often slow.
- `xor`: uses `XSetFunction`, so it needs XOR drawing.
- `text`: draws text, so it needs a font path.
- `clipmask`: uses clip masks.
- `float-heavy`: 20 or more `sin`, `cos`, `sqrt` or `pow` calls. The
  ESP32-S3 has a single-precision FPU only, so double-precision maths is
  slow in software. Expect these to need profiling.
- `needs-xlockmore`: built on the `xlockmore.h` framework, which the shim
  does not provide yet.

## GL hacks

The XL hacks use fixed-function OpenGL. A port would need a software
rasteriser (TinyGL has been ported to the ESP32) at reduced resolution. No
performance numbers exist yet, so this stays a separate future project.
