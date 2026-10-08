# Porting assessment

Rough effort to port each xscreensaver hack to the StopWatch X11 shim,
generated from xscreensaver {version} by `tools/score_hacks.py`. Regenerate
it with `uv run tools/score_hacks.py`.

## How to read this

- **{total} hacks** were scanned: every `hacks/*.c` and `hacks/glx/*.c`
  that registers itself with `XSCREENSAVER_MODULE`. Another {excluded}
  files (3D models, helper libraries, command-line tools) are excluded and
  listed at the end.
- The scan is static and heuristic. For each 2D hack it counts the Xlib
  calls in the source that the shim does not declare, and it also
  syntax-checks the unmodified source against the shim's headers
  (`cc -fsyntax-only`). Anything the compiler cannot find is a **shim
  gap**: a missing header, type, struct field, constant or function.
  That second check catches helpers hacks reach through `utils/` that a
  count of Xlib calls cannot see. It does not run anything, so treat the
  effort ratings as a prioritisation aid, not an estimate.
- The **All hacks** table is sorted from least to most effort (S, M, L,
  XL), then by name. Its **Ported** column shows ✅ for a hack in the
  firmware's registry (`hacks/registry.c`) and ❌ for one that was
  attempted and abandoned (see "Failed ports" after the table, which only
  appears when there are any). Failures are recorded in
  `tools/failed_ports.txt`.

## Effort ratings

| Rating | Meaning | Count |
| --- | --- | --- |
| S | 2D, and the unmodified source compiles against the shim | {n_s} |
| M | 2D, 1-4 shim gaps, no pixmaps or pixel read-back | {n_m} |
| L | 2D, 5+ shim gaps, or uses pixmaps or pixel read-back | {n_l} |
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
