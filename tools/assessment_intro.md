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
  count of Xlib calls cannot see. The effort rating does not run anything,
  so treat it as a prioritisation aid, not an estimate. It also says nothing
  about speed: see "Speed" below.
- The **All hacks** table lists the work still to do first, from least to
  most effort (S, M, L, XL) and then by name, and the ported hacks last.
  Its **Ported** column shows ✅ for a hack in the
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
| Ported | Already running on the device, so no rating | {n_ported} |

## Speed

Effort says what a hack takes to compile, not how fast it runs. Braid was
rated S and runs at 3-9 fps, because it draws about 7,500 wide lines a frame,
which nothing in its source says. So each unported S hack is also built with
the shim and run on the host for 300 steps (`tools/probe_hacks.py`), and the
**Speed** column gives its band and host milliseconds per step. A hack rated
M or higher has gaps, which are compile errors, so it cannot be run until the
shim fills them and its Speed is a dash.

- **low**: under 0.015 ms.
- **medium**: 0.015 to 0.05 ms.
- **high**: 0.05 ms or more.
- **does not link**, **does not compile**, **crashed on the host**, **timed
  out**: an S hack that could not be probed.

A ported hack shows the step measured on the device instead.

The host time ranks the device step well (a Spearman correlation of about 0.9
over the 29 hacks the bands were fitted to, and all seven of those with a
device step of 50 ms or more are in the high band) but it is not a prediction
in milliseconds: the device took 60 to 1,500 times as long. The ratio is
highest for hacks that still do software double-precision maths, so a low band
does not clear a hack that does a lot of `double` arithmetic. Those figures
are in-sample. Four hacks ported afterwards, with their predictions committed
first, all fell where predicted: two low-band hacks under 1 ms, a high-band
hack at 8 ms and a heavy one at 830 ms
([speed-predictions.md](speed-predictions.md)). See
[speed-backtest.md](speed-backtest.md) for the table and its limits.

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
