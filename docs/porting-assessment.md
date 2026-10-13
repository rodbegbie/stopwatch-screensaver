# Porting assessment

Rough effort to port each xscreensaver hack to the StopWatch X11 shim,
generated from xscreensaver 6.16 by `tools/score_hacks.py`. Regenerate
it with `uv run tools/score_hacks.py`.

## How to read this

- **283 hacks** were scanned: every `hacks/*.c` and `hacks/glx/*.c`
  that registers itself with `XSCREENSAVER_MODULE`. Another 125
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
| S | 2D, and the unmodified source compiles against the shim | 9 |
| M | 2D, 1-4 shim gaps, no pixmaps or pixel read-back | 7 |
| L | 2D, 5+ shim gaps, or uses pixmaps or pixel read-back | 44 |
| XL | GL: runs on the TinyGL layer, each hack needs its own GL calls and helpers checked (see below) | 132 |
| Ported | Already running on the device, so no rating | 91 |

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

About 40 hacks are built on the `xlockmore.h` framework. The shim provides it
(`firmware/src/xs_support/xlockmore.c`), so it is not a flag: it costs nothing
to port.

## GL hacks

The XL hacks use fixed-function OpenGL. They run on the GL layer in
`firmware/src/glshim/`, a thin shim over TinyGL (a software rasteriser) that
draws straight into the canvas. Gears is the first GL hack ported; the
speed and ideas are in issues #43 to #46. An unported XL hack still needs its
own checking: the GL calls it makes may not all be in TinyGL, and the shim
does not yet provide every GL helper under `xs_support/glx/`.

## Measured on the device

Eighty-one hacks have been run so far (default settings, 466×466 canvas
pushed to the display every frame, canvas held in PSRAM). The firmware times
each frame in three parts, averaged over 5 seconds: **step** is the hack's own
draw call, **push** is sending the canvas to the display, and **wait** is what
is left of the delay the hack asked for once the push is credited against it
(the runner caps any delay at 10 seconds). Rows other than Rorschach and Pedal
were measured before the cap was raised from 1 second; Helix also asks for
5-second holds, so its frame rate will now be lower than shown.

| Hack | fps | step | push | wait | Extra PSRAM |
| --- | --- | --- | --- | --- | --- |
| Pyro | 30.0 | 0.8-1.1 ms | 31.2 ms | 0 ms | about 80 KB |
| HyperCube | 28.3 | 2.7-2.9 ms | 31.5 ms | 0 ms | none measurable |
| Petri | 28.8-30.8 | 0.3-2.5 ms | 31.1 ms | 0 ms | about 1.3 MB |
| XSpirograph | 11.0-11.4 | 54-56 ms | 31.4 ms | 0-20 ms | none measurable |
| Helix | 16.6-23.6 | 0.9-2.1 ms | 31.3 ms | 9-26 ms | none measurable |
| Rorschach | 3.0 | 0.6-1.7 ms | 31.1-31.2 ms | 367 ms | none measurable |
| Pedal | 0.2-0.4 | 126-519 ms | 31.4-31.5 ms | 3290-5507 ms | none measurable |
| Coral | 15.0-21.2 | 15-25 ms | 31.2 ms | 0-10 ms | about 240 KB |
| Squiral | 30.6-31.0 | 0.1-0.3 ms | 31.1 ms | 0 ms | about 850 KB |
| Critical | 30.4 | 0.7 ms | 31.1 ms | 0 ms | about 15 KB |
| CloudLife | 24.2 | 8.8-8.9 ms | 31.4 ms | 0 ms | about 260 KB |
| WhirlWindWarp | 17.0-24.8 | 6.2-26.4 ms | 31.3 ms | 0 ms | about 410 KB |
| Flame | 4.4-6.8 | 26-107 ms | 31.3-31.4 ms | 85-187 ms | none measurable |
| Hopalong | 19.2-25.2 | 7.3-20.1 ms | 31.2-31.3 ms | 0 ms | about 8 KB |
| Vines | 2.8-3.0 | 133-160 ms | 31.2 ms | 175-187 ms | none measurable |
| Sierpinski | 2.4-2.6 | 1.8-4.0 ms | 31.3-31.4 ms | 377-409 ms | about 32 KB |
| FadePlot | 28.0-28.2 | 2.9-3.2 ms | 31.3-31.4 ms | 0 ms | about 16 KB |
| Thornbird | 29.0-29.2 | 2.0-2.2 ms | 31.1-31.2 ms | 0 ms | about 11 KB |
| Spiral | 18.8 | 0.9-1.2 ms | 31.1 ms | 21 ms | about 5 KB |
| Sphere | 30.0-30.4 | 0.6-1.1 ms | 31.1-31.2 ms | 0 ms | none measurable |
| Discrete | 5.4-5.6 | 149-160 ms | 31.5 ms | 0 ms | about 16 KB |
| Galaxy | 9.4-11.2 | 58-75 ms | 31.1-31.3 ms | 0 ms | about 225-266 KB |
| Drift | 22.0-23.0 | 11-13 ms | 31.5 ms | 0 ms | about 16 KB |
| Lightning | 29.4 | 1.8-1.9 ms | 31.2 ms | 0 ms | none measurable |
| Maze | 6.0-28.8 | 0.1-1.3 ms | 31.2-33.4 ms | 0-256 ms | not measured |
| Blaster | 27.8 | 3.6-3.7 ms | 31.2-31.3 ms | 0 ms | not measured |
| Substrate | 12.4-20.6 | 2.9-43.9 ms | 41.1-44.4 ms | 0 ms | about 1.74 MB |
| Pacman | 75.6-79.2 | 1.3-1.9 ms | 0.8-1.4 ms | 10.0-10.6 ms | about 580 KB |
| Braid | 3.2-8.6 | 95-296 ms | 19.5-22.7 ms | 0 ms | none measurable |
| Mountain | 44.0-45.2 | 0-0.4 ms | 0.1-0.4 ms | 21.9-22.1 ms | about 20 KB |
| Epicycle | 22.8-44.0 | 0.3-2.1 ms | 0.3-0.8 ms | 22.0-59.5 ms | none measurable |
| Kaleidescope | 27.8-34.4 | 7.0-9.8 ms | 17.7-25.5 ms | 0-3.7 ms | none measurable |
| Gears | 2.8-10.0 | 65-330 ms | 32.3-34.5 ms | 0 ms | 0.7-1.4 MB |
| Morph3D | 8.0-11.4 | 48-86 ms | 32.6-36.2 ms | 5.2-8.8 ms | 0 (PSRAM identical every visit) |
| Morph3D (dirty rectangle, `*delay: 10000`) | 14.4-15.2 | 54-58 ms | 2.0-9.2 ms | 2.6-9.4 ms | not re-measured |
| Gears (dirty rectangle, first layout after boot) | 10.6-13.0 | 44-63 ms | 4.4-12.1 ms | 20-29 ms | not re-measured |
| Gears (dirty rectangle, `*delay: 10000`, first layout after boot) | 12.6-17.6 | 46-65 ms | 5.5-13.5 ms | 0-5.8 ms | not re-measured |
| Morph3D, five shapes (final code, overnight, 23 starts) | 11.6-31.2 | 20-75 ms | 6.3-9.1 ms | 2.6-5.2 ms | 0 (PSRAM identical every visit) |
| CubicGrid (final code, overnight, 37 windows) | 11.2-14.0 | 38-53 ms | 32.1-36.1 ms | 0 ms | 0 (PSRAM identical every visit) |
| Gears (final code, overnight, 29 windows, many layouts) | 4.0-19.8 | 39-241 ms | 6.1-23.7 ms | 0-5.2 ms | 0 (PSRAM identical; heap falls 388 B a lap, its known leak) |
| CubicGrid (ticks 30, 27,000 points; not registered) | 5.6-6.0 | 137-148 ms | 32.3-36.0 ms | 0 ms | 1.66 MB more than Morph3D (display list) |
| CubicGrid (ticks 20, 8,000 points; registered) | 11.2-12.4 | 48-52 ms | 32.2-36.1 ms | 0 ms | 0.49 MB more than Morph3D (display list) |
| Celtic (as copied; first port) | 1.0-20.4 | 29-1025 ms | 0.1-5.5 ms | 7.9-928 ms | none measurable |
| Celtic (single precision, upstream graph sizes) | 1.0-20.2 | 5.6-151 ms | 0.8-6.6 ms | 9.4-1110 ms | 16 KB in one window (heap ran out) |
| Celtic (single precision, graph sized to the screen, step scale 1) | 1.2-20.2 | 13.5-129 ms | 1.1-6.4 ms | 9.2-929 ms | none measurable |
| Celtic | 5.2 | 12.6-76.8 ms | 3.1-10.6 ms | 215-221 ms | none measurable |
| Deluxe (opaque, arcs as polylines; first port) | 2.0-5.4 | 154-510 ms | 32.6 ms | 0 ms | not compared with idle (flat at 7,327,227) |
| Deluxe (opaque, circles as rings) | 13.2-15.4 | 31.5-42.1 ms | 32.5-35.1 ms | 0 ms | not compared with idle (flat at 7,327,227) |
| Deluxe | 10.4-12.8 | 44.6-62.6 ms | 32.5-32.6 ms | 0 ms | not compared with idle (flat at 7,327,227) |
| Forest | 1.8 | 13.0-58.9 ms | 1.7-4.0 ms | 550-553 ms | none measurable |
| Laser | 15.0-15.8 | 21.6-25.1 ms | 21.5-23.6 ms | 18.3-20.8 ms | none measurable |
| Lissie | 85.6 | 0.3-0.4 ms | 0.4 ms | 10.9 ms | none measurable |
| Rotor | 84.8 | 0.5 ms | 0.5 ms | 10.8 ms | none measurable |
| Scooter | 17.8-19.2 | 19.0-23.1 ms | 32.3 ms | 0 ms | about 162 KB |
| Grav | 75.6-77.8 | 1.5-1.9 ms | 0.7-0.8 ms | 10.5-10.7 ms | none measurable |
| Euler2D | 17.8 | 27.9 ms | 27.6 ms | 0 ms | about 71 KB |
| Anemone | 12.6-17.0 | 25.5-46.3 ms | 32.4 ms | 0 ms | about 98 KB |
| Anemotaxis | 17.8-18.8 | 19.8-22.5 ms | 32.5 ms | 0 ms | none measurable |
| Compass | 15.4-15.6 | 30.5-31.5 ms | 32.5 ms | 0 ms | none measurable |
| FuzzyFlakes | 15.6 | 30.2-30.9 ms | 32.5 ms | 0 ms | none measurable |
| IFS | 22.2-24.4 | 19.0-23.2 ms | 2.5-5.9 ms | 16.2-19.7 ms | about 35 KB |
| Interaggregate | 13.4-15.0 | 41.8-47.4 ms | 24.3-28.9 ms | 0 ms | about 853 KB |
| LMorph | 14.8-17.2 | 24.7-34.3 ms | 32.5 ms | 0 ms | none measurable |
| Wormhole | 17.6-17.8 | 22.4-23.1 ms | 32.4 ms | 0 ms | about 32 KB |
| Halftone | 4.5-5.2 | 163-193 ms | 32.5 ms | 0 ms | none measurable |
| Truchet | 0.8-1.0 | 652-900 ms | 34.4-36.1 ms | 404-406 ms | about 1.5 MB |
| Truchet (scrolling mode) | 7.2-8.0 | 19.8-36.0 ms | 31.5-39.9 ms | 66-75 ms | about 1.5 MB |
| UnknownPleasures | 5.0-5.2 | 162-169 ms | 32.3-37.1 ms | 0 ms | about 0.93 MB |
| UnknownPleasures (first flash: aliased lines, additive quads) | 3.6-3.8 | 226-237 ms | 32.3-48.3 ms | 0 ms | about 0.93 MB |
| UnknownPleasures (20 rows of 50 points, final code; not registered) | 7.4-7.6 | 90-92 ms | 41.6-43.0 ms | 0 ms | 6.27 MB free, with the QR overlay's buffers |
| Hexadrop (as copied, double) | 2.2-2.6 | 377-442 ms | 32.5 ms | 0 ms | none measurable |
| Hexadrop (single precision) | 3.0-3.2 | 282-313 ms | 32.5-35.8 ms | 0 ms | none measurable |
| Hexadrop (float convex fill) | 9.0-9.2 | 76-79 ms | 32.5 ms | 0 ms | none measurable |
| Hexadrop (registered: two-pixel stores) | 10.2 | 64.3-65.6 ms | 32.5 ms | 0 ms | none measurable |
| StonerView | 36.2-41.6 | 12.6-15.6 ms | 6.5-9.6 ms | 1.9-4.8 ms | about 0.63 MB |
| Halo (Seuss mode, registered) | 22.0-25.6 | 16.6-23.0 ms | 15.9-16.5 ms | 5.5-5.6 ms | about 40 KB |
| Halo (Ramp mode, registered) | 16.6-40.8 | 13.8-43.4 ms | 8.2-17.6 ms | 0.8-1.3 ms | about 12 KB |
| Halo (Ramp mode, 100 ms delay, colour pixmap) | 4.6-15.4 | 35.7-110 ms | 7.9-31.9 ms | 17.6-80.9 ms | about 0.42 MB |
| Halo (Seuss mode, colour pixmaps) | 5.2-10.2 | 77-172 ms | 15.4-18.9 ms | 4.3-5.8 ms | about 0.85 MB |
| XMatrix (typed trace, then rain) | 17.6-87.2 | 0.1-23.2 ms | 3.4-32.5 ms | 0-8.2 ms | about 0.47 MB |
| Qix | 31.4-48.6 | 8.8-17.9 ms | 8.9-12.8 ms | 0-2.4 ms | about 5 KB |
| Fireworkx (as copied, double) | 2.0-2.6 | 367-587 ms | 32.4-36.0 ms | 0 ms | about 3.1 MB |
| Fireworkx (single precision, float frand) | 2.8 | 327-348 ms | 32.4 ms | 0 ms | about 3.1 MB |
| Fireworkx (scale 2) | 7.6-7.8 | 96-99 ms | 32.5 ms | 0 ms | about 0.8 MB |
| Fireworkx (scale 3, upstream's constants) | 10.6-11.2 | 56-62 ms | 32.5-32.6 ms | 0 ms | about 0.35 MB |
| Fireworkx (scale 3, lengths scaled, 2 shells of 120 sparks, registered) | 12.4-12.6 | 45.8-47.1 ms | 32.5 ms | 0 ms | about 0.33 MB |
| Fireworkx (scale 4) | 11.8-12.6 | 46-51 ms | 32.6 ms | 0 ms | about 0.2 MB |
| Starfish (as copied) | 8.2-9.2 | 76-87 ms | 32.6-35.5 ms | 0 ms | none measurable |
| Starfish (registered: float polygon fill) | 19.0-19.2 | 18.6-19.0 ms | 32.5 ms | 0 ms | none measurable |
| Goop (as copied, opaque layers) | 7.0-7.4 | 103-109 ms | 32.4 ms | 0 ms | about 0.75 MB |
| Goop (opaque layers, `GOOP_ALPHA=32`) | 14.4-14.6 | 35.1-35.6 ms | 32.5-32.6 ms | 0 ms | about 0.3 MB |
| Goop (translucent layers, full clear) | 11.0-11.4 | 54-57 ms | 32.5 ms | 0 ms | about 0.3 MB |
| Goop (translucent, packed blend, partial erase, registered) | 14.6-15.2 | 44.7-46.9 ms | 19.8-21.3 ms | 0 ms | about 0.3 MB |
| Loop (as copied, 100 ms delay) | 8.6-9.0 | 3.5-6.9 ms | 1.1-5.1 ms | 101-110 ms | about 1 MB |
| Loop (registered, 50 ms delay) | 15.4-16.8 | 4.5-10.4 ms | 1.5-3.7 ms | 52-54 ms | about 1 MB |
| Hypnowheel | 1.4 | 720 ms | 32.6 ms | 0 ms | about 0.64 MB |
| Hypnowheel (`*layers: 1`, probe) | 4.4 | 200 ms | 32.5 ms | 0 ms | about 0.64 MB |
| Hypnowheel (`*count: 5`, probe) | 2.0 | 511 ms | 32.6 ms | 0 ms | about 0.64 MB |
| Hypnowheel (single precision, saturating blend, probe) | 2.2 | 426 ms | 32.6 ms | 0 ms | not measured |
| Hypnowheel (as above, half resolution, probe) | 4.0 | 227 ms | 32.5 ms | 0 ms | not measured |
| CWaves (as copied, double) | 7.8 | 95-97 ms | 32.6 ms | 0 ms | about 7 KB |
| CWaves (single precision) | 10.6 | 61 ms | 32.4 ms | 0 ms | about 7 KB |
| CWaves | 20.0-20.2 | 16.1-16.2 ms | 32.5 ms | 0 ms | about 7 KB |
| Vermiculate | 74-79 | 1.3-2.2 ms | 1.8-2.9 ms | 8.4-9.5 ms | about 0.46 MB |
| Cynosure | 1.8 | 17.0-17.8 ms | 27.5-28.5 ms | 523-524 ms | none measurable |
| Moire (as copied, double) | 4.8 | 40.6-44.4 ms | 1.3 ms | 283.6 ms | none measurable |
| Moire | 4.8 | 9.5-9.7 ms | 1.3 ms | 283.7 ms | none measurable |
| Moire2 (first plane code) | 9.6 | 53.7-54.0 ms | 32.5 ms | 19.6 ms | about 0.25 MB |
| Moire2 (plane copy and expansion in 32 bits) | 12.6 | 28.0-28.3 ms | 32.4 ms | 19.8 ms | about 0.25 MB |
| Moire2 | 16.2 | 28.1-28.3 ms | 32.5 ms | 0 ms | about 0.25 MB |
| Moire2 (no expansion, probe) | 12.2 | 26.9 ms | 0.1 ms | 55.4 ms | about 0.25 MB |
| Moire2 (no expansion or plane copy, probe) | 18.0 | 0.5 ms | 0.1 ms | 55.5 ms | about 0.25 MB |
| Boing (with scanlines) | 5.0 | 169-172 ms | 32.4 ms | 0 ms | about 0.64 MB |
| Boing | 8.2-8.4 | 87.1-89.0 ms | 32.4 ms | 0 ms | about 0.64 MB |
| Boing (no scanlines, `*ballSize: 0.3`, probe) | 9.2-9.4 | 73.9-75.2 ms | 32.6 ms | 0 ms | about 0.64 MB |
| HexTrail (`*count: 20`, upstream's) | 3.2-15.0, mean 5.7 | 34-293 ms | 5.0-32.2 ms | 0-28 ms | about 0.86 MB |
| HexTrail (`*count: 10`) | 6.4-31.2, mean 9.1 | 19-122 ms | 10.1-32.4 ms | 0-3 ms | about 0.70 MB |
| HexTrail (count 6, double) | 9.2-36.8, mean 12.7 | 13-76 ms | 11.1-32.5 ms | 0-3 ms | about 0.66 MB |
| HexTrail (count 6, single precision) | 9.6-38.2, mean 13.0 | 12-71 ms | 11.5-32.5 ms | 0-2 ms | about 0.66 MB |
| HexTrail | 10.8-50.6, mean 15.3 | 8-59 ms | 6.9-32.5 ms | 0-5 ms | about 0.66 MB |
| Eruption (as copied) | 9.6-9.8 | 72.5-72.6 ms | 29.0 ms | 0 ms | about 0.70 MB |
| Eruption (half size, `lowrez_put.h` before its int loop) | 13.6-13.8 | 42.6 ms | 29.2 ms | 0 ms | about 0.21 MB |
| Eruption | 14.6 | 39.0-39.1 ms | 29.0 ms | 0 ms | about 0.21 MB |
| Eruption (`-DERUPTION_SCALE=3`) | 18.0 | 25.5 ms | 29.0 ms | 0 ms | about 0.12 MB |
| Eruption (half size, no put, probe) | 40.8 | 13.4 ms | 0.1 ms | 11.0 ms | about 0.21 MB |
| Kumppa | 13.4 | 44.5 ms | 29.3 ms | 0 ms | none measurable |
| ShadeBobs | 73.6-75.4 | 2.0-2.3 ms | 0.7-0.8 ms | 10.5 ms | about 0.43 MB |
| Piecewise (double) | 9.8-10.4 | 66.0-71.0 ms | 29.3 ms | 0 ms | none measurable |
| Piecewise (double, no clear, probe) | 12.4-13.2 | 51.2-56.2 ms | 23.2-24.6 ms | 0 ms | none measurable |
| Piecewise | 13.0-14.0 | 41.2-44.0 ms | 29.3 ms | 0 ms | none measurable |
| BoxFit | 10.0-25.4 | 9.1-17.0 ms | 5.1-9.2 ms | 24-81 ms | about 37 KB |
| Interference | 10.2 | 67.9-68.0 ms | 29.0 ms | 2.3 ms | about 0.44 MB |
| BinaryRing (as copied, 24-bit path) | 2.8-3.2 | 284-335 ms | 29.1 ms | 0 ms | about 2.3 MB |
| BinaryRing (single precision) | 3.6-4.2 | 210-254 ms | 29.1 ms | 0 ms | about 2.3 MB |
| BinaryRing (single precision, no image put or copy, probe) | 7.6-15.8 | 52.5-121.4 ms | 0.1-0.2 ms | 11.1 ms | about 2.3 MB |
| BinaryRing | 7.2-13.8, 13.6 after 20 s | 44.9-113.3 ms | 13.4-27.9 ms | 0-1.5 ms | about 2.3 MB |
| GraphStat | 10.2-14.6 | 35.0-67.8 ms | 29.1-29.3 ms | 2.1-2.3 ms | about 0.67 MB |
| EtruscanVenus (64 by 48, full size) | 4.0-7.8 | 100-229 ms | 13.8-29.2 ms | 0-13.4 ms | about 0.93 MB |
| EtruscanVenus (64 by 48, half size) | 6.4-9.6 | 78-127 ms | 13.6-29.2 ms | 0-13.5 ms | about 0.71 MB |
| EtruscanVenus | 9.2-12.6 | 53.2-78.6 ms | 15.6-29.2 ms | 0-10.8 ms | about 0.64 MB |
| BlockTube (as copied, 1,000 blocks, full size) | 3.2-3.4 | 264-285 ms | 29.0 ms | 8.8-12.5 ms | about 1.13 MB |
| BlockTube (1,000 blocks, half size) | 5.0 | 161.6-164.0 ms | 29.2 ms | 12.2 ms | about 0.92 MB |
| BlockTube (500 blocks, half size, single precision, `*delay: 40000`) | 7.0-7.4 | 93.9-102.5 ms | 26.1-29.2 ms | 12.2-15.9 ms | about 0.87 MB |
| BlockTube (500 blocks) | 8.0-9.0 | 83.9-94.0 ms | 26.4-32.8 ms | 0 ms | about 0.87 MB |
| BlockTube | 8.8-10.0 | 71.6-80.4 ms | 28.0-33.0 ms | 0 ms | about 0.86 MB |
| Fireworkx (after `lowrez_put.h`'s int loop) | 13.8-14.0 | 41.7-44.3 ms | 29.0 ms | 0 ms | about 0.34 MB |
| Abstractile | 20.6-32.2 | 2.4-11.1 ms | 5.3-11.2 ms | 21.2-43.5 ms | about 6.1 MB |
| Attraction | 59.0-59.2 | 5.6 ms | 3.2-3.7 ms | 7.6-8.1 ms | none measurable |
| Greynetic | 75.0-76.4 | 1.8-2.0 ms | 4.9-5.2 ms | 6.1-6.3 ms | none measurable |
| Wander | 37.6 | 4.4 ms | 0.4-0.6 ms | 21.8-21.9 ms | none measurable |
| PopSquares | 22.6 | 15.3 ms | 27.9 ms | 0.0 ms | none measurable |
| Lisa (as copied) | 9.6 | 87.5 ms | 12.6-13.3 ms | 5.7 ms | none measurable |
| Lisa | 14.2-14.4 | 51.9-52.5 ms | 12.7-14.2 ms | 4.2-5.7 ms | none measurable |
| Penrose | 77.4-89.6 | 0.0-0.9 ms | 0.1-1.0 ms | 10.9-11.7 ms | none measurable |
| Worm | 51.6 | 0.3 ms | 1.3-1.4 ms | 17.7 ms | about 164 KB |
| Triangle | 57.4-67.0 | 3.6-6.0 ms | 0.9-2.1 ms | 9.3-10.4 ms | about 145 KB |
| XRaySwarm | 22.8-23.0 | 21.2-21.6 ms | 0.9-2.8 ms | 19.4-21.6 ms | about 59 KB |
| Rocks | 17.2-17.4 | 2.5-3.2 ms | 3.5-4.7 ms | 50.7-52.0 ms | about 2.66 MB |
| DecayScreen | 63.6-66.8 | 3.3-3.9 ms | 4.3-5.0 ms | 6.7-7.3 ms | none measurable |
| XJack | 7.8-15.8 | 0.1-13.8 ms | 0.3-15.0 ms | 46-130 ms | none measurable |

XJack (2026-10-12) is paced by its own delay, so its fps says little about the
board: typing one character a frame takes 0.1 ms and the rest is `wait`. Read
the row in two phases. Typing (the first 25 s, windows 2 to 5): 7.8-9.2 fps,
step 0.1 ms, push 0.3 ms (10 rows a frame), wait 109-130 ms. Scrolling (after
the page fills, windows 6 to 70): 8.8-15.8 fps, mean 11.6, step 3.9-13.8 ms
(each of a line's eight sub-steps copies the window up 3 pixels),
push 4.3-15.0 ms (74-241 rows a frame, mean 150), wait 46-104 ms. The host
step is 0.003 ms, so the board ran roughly 30 times the host while typing; the
speed backtest cannot take one number for it. Read from a pinned build at 20
px with rotation off, for 350 s (the first window dropped), with Rod's go-ahead
to flash. Free internal heap fell 352 bytes in three steps (303,600 at boot,
303,568, 303,336, then 303,248 from about 225 s) and PSRAM did not move
(6,989,667 throughout); a 6-minute run cannot say whether that drift is a
leak. No stack canary, panic or reboot appeared. Rod looked at the panel and
said it looks good at 20 px.

The rows from Abstractile to XRaySwarm are the batch of 2026-10-12 (Abstractile,
Attraction, Greynetic, Wander, PopSquares, Lisa, Penrose, Worm, Triangle and
XRaySwarm), each from a pinned build with rotation off, read for 45 s (the
first window dropped) with Rod's go-ahead to flash. "Extra PSRAM" is the drop
from 6,989,667 bytes; good to about 10 KB. Abstractile holds about 6.1 MB
while it runs, leaving under 1 MB of PSRAM. Lisa is registered in single
precision (`lisa_single.c`): 9.6 fps as copied, 14.3 after. Rocks and
DecayScreen were added in a second pass, once the shim filled bitmaps and
painted a picture for a screen grab; Rocks holds 2.66 MB of PSRAM and leaves
about 32 KB of internal heap while it runs, and gives both back when it stops.

The seven rows from CWaves to HexTrail are the CWaves, Vermiculate, Cynosure,
Moire, Moire2, Boing and HexTrail batch (2026-10-11), each from a pinned build
read for 30 to 150 s with Rod's go-ahead to flash while he was away. There is
no idle PSRAM reading for this build (the 7,327,227 above predates the QR
overlay), so "Extra PSRAM" is the drop from 6,989,667 bytes, which Cynosure
and Moire read and which allocate almost nothing; treat it as good to about
10 KB. Free heap was flat in every capture and no stack canary, panic or
reboot appeared.

The rows from Eruption to BlockTube are the batch of 2026-10-12 (Eruption,
Kumppa, ShadeBobs, Piecewise, BoxFit, Interference, BinaryRing, GraphStat,
EtruscanVenus and BlockTube), each from a pinned build with rotation off, read
for 20 to 60 s with Rod's go-ahead to flash while he was away. "Extra PSRAM"
is the drop from 6,989,667 bytes, which Kumppa and Piecewise read; good to
about 10 KB. A full-frame write to the PSRAM canvas costs about 25 ms here:
Eruption at half size spent 26 ms of its 39 ms step putting the image (each
pixel written as a 2 by 2 square), against 13 ms for the fire itself.
BinaryRing's 13.6 fps comes after about 20 s: while the ring is still on the
screen the step is 98-113 ms. Interference changes every pixel each frame, so
it pays a full put and a full push. GraphStat's step is TinyGL drawing its
grid as thin full-height quads (fill 35 ms for about 3,400 triangles) and a
10 ms clear. EtruscanVenus's step depends on its mode: the ones that fill the
screen are fill-bound, and the rest is its own surface maths and about 16,000
GL calls a frame. Its first two rows were read with `-DGL_PROFILE`, as were the
first three BlockTube rows. Rod dropped BlockTube after seeing it (too slow,
and not much going on), so its rows are the measured record of a hack that
is not in the rotation. Fireworkx shares `lowrez_put.h`, so its row is
after this batch's change to the helper (12.4-12.6 fps before, with
pixel-identical frames). A soak of the whole registry at `-DROTATE_SECONDS=5` (five
laps, 408 changes) had no reset, panic or stack canary; each new hack read the
same free PSRAM on every visit, and free heap fell by the same 40 to 570 bytes
a lap for every hack, all of it between Gears and Morph3D (Gears' known
upstream leak).

CWaves' cosines were 4 ms of a 61 ms step in single precision; the rest was
233 two-pixel-wide column fills, each store on a different cache line of the
PSRAM canvas. The picture depends on x only, so the wrapper writes the columns
into row 0 and copies that row down (frames identical to the unmodified hack).
Moire is paced by its own delays, 50 ms between 20-row chunks and 5 s between
pictures, so its fps says nothing about speed: the step fell from 44 to 9.5 ms
a chunk. Cynosure waits 500 ms between frames by design (upstream's
default), as Forest does.

Moire2's first plane code took 54 ms. Skipping one call at a time showed the
plane copies at 26 ms and the expansion at 27 ms, both slowed by 64-bit index
maths on a 32-bit core and, for the expansion, a branch a pixel. Working in
32 bits and a byte at a time took the step to 28 ms, and overriding the hack's
50 ms delay (which waited out all but the 28 ms) took it to 16.2 fps. The
occasional frame that builds new planes takes 96-141 ms.

Boing's scanlines, 208 blended full-width lines, cost about 83 ms of the 171 ms
step; a smaller ball (0.3 for 0.5) saves 13 ms more, so the rest is fixed
(the clear, the wall and the shadow). HexTrail draws every hexagon built so
far, so its step climbs to a ceiling and it restarts; the means above are over
150 s, which covers several cycles. Single precision alone moved HexTrail from
12.7 to 13.0 mean (the worst step from 76 to 71 ms): its time is in the clears,
the push and TinyGL, not in the hack's own maths. HexTrail never enables the
depth test but clears the depth buffer every frame, a second full-canvas clear
of PSRAM; skipping it while the test is off (done when the test is enabled
instead) took the mean to 15.3 and the worst step to 59 ms. Both GL hacks were
below the 15 fps bar in their first registered form (8.3 and 12.7). Rod dropped
Boing after seeing it (too slow, and plain on a small screen), so its rows are
the measured record of a hack that is not in the rotation.

Unknown Pleasures' rows are from pinned builds (2026-10-10). The first flash
had TinyGL's one-pixel lines and its additive blend, so the black quads that
should hide the rows behind a peak hid nothing; the registered row is after
smooth lines, opaque fills, polygon offset and the refresh-synced push (see the
GL notes and `.claude/rules/device-display.md`). The push figures here were
read before the log had a `sync=` field, so they include the wait for the
panel's next TE pulse: 32.3 ms of push and about 4 ms of waiting. `push=` now
leaves that wait out.
A tap on the overlay adds about 16 ms to the push (48 ms). The registered
row's 0.93 MB is from before the QR overlay merged; the same code rebased onto
it was read again at 5.0 fps, a step of 169 ms and a push of 35.7 to 37.6 ms,
with 6,058,235 bytes of PSRAM free (the QR buffers included, so not comparable
with the idle figure). The 20 by 50
variant (`*count: 40`, `*resolution: 50`) is the final code on a later main,
which holds a QR buffer, so its free PSRAM is not comparable with the others.
Its step is 45 percent below the registered build's (91 ms against 165); on
the first-flash code the same smaller scene cut the step by 34 percent (234 to
154 ms). Heap was flat in every capture, and no reset or panic appeared.

Truchet's rows are from a pinned build (2026-10-10). Each start picks one of
twelve modes at random. The tiling row is two windows from one start (650 to
900 ms per tiling, then its 400 ms pause), and the scrolling row is fourteen
windows from another (each step is a 466 by 466 copy from its pixmap, then a
100 ms pause). Its 866 by 866 pixmap lowers free PSRAM by about 1.5 MB, and
the reading is the same on every visit of a `ROTATE_SECONDS=5` capture of
three and a half laps.

Each batch row is the registered build: Euler2D with 512 particles, Anemone
with 64 arms, FuzzyFlakes at density 3, LMorph without its pause, and
Compass, Interaggregate and the first four in single precision. Halftone's row
is single precision and not registered.
The sixteen rows from Forest to Halftone are the S-hack batch, read on one
lap of the final build (30 s a hack, the first five-second window of each
dropped, four to five windows read; Halftone's row is from the earlier
20 s laps). Free heap was flat across each hack's windows (Anemotaxis fell
636 bytes, Forest 232 and Scooter 88 over its visit), and no stack canary,
panic or reboot appeared in any capture. "Extra PSRAM" is the drop from the
idle 7,327,227 bytes. How each slow hack improved, on 20 s laps:

| Hack | As copied (double) | Single precision | Plus float literals | Plus lighter setting |
| --- | --- | --- | --- | --- |
| Euler2D | 2.6 fps, step 384 ms | 11.8 fps, 57 ms | 12.4 fps, 53 ms | 17.8 fps, 28 ms (count 512) |
| Anemone | 7.5 fps, step 95 ms | 11.3 fps, 49 ms | 11.3 fps, 49 ms | 14.9 fps (64 arms) |
| Interaggregate | 3.6 fps, step 259 ms | 15.2 fps, 41 ms | 14.0-16.3 fps | none |
| LMorph | 9.3 fps, step 35 ms | 9.9 fps, 27 ms | 10.2 fps, 24 ms | 15.4 fps (no 70 ms pause) |
| FuzzyFlakes | 12.1 fps, step 50 ms | 13.5 fps, 41 ms | 13.6 fps, 40 ms | 15.6 fps (density 3) |
| Compass | 14.9 fps, step 34 ms | 15.3 fps, 32 ms | 15.5 fps, 31 ms | none |
| Halftone | 2.5 fps, step 381 ms | 4.8 fps, 178 ms | 5.2 fps, 163 ms | none |

Anemone's step moves between 26 and 46 ms as its arms grow, and Interaggregate's
frame rate drifts as the picture fills, so both sit on the 15 fps line.
Halftone with `XFillArc` returning at once measured 15.4 fps (step 31.8 ms), so
its dots' ellipse fills are about 80% of its step (issue #57). Forest waits
500 ms between trees by design.

Interaggregate's alpha blend was fixed after the batch lap (the wrapper now
swaps pixels at the hack's boundary, as Substrate's does). Its row is from a
45 s pinned run after the fix (13.4-15.0 fps, heap flat), and Rod checked on
the device that it draws earth tones, not a rainbow.

Rod looked at all fifteen registered hacks of the batch on the device on
2026-10-10 and found them performant and in line with the same hacks on other
platforms (Anemotaxis was missing from his written list; he confirmed after
that he had looked at it). All fifteen stay in the rotation, Forest included.
Halftone stays out (issue #57). Truchet, the batch's last S hack, was ported
later, once primitives could draw into pixmaps (issues #58 and #61).

Maze's row is 26 five-second readings over 160 seconds, taken on a build that
includes the overlay stamping. Its steps are cheap, and its frame rate is set
by the delays it asks for between phases (a 10 ms solving step, then holds
before and after solving), so it is mostly waiting. Free heap held at
339,508 bytes, then 339,412 for the rest of the run, and free PSRAM at
7,290,919 bytes throughout. No stack canary, panic or reboot appeared. There
is no PSRAM baseline for this build yet, so its roughly 125 KB of state is
not split out, and the log does not show whether the allocator put it in PSRAM
or in internal heap.

Blaster's row is three steady five-second readings, taken after the canvas
moved to display byte order (issue #23; see below). It is limited by the push,
not by the hack: its step is under 4 ms and its wait is 0. Free heap held
between 337,988 and 338,372 bytes and free PSRAM at 7,424,155 throughout, and
no stack canary, panic or reboot appeared. Before that change its push took
43.4-44.4 ms and it ran at 20.4-20.8 fps. There is no PSRAM baseline for this
build, so its extra PSRAM is not split out.

Substrate is built in single precision (`hacks/substrate_single.c`), and its
row is 30 five-second readings of that build. It starts near 20 fps with a
step of about 3 ms. As the cracks and their sand paint build up, the step grows
over about 40 seconds and settles at 37-38 ms and 12.4-12.6 fps, with a wait
of 0, so from then on the frame is the hack's step plus the push.

The same hack in double precision settled at a step of 119-121 ms and 6.2 fps,
over a 31.8 minute capture (382 readings). That capture also showed one
restart, at about 25 minutes: the step fell to 26 ms and climbed back as the
new picture filled, and free PSRAM returned to the same value (only two
distinct readings in the whole run), so its two buffers were freed and
allocated again without a leak. The single-precision build has not been run
through a restart. Free PSRAM was 1,742,096 bytes lower than under HyperCube in
an earlier log, which matches the two image-sized buffers (2 x 466 x 466 x 4
bytes, plus a few KB), so plain `malloc` put both in PSRAM. Free heap stayed
between 336,228 and 339,524 bytes. No stack canary, panic or reboot appeared in
any run.

For a while after the byte-order fix, every hack pushed in 41-45 ms and ran
at 20-23 fps where the rows above show 31 ms and 28-30 fps. Issue #23 found the
cause: with `setSwapBytes(true)`, M5GFX swapped the bytes of all 217,156 pixels
on every push, which cost 9.8 ms (Pyro pushed in 31.2 ms with the swap off and
41.0 ms with it on). The canvas now holds pixels in the display's byte order
and the swap is off. Steady readings on the device after that change, with the
rotation off:

| Hack | push | fps |
| --- | --- | --- |
| Pyro | 31.2 ms | 30.0 |
| HyperCube | 31.5 ms | 28.0 |
| Blaster | 31.2-31.3 ms | 27.8 |
| Substrate | 31.1-31.2 ms | 28.6-29.0 early on, 18.0 once its step grew to 24 ms |
| Maze | 31.1 ms | 8.0-10.6 (paced by its own delays) |

Substrate's capture was only 22 seconds, so its row above, which covers the
whole build-up, still shows the earlier push of 41.1-44.4 ms and the frame
rates that went with it. Its steps are unchanged. Rows measured before the
byte-order fix and not repeated are unaffected.

Pushing part of the canvas scales with its area. On Pyro, with the swap on,
the top half took 20.6 ms (of 41.0) and a quarter-size block pushed row by row
took 13.7 ms. Each separate `pushImage` call costs about 12 microseconds, so a
row-by-row push of the whole canvas took 46.8 ms and a row-by-row push of the
inscribed circle took 38.7 ms, against 41.0 ms for one call.

Issue #6 then sent only the rows a hack drew. The canvas keeps the leftmost and
rightmost pixel written in each row, and the planner pushes those spans, or one
full push when that is cheaper. M5GFX keeps a framebuffer for this panel, and
`endWrite` flushes one bounding box around everything written in the batch. So
rows are grouped into batches by a cost model fitted to device probes, and rows
far apart get their own: on Pyro, two corner pixels in one batch took 11.7 ms
and 0.1 ms in separate ones, and 20 scattered rows of 40 pixels took 1.8 ms
batched and 0.5 ms apart, while 20 adjacent rows took 0.2 ms batched. A first
version that put every row in one batch left Squiral at 9.3 ms for 20 rows
(thought at the time to be a floor between display updates; it was a flush box
spanning the screen). Steady readings on the device with the grouped batches
and the rotation off (previous push 31 ms and 28-30 fps):

| Hack | push | fps | rows per frame |
| --- | --- | --- | --- |
| Pyro | 1.1-1.5 ms | 83 | 56-74 |
| Squiral | 0.5 ms | 88 | 20-21 |
| Lightning | 4.3-4.5 ms | 64 | 85-91 |
| Maze | 0.5-4.3 ms | 6-29 | 24-76 |
| HyperCube | 12.1-13.4 ms | 55-59 | 287-305 |
| Blaster | 5.1-5.5 ms | 67 | 248-259 |
| CloudLife | 27.7 ms | 26.4 | 464 |
| Pedal | 19.6-33.9 ms | 0.2-0.4 | 348-495 |

Pedal's row is from the first version and was not repeated. Maze, Lightning and
Pyro are held back by their own delays, not the push, and the fps column shows
that for every hack that waits. Substrate's early frames pushed in 1.0-5.0 ms
(44 fps) and its later ones in 3.7-6.4 ms as its picture filled. CloudLife and
Pedal redraw nearly every row, so they gain little. The host estimates (a lower
bound, since they counted only pixels that changed colour) were too low for
hacks that redraw unchanged pixels. Pyro's step stayed at 0.7-0.9 ms and
Substrate's first readings at 2.6-3.0 ms, as before, so the marking costs
nothing visible on the hot path. Rod checked Maze, the name and fps labels and
several hacks on the screen with the first version; the grouped version needs
the same look.

Free heap and free PSRAM return to exactly the same values every time a
hack is switched back to, so switching does not leak.

## What limits the frame rate

- A full push of the 434 KB canvas costs a steady 31 ms, but the firmware now
  sends only the rows a hack drew (issue #6), so the ceiling depends on the
  hack: about 30 fps for hacks that redraw most of the screen, and 55-90 fps
  for sparse ones (see the table above).
- A hack's delay is the pause after it draws, and the firmware credits only
  the 31 ms push against it. Hacks that ask for 10-20 ms therefore run at the
  push ceiling. Before the push was credited the delay was added on top, and
  Pyro, HyperCube and Petri ran at 22-23.5 fps. Crediting the whole step as
  well was tried and dropped: a hack asking for a hold got none once its own
  draw took that long, so slow Pedal pictures were erased as soon as they
  appeared.
- Rorschach and Pedal draw a picture and then hold it for the 5 seconds they
  ask for, so their frame rates (3.0 and 0.2-0.4) measure the holds, not the
  speed. Rorschach draws a picture in about 15 frames over half a second.
- XSpirograph is the slowest at drawing: 54-56 ms a frame for 1000 lines.
  Whether that is the PSRAM pixel writes or its double-precision maths has
  not been separated. Coral (15-25 ms), CloudLife (about 9 ms) and
  WhirlWindWarp (about 6 ms) are the next most expensive; Squiral and
  Critical draw in under a millisecond and run at the push ceiling.
- Coral's step time falls as the picture fills in (15.0 fps, then 21.2).
- Flame was the slowest hack at first: 76-592 ms a step (1.8-7.4 fps), because
  its source is all `double` with `sin`, `cos` and `sqrt` in a recursive
  per-point loop, and the ESP32-S3 has only a single-precision FPU. Building
  it in single precision (`hacks/flame_single.c`) cut the step to 26-107 ms.
  Its frame rate is now 4.4-6.8 fps and set by the hack's own pauses between
  pictures (85-187 ms of wait), not by compute. The scorer's `float-heavy`
  flag missed Flame, because it counts call sites in the source, not how often
  loops run it. A press made during a long step is kept and handled when the
  step ends (see Buttons below).
- WhirlWindWarp's step time sits on plateaus that change over time: about 6,
  8, 10, 19-20 and 25-26 ms across restarts, and it shifted within a single
  run (19.1, then 20.4, then 26.4 ms). It switches around 16 forcefields on
  and off at random, which fits, but which ones are expensive has not been
  identified.
- Free heap and PSRAM showed a single reading per hack across five restarts
  each of Flame and WhirlWindWarp, so repeated starts do not leak.
- The slowest of the xlockmore batch were Discrete (about 1 s a frame) and
  Drift (64 ms) until they were built in single precision, which brought them
  to about 150 ms and 11-13 ms. Vines and Sierpinski are slow because they ask
  for long delays (Sierpinski waits about 409 ms between cheap steps). Rod
  found Discrete's slow updates in keeping with other deliberately slow hacks,
  and saw nothing wrong with Discrete, Drift or Flame in single precision.
- Galaxy first ran at 5 fps (152-172 ms a frame): about 4,400 stars, each
  pulled by every galaxy in `double` maths, which the ESP32-S3 emulates in
  software. Building the unmodified source with `double` redefined as `float`
  (`hacks/galaxy_single.c`) cut the step to 38-42 ms and 14 fps with two
  galaxies, and the frames look the same on the host. A count override of 2
  leaked the star buffers on each restart, so it now uses `count: -3` (two or
  three galaxies, re-picked on each restart). With that it ran at 9.4-11.2 fps
  (58-75 ms a step) across two restarts' configurations, which is the cost of
  the third galaxy and the larger star counts.
- Thornbird keeps 400 buffers of 100 rectangles (about 320 KB) in internal
  heap, filled one per frame, so free heap falls to under 1 KB after about
  six seconds. It is bounded, not a leak: switching away returned the heap
  to its earlier value.
- Hacks that draw many primitives per frame are the ones to profile first.

## Other notes

Rorschach keeps a 9.6 KB array on the stack, which overflowed the Arduino
loop task's default 8 KB stack and rebooted the device on the first frame. The
firmware now sets a 16 KB loop stack (about 8 KB less free heap). Any hack with
large local arrays can hit the same limit.

Pyro's `init` builds two 6284-entry sine and cosine tables in double
precision, so restarting it dips to about 17 fps for a few seconds.

## Buttons

M5Unified reads the StopWatch buttons (GPIO2 for A and GPIO1 for B, active
low) only inside `M5.update()` and keeps no edge, so a press that began and
ended during one long hack step was never seen. The firmware now samples both
pins every 5 ms from a small task on core 0, and a latch counts a press once
the pin has held its new level for 30 ms. Repeated presses while a step runs
count as one, and holding a button does not repeat.

The serial switch line reports `press_waited`, the time from the press to the
switch. On the device it was usually 45-100 ms (the 30 ms settle plus one loop
period). Presses made during long steps waited for them: about 0.4 s on Flame
and 0.5-1.3 s around Pedal and Rorschach. About 90 switches, including bursts
of roughly four presses a second, showed no lost press, no reset and no
memory change.

A first version used GPIO interrupts and read the pin level when each ran.
The button bounced on release, the rising-edge interrupt saw the line low, and
the release was logged as a second press, so one click switched two hacks. The
polling design replaced it.

## Pedal

Pedal picks up to 1000 points per picture. The shim used to skip polygons
over 256 points, and the canvas dropped scanline crossings past 64, so half of
Pedal's pictures drew nothing. Both limits are gone. Sorting the crossings
with insertion sort then cost 2.6 million steps per picture on average and
21.6 million on the worst, which took about 4-5 seconds on the device; with
`qsort` above 16 crossings the step means are 126-519 ms. Pedal remains the
second most expensive hack to draw after Flame.

## Wide lines, arcs and pixmaps

The shim now honours `line_width`, cap and join styles, draws `XDrawArc` and
partial `XFillArc`, and lets `XCopyArea` write into pixmaps made by
`XCreatePixmap`. Width 0 and 1 keep the old line code.

No registered hack draws a wide line here. A survey over 200 frames of each
found a largest width of 1 (XSpirograph and Blaster; their Retina branches do
not run), and Maze's width-2 call sits under `HAVE_JWXYZ`, which is not
defined. So nothing the device runs today uses the new code, and the checks
were that nothing else changed:

- Frame hashes of 26 of the 27 hacks (every one but Coral, which reads the
  clock) are identical before and after, taken at four points over 200 frames.
  Maze, Pyro, Hopalong, Galaxy, Blaster and Substrate also dumped identical
  raw frames.
- On the device, Maze pinned with the rotation off for 75 seconds on each
  build, plain `main` and this branch: 11 five-second windows each, with the
  same medians for fps (24.4), push (1.1 ms) and rows (32), and step medians of
  0.30 and 0.40 ms. The two captures started at different points in Maze's
  cycle, so single windows differ and the 0.1 ms is within the resolution of
  the log. No reboot, panic or stack canary appeared.
- Not measured: the new code on the device. Wide lines, arcs and the pixmap
  copy are covered by host tests only. Blaster, which fills arcs, was not
  re-measured on the device after the change. Pacman is the first port that
  needs them.

## Pacman

Pacman runs at 75.6-79.2 fps (second window onward, 46 windows of 5 seconds
pinned with the rotation off), paced by its own 10 ms delay: the step is
1.3-1.9 ms, the push 0.8-1.4 ms for 45-84 rows a frame, and the wait 10.0-10.6
ms. It takes about 580 KB of PSRAM (7,424,155 free before it starts, 6,842,931
after) and about 50 KB of internal heap (the scaled sprites are each under
4 KB, so `malloc` keeps them in internal RAM). Rod checked the colours on the
screen: cyan walls, a yellow Pacman, the four ghost colours.

Two recursions in the hack had to go. The level generator (`creatlevelblock`
and `nextstep`) recursed up to about 315 KB deep on the host and rebooted the
board on its first frame (stack canary, loop task, 16 KB). The build now
never calls it: half the levels are still the fixed one, as upstream, and the
rest come from a pool of 24 levels that the real generator made on the host
(`tools/pacman_levels.py`), with the cells outside the round display walled off
(issue #32). The ghosts' route home
(`recur_back_track`) reaches 453 levels of 48 bytes from some cells, about
22 KB; the build rewrites it as a loop over a heap stack that visits cells in
the same order, pinned over 30,000 frames. With a temporary stack gauge in the
serial line (not committed), the loop task's lowest free stack was 5,092 bytes
of 16,384 and falling after three minutes with the recursion, and a steady
13,844 bytes over four minutes after the rewrite.

Upstream also leaks about 514 KB of pixmaps each time Pacman starts. The
runner now releases a stopped hack's pixmaps. Rotating every 5 seconds for 70
rotations (two Pacman visits), the hack after each visit read the same heap
and PSRAM (289,228 and 7,342,579).

Not measured: runs longer than four minutes pinned, and the stack gauge only
saw the loop task while ghosts followed routes home, which the host pin shows
they do but the log does not mark.

## Braid

Braid is too slow to be a good screensaver, and four rounds of work took it
from 0.8-2.0 fps to 3.2-8.6 fps. Rod judged the colours and the wide strands
right on the screen, and the colour spin too slow. Each braid lasts 100 frames
and the next is random, so the rate steps between plateaus (strand count and
line width set the cost). One light braid ran at 21.2 fps. Readings are second
windows of 5 seconds, pinned with the rotation off, at 150-180 seconds each.

Braid redraws every segment of the braid on every frame (up to about 7,500
`XDrawLine` calls) to spin the colours, and a random width of 1-7 pixels sends
most of them through the stroker.

| Build | First braid | Second | Third |
| --- | --- | --- | --- |
| As copied | 2.0 fps, 482 ms | 1.2 fps, 828 ms | 0.8 fps, 1,422 ms |
| Single-precision wrapper | 2.8 fps, 350 ms | 1.6 fps, 634 ms | 1.0 fps, 1,142 ms |
| Disc rows cached by width | 6.6 fps, 130 ms | 4.2 fps, 223 ms | 2.8 fps, 357 ms |
| Fast `sin` and `cos` | 7.0 fps, 123 ms | 4.4 fps, 212 ms | 2.8 fps, 342 ms |
| Literals made floats | 8.6 fps, 95 ms | 5.2 fps, 176 ms | 3.2 fps, 296 ms |

The step is the hack's draw call; the push stayed at 19.5-22.7 ms (377-405
rows a frame). What each change was worth, and how it was found:

- **Wide-line discs.** A width above 3 gets round caps, and
  `canvas_fill_ellipse` works every row out in `double` (a division, `sqrt`,
  `ceil`, `floor`), which the S3 does in software. A round-capped segment draws
  two discs. The host profile showed the ellipse at 6% of the time, because a
  Mac does `double` at full speed; on the board it was the largest cost, 2.7 to
  3.2 times. `stroke.c` now builds each width's rows once (widths to 16) and a
  test pins the pixels to `canvas_fill_ellipse`.
- **Literals.** `single_precision.h` renames the keyword `double`, but
  `0.5 * (1.0 + sinf(x)) * r2` and `t / theta * M_PI` were still software
  double: the object file called `__muldf3`, `__adddf3` and `__divdf3`. A probe
  build with
  `XDrawLine` returning at once showed the hack's own step was 45-90 ms,
  and swapping in a fast `sin` and `cos` alone moved it by about 5%. Rewriting
  the literals as floats at build time (`tools/float_literals.py`) removed the
  helpers and took the first braid from 123 to 95 ms. Frames stayed identical.
- **Still slow.** The rest is drawing: each wide segment is a quad filled a row
  at a time (a float division per edge per row, then a rectangle fill per row)
  plus two discs. A braid with many strands and width 7 still takes about 300
  ms. Braid's own `applywordbackto` runs inside the inner loop and cannot be
  changed, because the hack stays byte-identical. Reaching 10 fps would need the
  stroker's per-row cost cut by more than half, which has not been tried.

Free PSRAM was 7,417,507 bytes throughout, and free internal heap 325,196
bytes (326,220 before the disc table, which takes 1 KB of static memory). No
stack canary, panic or reboot appeared in any capture. Not measured: runs over
three minutes, and Braid's restart on the device beyond the three braids seen.

## Four ports that test the Speed bands

Mountain, Epicycle, Kaleidescope and Celtic were ported to test the host Speed
bands, which were fitted to the first 29 hacks. Predictions were committed
before any port (`docs/speed-predictions.md`, which also has the verdicts),
and each was flashed unmodified, pinned with the rotation off, for 180 seconds
(29-35 five-second windows, the first dropped).

- **Mountain** (low band, 0.0005 ms on the host): step median 0.1 ms, 0-0.4,
  paced by its own 20 ms delay at 44-45 fps; it idles at 0.0 ms between
  pictures. It takes about 20 KB of PSRAM (7,404,051 free against 7,424,155).
- **Epicycle** (low band, 0.0009 ms, 31 `double`s): step median 0.4 ms, 0.3-2.1,
  with two of 34 windows over 1.4 ms (1.5 and 2.1 ms; the other 32 are 0.3-0.8
  ms), probably the windows holding a restart. Free heap 327,220-327,612.
- **Kaleidescope** (high band, 0.0695 ms): step median 8.3 ms, 7.0-9.8, at
  27.8-34.4 fps with a 17.7-25.5 ms push. Free heap sat at 219,416-219,556 bytes,
  about 105 KB below an idle build, with PSRAM untouched.
- **Celtic** (high band, 1.12 ms, 42 `double`s, wide round-capped lines):
  step median 832 ms, 29-1,025, at about 1.2 fps. A picture's memory depends
  on the pattern: the first held about 300 KB of the 325 KB of free internal
  heap (free heap read 25,684 bytes), the next two 232,436 and 247,516, and it
  returned to 328,556 between pictures. Nothing aborted (it calls `abort()` on
  a failed allocation, but the board's `malloc` falls back to PSRAM, see
  "Celtic after issue #37"). Three windows drew nothing (rows=0) yet took
  844-1,025 ms a step, so much of the cost is computing, not drawing. Rod took
  it out of the rotation (issue #37). It was registered again after the work
  in the next section.

None showed a stack canary, panic or reboot, and free PSRAM was constant
within each run (Mountain's 20 KB is its offset from the idle figure).

## Celtic after issue #37

Celtic went back into `g_hacks[]` (after HexTrail, so the toasters stay last)
and was measured on the device three times, each pinned with the rotation off
for about 180 seconds (26, 26 and 25 five-second windows, the first dropped),
with one change between captures:

| Build | Step median | Step range | Lowest free heap |
| --- | --- | --- | --- |
| Single precision, upstream graph sizes | 64.8 ms | 5.6-151 ms | 1,792 B |
| And the graph sized to the screen | 56.9 ms | 13.5-129 ms | 164,684 B |
| And a step scale of 4 (the registered build) | 31.8 ms | 12.6-76.8 ms | not sampled (see below) |

- **Single precision** (`celtic_single.c`, literals made floats at build time;
  no soft-double helper in the object). Each animation tick walks the shadow
  look-ahead forward in steps of 0.0001, so a draw call is 25,000 to 58,000
  cubic Bezier evaluations in the big patterns. The float build exposed two
  upstream faults: `spline_value_at` aborts when a fused multiply-add makes
  `t*n - floor(t*n)` slightly negative (`tools/celtic_patch.py` works the
  product out once and clamps), and `celtic_free` never frees the colour table
  (384 bytes a start).
- **Heap.** Upstream picks margins of -600 to 100 (grid, Kennicott) and -900
  to 0 (triangle), and the graph covers the screen plus twice the margin, so
  most of a negative margin is never seen. The board's `malloc` does not fail
  at the end of internal RAM (`CONFIG_SPIRAM_USE_MALLOC`, small blocks
  internal first, then PSRAM): a big pattern drains internal heap, and the
  16 KB drop in free PSRAM in the first capture is that overflow. By a model
  of the board's allocations (a node 12 B, an edge 16 B, a segment 32 B, a 4 B
  block header), 44 of 300 random starts needed more than the 312 KB free; the
  patch (minus one edge for grid and Kennicott, half the width plus two edges
  for the triangle) leaves none, with the worst pattern at about 104 KB. The
  board's lowest reading was 164,684 bytes free, about 147 KB in use, so the
  model probably underestimates by about 40%. No window had less than 164 KB
  free and PSRAM stayed flat.
- **Step scale.** The graph sizing did not change the work in a step (spline
  evaluations per draw call on the host: Kennicott about 58,000, triangle
  about 25,000, grid and polar about 1,000). `CELTIC_STEP_SCALE` multiplies
  the 0.0001 (default 4, `-DCELTIC_STEP_SCALE=<n>` overrides it). Per draw
  call on the host, Kennicott evaluations fell from 55,644 to 29,039, 16,626
  and 9,719 at scales 2, 4 and 8, triangle from 19,875 to 11,280, 7,324 and
  5,021, while line draws per call did not change (1,600 for Kennicott), so
  drawing is now the floor. A picture takes 100, 51, 25 and 13 draw calls.
  Frames at each scale look the same.
- **Reading the last row.** The `fps` column reads 5.2 in every window: a
  picture is now about 25 quick draw calls and then the 5 s `*delay2` pause
  (about 80% of each cycle), so the column measures the pause and `wait` is
  its average. While a picture is being drawn, `1000 / (step + push + 10 ms)`
  is 11.0 fps in the worst window, 20.6 at the median and 38.9 at the best.
  The heap readings in that capture are all idle values (310,208-310,296),
  because the samples fell in the pause with no pattern held, so it says
  nothing about the heap: the figure above is the second capture's, and the
  step scale does not change the graph size.
- Rod saw the knots run edge to edge, the colours and drawing correct, and the
  pace and look good at a step scale of 4. The pause between pictures is a
  little long: the shim's `erase_window` clears at once where upstream draws
  nothing on the first call, so the 5 s wait is on black and not on the
  finished picture (issue #86).

## Deluxe

Deluxe was rated L (`GCPlaneMask`, `allocate_alpha_colors`, pixmaps) and
sat in the high Speed band. Its source is unchanged. `deluxe_opaque.c`
includes it with `*transparent` and `*doubleBuffer` off (plane masks and
pixmap double buffering cannot work on this canvas) and routes full wide
circles to `stroke_circle`. Each build was flashed pinned with the rotation
off and logged for 45 seconds (nine five-second windows, the first read
high because of the name overlay).

- As first ported (opaque, arcs through the polyline stroker) it ran at
  2.0-5.4 fps. A host profile put 94% of a frame in the 50 pixel wide
  circles: 400 frames took 1.14 s, 0.07 s without `XDrawArc`, and 0.81 s with
  the joins skipped. An arc is about a thousand wide segments, each filled a
  row at a time, so each pixel was painted about 25 times.
- `stroke_circle` fills the ring in two spans a row: 400 frames took 0.12 s
  on the host, and the board ran 13.2-15.4 fps, paced by the 32.5 ms push.
  The polyline path also leaves gaps in a ring, which the ring does not.
- Translucency is a per-GC `alpha` (26 of 32, the weight upstream's
  translucent path uses) that `stroke.c` blends over the canvas. Blending each
  piece of a polyline separately blended the overlaps twice, so a star's
  corners showed as bright diamonds (11.2-13.2 fps). Drawing a translucent
  shape into a one-bit-per-pixel mask and blending it once removed them, and
  costs 3-5 ms a frame on the board (10.4-12.8 fps) though nothing on the
  host. That is the registered build.
- Not measured: PSRAM against an idle build (it was flat at 7,327,227 in every
  capture, so the 27 KB mask per shape is returned), a restart or lap soak
  with rotation on, and a long run. Heap held at 322,648-323,056 bytes.
  Colour was judged by Rod on the screen.

Hexadrop, StonerView, Halo and XMatrix were measured on pinned builds
(2026-10-10), one 20-60 second capture each except where a range spans
several. Hexadrop's step with its pixel writes skipped was 0.1 ms, so its
cost is the canvas writes into PSRAM, not the maths. Halo's mode is chosen by
`random()`, a device boot is deterministic, and so every boot picked Ramp;
the Seuss rows come from a temporary build forcing `*colorMode: seuss`. The
colour-pixmap rows are the first version, replaced by packed bit planes (the
Seuss colour-pixmap row's PSRAM is two 434 KB pixmaps). Halo's registered
build asks for a 10 ms pause between frames (upstream asks for 100 ms). XMatrix
runs its typed trace first, then the rain, where it is limited by the 466-row
push. Flurry was not ported, so has no row. Colour and pace were not judged.

Qix, Fireworkx, Starfish, Goop, Loop and Hypnowheel were measured on pinned
builds (2026-10-11), one 25-40 second capture per row. Fireworkx's picture is
rendered at a third of the width and height and drawn four times over by
`lowrez_put.h`, and the scales are a build flag (`FIREWORKX_SCALE`); the
registered build asks for 3. Starfish and Goop fill their polygons with
`float_polygon.h` instead of the shim's double-precision fill, which was most
of their step; Goop also draws straight onto the window instead of a
window-sized pixmap it then copied, and blends each blob in at the alpha
upstream's jwxyz path gives it (23 parts in 32). Their first single-precision
builds drew garbage (a layout mismatch in `spline.h`, since fixed) and the
speeds they were measured at are not recorded; the rows above are from the
fixed builds. Loop's registered build asks for 50 ms
between generations (upstream asks for 100). Hypnowheel's probes used
temporary registry overrides, since reverted. The later rows are
`GL_PROFILE` builds with `-DHYPNOWHEEL_PROBE` (2026-10-11, issue #43): about
275 ms of the 706 ms step was `double` trig, which single precision removed;
half resolution (`-DHYPNOWHEEL_HALF`) took fill from 334 to 129 ms.
Qix's internal heap read about 43 KB low while it ran, which was its leaked
segment arrays (fixed). Colour and pace were not judged.

Fireworkx's registered row scales the lengths it works in (spark speed,
gravity, the flash light's reach, the velocity jitter) by the width it renders
at over the 1024 it is tuned for, throws 120 sparks a shell instead of 500, and
fires two of its four shells (`tools/fireworkx_patch.py`; the build flags are
`FIREWORKX_PIXCOUNT` and `FIREWORKX_SHELLS`): at a third of its size upstream's
constants made every burst fill the picture and the glow blur saturate to
white. Goop's registered row blends all three colour channels with one
multiply (`float_polygon.h`) and clears only where last frame's blobs were
(`span_erase.h`), which also shrinks its push.

## Suggested order for shim stage 2

Shim gaps across 2D hacks, ranked so gaps that block hacks
needing few additions come first.

| Gap | Score | 2D hacks needing it |
| --- | --- | --- |
| `error: too few arguments to function call, expected 2, have 1` | 4.71 | 7 |
| `XYBitmap` | 1.64 | 5 |
| `XQueryColors` | 1.43 | 4 |
| `XDrawString` | 1.39 | 6 |
| `XFree` | 1.27 | 4 |
| `XImage.byte_order` | 1.2 | 5 |
| `visual_rgb_masks` | 1.19 | 4 |
| `pow2.h` | 1.17 | 4 |
| `UnmapNotify` | 1.0 | 1 |
| `XCreateBitmapFromData` | 1.0 | 1 |

## All hacks

| Hack | Kind | Effort | Speed | Ported | Shim gaps | Flags | LOC |
| --- | --- | --- | --- | --- | --- | --- | --- |
| binaryhorizon | 2d | S | high (1.8 ms) | - | - | pixmaps, readback | 624 |
| bouboule | 2d | S | high (0.052 ms) | - | - | xor | 860 |
| ccurve | 2d | S | high (0.55 ms) | - | - | pixmaps | 872 |
| flow | 2d | S | high (0.95 ms) | - | - | pixmaps | 1216 |
| halftone | 2d | S | high (0.68 ms) | ❌ | - | pixmaps | 413 |
| marbling | 2d | S | high (12 ms) | - | - | - | 635 |
| nerverot | 2d | S | high (0.38 ms) | - | - | pixmaps, float-heavy | 1367 |
| rotzoomer | 2d | S | crashed on the host | - | - | pixmaps, readback | 601 |
| twang | 2d | S | crashed on the host | - | - | pixmaps, readback | 791 |
| ant | 2d | M | - | - | `NUMSTIPPLES`, `automata.h`, `hexagonUnit`, `triangleUnit` | - | 1351 |
| barcode | 2d | M | - | - | `XImage.bitmap_bit_order`, `XImage.byte_order`, `XYBitmap` | - | 2055 |
| deco | 2d | M | - | - | `XStoreColors`, `allocate_writable_colors` | - | 345 |
| hyperball | 2d | M | - | - | `UnmapNotify` | - | 2464 |
| imsmap | 2d | M | - | - | `XYBitmap` | - | 426 |
| metaballs | 2d | M | - | - | `BitmapPad`, `XFree`, `XListPixmapFormats` | - | 438 |
| munch | 2d | M | - | - | `i_log2`, `pow2.h` | xor | 462 |
| apollonian | 2d | L | - | - | `FcChar8`, `XColor.color`, `XGlyphInfo`, `XftColor`, `XftColorAllocName`, `XftDraw`, `XftDrawCreate`, `XftDrawDestroy`, `XftDrawStringUtf8`, `XftFont`, `XftFontClose`, `XftTextExtentsUtf8`, `error: expected expression`, `load_xft_font_retry`, `overall` | - | 820 |
| apple2-main | 2d | L | - | - | `A2CONTROLLER_DONE`, `A2CONTROLLER_FREE`, `A2_GR_FULL`, `A2_GR_HIRES`, `A2_GR_LORES`, `ANALOGTV_DEFAULTS`, `ANALOGTV_OPTIONS`, `GrayScale`, `PseudoColor`, `TTY_BLINK`, `TTY_BOLD`, `TTY_INVERSE`, `TTY_SYMBOLS`, `XQueryColors`, `a2_clear_gr`, `a2_clear_hgr`, `a2_cls`, `a2_display_image_loading`, `a2_goto`, `a2_hline`, `a2_hplot`, `a2_invalidate`, `a2_plot`, `a2_printc`, `a2_printc_noscroll`, `a2_prints`, `analogtv_reconfigure`, `ansi-tty.h`, `ansi_tty`, `ansi_tty_free`, `ansi_tty_init`, `ansi_tty_print`, `apple2.h`, `apple2_one_frame`, `apple2_sim_t`, `apple2_start`, `apple2_state_t`, `error: expected expression`, `error: incompatible integer to pointer conversion initializing 'char *' with an expression of type 'int' [-Wint-conversion]`, `flag`, `load_image_async`, `sim`, `st`, `tc`, `tty`, `tty_char`, `tty_flag`, `utf8_encode`, `utf8_to_latin1`, `utf8wc.h`, `visual_cells`, `visual_class`, `visual_rgb_masks` | pixmaps, readback | 1642 |
| blitspin | 2d | L | - | - | `GXand`, `GXclear`, `GXset`, `XDisplayHeight`, `XDisplayWidth`, `XScreenNumberOfScreen`, `file_to_pixmap`, `images/gen/som_png.h`, `pow2.h`, `som_png`, `to_pow2` | pixmaps, readback, clipmask | 467 |
| bsod | 2d | L | - | - | `A2CONTROLLER_DONE`, `A2CONTROLLER_FREE`, `A2_GR_FULL`, `A2_GR_HIRES`, `A2_GR_LORES`, `ANALOGTV_DEFAULTS`, `ANALOGTV_OPTIONS`, `FcChar8`, `XColor.color`, `XFetchName`, `XGlyphInfo`, `XSetPlaneMask`, `XStoreName`, `XftColor`, `XftDraw`, `XftDrawCreate`, `XftDrawDestroy`, `XftDrawStringUtf8`, `XftFont`, `XftFontClose`, `XftTextExtentsUtf8`, `XftTextExtentsUtf8_multi`, `a2_cls`, `a2_goto`, `a2_init_memory_active`, `a2_invalidate`, `a2_poke`, `a2_printc`, `a2_printc_noscroll`, `a2_prints`, `amiga_png`, `android_png`, `apple2.h`, `apple2_one_frame`, `apple2_sim_t`, `apple2_start`, `apple2_state_t`, `apple_png`, `atari_png`, `atm_png`, `dvd_png`, `em`, `error: expected expression`, `font`, `gnome1_png`, `gnome2_png`, `hmac_png`, `images/gen/amiga_png.h`, `images/gen/android_png.h`, `images/gen/apple_png.h`, `images/gen/atari_png.h`, `images/gen/atm_png.h`, `images/gen/dvd_png.h`, `images/gen/gnome1_png.h`, `images/gen/gnome2_png.h`, `images/gen/hmac_png.h`, `images/gen/mac_png.h`, `images/gen/macbomb_png.h`, `images/gen/osx_10_2_png.h`, `images/gen/osx_10_3_png.h`, `images/gen/ransomware_png.h`, `images/gen/sun_png.h`, `load_xft_font_retry`, `mac_png`, `macbomb_png`, `osx_10_2_png`, `osx_10_3_png`, `ov`, `ov2`, `ransomware_png`, `screen_number`, `sim`, `st`, `sun_png`, `utf8_decode_combining`, `utf8wc.h`, `xft.h`, `xft_word_wrap`, `xftwrap.h` | pixmaps, readback, clipmask | 7809 |
| bubbles | 2d | L | - | - | `BUBBLE_MAGIC`, `Bubble`, `Bubble_Step`, `DELETE_BUBBLE`, `KEEP_BUBBLE`, `MAX`, `MAX_DROPPAGE`, `MIN`, `bubbles.h`, `default_bubbles`, `error: expected expression`, `head`, `init_default_bubbles`, `least`, `newpix`, `nextbub`, `num_default_bubbles`, `pixmap_list`, `rv`, `tmp`, `tmppix`, `touch` | pixmaps, clipmask | 1468 |
| bumps | 2d | L | - | - | `XQueryColors` | pixmaps, readback | 705 |
| crystal | 2d | L | - | - | `XCreateColormap`, `XFreeColormap`, `XInstallColormap`, `XSetWindowColormap`, `free_colors`, `has_writable_cells`, `make_random_colormap`, `make_smooth_colormap`, `make_uniform_colormap`, `rotate_colors` | xor, float-heavy | 1286 |
| demon | 2d | L | - | - | `NUMSTIPPLES`, `STIPPLESIZE`, `automata.h`, `hexagonUnit`, `stipples`, `triangleUnit` | - | 953 |
| distort | 2d | L | - | - | `XImage.byte_order`, `XImage.format` | pixmaps, readback | 894 |
| droste | 2d | L | - | - | `i_log2_fast`, `pow2.h` | pixmaps, readback | 686 |
| fiberlamp | 2d | L | - | - | `RootWindow`, `XAllocNamedColor`, `XTranslateCoordinates` | pixmaps | 480 |
| filmleader | 2d | L | - | - | `ANALOGTV_DEFAULTS`, `ANALOGTV_OPTIONS`, `ANALOGTV_SIGNAL_LEN`, `FcChar8`, `XGlyphInfo`, `XftColor`, `XftColorAllocName`, `XftColorFree`, `XftDraw`, `XftDrawCreate`, `XftDrawDestroy`, `XftDrawStringUtf8`, `XftFont`, `XftTextExtentsUtf8`, `analogtv`, `analogtv.h`, `analogtv_allocate`, `analogtv_draw`, `analogtv_input`, `analogtv_input_allocate`, `analogtv_load_ximage`, `analogtv_reception`, `analogtv_reception_update`, `analogtv_reconfigure`, `analogtv_release`, `analogtv_set_defaults`, `analogtv_setup_sync`, `error: expected expression`, `extents`, `load_xft_font_retry`, `screen_number`, `xftfont` | pixmaps, readback | 548 |
| flag | 2d | L | - | - | `GCFont`, `XDrawString`, `XFontStruct`, `XFreeFont`, `XGCValues.font`, `XLoadQueryFont`, `XTextExtents`, `XYBitmap`, `bob_png`, `file_to_pixmap`, `font`, `images/gen/bob_png.h` | pixmaps, readback, text | 570 |
| fluidballs | 2d | L | - | - | `FcChar8`, `RootWindow`, `XTranslateCoordinates`, `XftColor`, `XftColorAllocName`, `XftDraw`, `XftDrawCreate`, `XftDrawDestroy`, `XftDrawStringUtf8`, `XftFont`, `XftFontClose`, `error: expected expression`, `error: too few arguments to function call, expected 2, have 1`, `load_xft_font_retry`, `screen_number` | pixmaps | 881 |
| fontglide | 2d | L | - | - | `FcChar8`, `XDrawString`, `XDrawString16`, `XFreeFont`, `XGetAtomName`, `XGlyphInfo`, `XLoadQueryFont`, `XRenderColor`, `XSetFont`, `XTextExtents`, `XTextExtents16`, `XftColor`, `XftColorAllocValue`, `XftColorFree`, `XftDraw`, `XftDrawCreate`, `XftDrawDestroy`, `XftDrawStringUtf8`, `XftFont`, `XftFontClose`, `XftTextExtentsUtf8`, `bg`, `error: Xft is required under X11`, `error: expected expression`, `extents`, `fg`, `load_xft_font_retry`, `screen_number`, `swap`, `utf8_decode_combining`, `utf8wc.h`, `xftdraw` | pixmaps, readback, text, clipmask | 2474 |
| glitchpeg | 2d | L | - | - | `BitmapBitOrder`, `XImage.bitmap_bit_order`, `XImage.blue_mask`, `XImage.byte_order`, `XImage.green_mask`, `XImage.red_mask`, `XtAppAddInput`, `XtInputExceptMask`, `XtInputId`, `XtInputReadMask`, `XtRemoveInput` | readback | 466 |
| intermomentary | 2d | L | - | - | `XSetTile` | pixmaps | 605 |
| juggle | 2d | L | - | - | `XDrawImageString`, `XDrawString`, `XFontStruct`, `XFreeFontInfo`, `XLoadQueryFont`, `XTextWidth`, `gettimeofday` | text, float-heavy | 2798 |
| julia | 2d | L | - | - | `Cursor`, `XCreatePixmapCursor`, `XDefineCursor`, `XFreeCursor`, `XSetStipple`, `XSetTSOrigin`, `XUndefineCursor` | pixmaps, float-heavy | 451 |
| lcdscrub | 2d | L | - | - | `error: too few arguments to function call, expected 2, have 1` | pixmaps, readback, clipmask | 399 |
| m6502 | 2d | L | - | - | `ANALOGTV_BLACK_LEVEL`, `ANALOGTV_BOT`, `ANALOGTV_DEFAULTS`, `ANALOGTV_OPTIONS`, `ANALOGTV_TOP`, `ANALOGTV_VISLINES`, `ANALOGTV_VIS_END`, `ANALOGTV_VIS_LEN`, `ANALOGTV_VIS_START`, `ANALOGTV_WHITE_LEVEL`, `Bit8`, `analogtv`, `analogtv.h`, `analogtv_allocate`, `analogtv_draw`, `analogtv_draw_solid`, `analogtv_input`, `analogtv_input_allocate`, `analogtv_lcp_to_ntsc`, `analogtv_reception`, `analogtv_reception_update`, `analogtv_reconfigure`, `analogtv_release`, `analogtv_set_defaults`, `analogtv_setup_sync`, `asm6502.h`, `m6502.h`, `m6502_build`, `m6502_destroy6502`, `m6502_next_eval`, `m6502_start_eval_file`, `m6502_start_eval_string`, `machine_6502` | - | 288 |
| memscroller | 2d | L | - | - | `FcChar8`, `XGlyphInfo`, `XImage.blue_mask`, `XImage.green_mask`, `XImage.red_mask`, `XftColor`, `XftColorAllocName`, `XftDraw`, `XftDrawCreate`, `XftDrawDestroy`, `XftDrawStringUtf8`, `XftFont`, `XftFontClose`, `XftTextExtentsUtf8`, `error: expected expression`, `load_xft_font_retry`, `overall`, `screen_number`, `xft.h` | pixmaps | 626 |
| noseguy | 2d | L | - | - | `FcChar8`, `XGlyphInfo`, `XftColor`, `XftColorAllocName`, `XftDraw`, `XftDrawCreate`, `XftDrawDestroy`, `XftDrawStringUtf8`, `XftFont`, `XftFontClose`, `XftTextExtentsUtf8`, `error: expected expression`, `extents`, `images/gen/nose-f1_png.h`, `images/gen/nose-f2_png.h`, `images/gen/nose-f3_png.h`, `images/gen/nose-f4_png.h`, `images/gen/nose-l1_png.h`, `images/gen/nose-l2_png.h`, `images/gen/nose-r1_png.h`, `images/gen/nose-r2_png.h`, `load_xft_font_retry`, `nose_f1_png`, `nose_f2_png`, `nose_f3_png`, `nose_f4_png`, `nose_l1_png`, `nose_l2_png`, `nose_r1_png`, `nose_r2_png`, `screen_number` | pixmaps, readback, clipmask | 720 |
| penetrate | 2d | L | - | - | `FcChar8`, `XGlyphInfo`, `XftColor`, `XftColorAllocName`, `XftDraw`, `XftDrawCreate`, `XftDrawDestroy`, `XftDrawStringUtf8`, `XftFont`, `XftFontClose`, `XftTextExtentsUtf8`, `error: expected expression`, `load_xft_font_retry`, `overall`, `screen_number`, `usleep` | - | 1037 |
| phosphor | 2d | L | - | - | `FALSE`, `FcChar8`, `TTY_BOLD`, `TTY_INVERSE`, `TTY_ITALIC`, `TTY_SYMBOLS`, `Time`, `XColor.color`, `XGlyphInfo`, `XWriteBitmapFile`, `XYBitmap`, `XftColor`, `XftDraw`, `XftDrawCreate`, `XftDrawStringUtf8`, `XftFont`, `XftTextExtentsUtf8`, `_6x10font_png`, `ansi-tty.h`, `ansi_graphics_unicode`, `ansi_tty`, `ansi_tty_free`, `ansi_tty_init`, `ansi_tty_print`, `ansi_tty_resize`, `error: expected expression`, `font`, `images/gen/6x10font_png.h`, `load_xft_font_retry`, `overall`, `screen_number`, `tcell`, `tty`, `tty_char`, `utf8_encode`, `utf8_to_latin1`, `utf8wc.h`, `xftdraw` | pixmaps, readback, clipmask | 1260 |
| polyominoes | 2d | L | - | - | `XImage.bitmap_bit_order`, `XImage.bitmap_unit`, `XImage.byte_order`, `XYBitmap`, `countof` | - | 2370 |
| pong | 2d | L | - | - | `ANALOGTV_BLACK_LEVEL`, `ANALOGTV_BOT`, `ANALOGTV_DEFAULTS`, `ANALOGTV_OPTIONS`, `ANALOGTV_TOP`, `ANALOGTV_VISLINES`, `ANALOGTV_VIS_END`, `ANALOGTV_VIS_LEN`, `ANALOGTV_VIS_START`, `ButtonPressMask`, `ButtonReleaseMask`, `CurrentTime`, `Cursor`, `FocusChangeMask`, `FocusIn`, `FocusOut`, `GrabModeAsync`, `KeyPressMask`, `KeyRelease`, `KeyReleaseMask`, `X11/keysym.h`, `XCreatePixmapCursor`, `XDefineCursor`, `XGrabPointer`, `XHeightMMOfScreen`, `XHeightOfScreen`, `XUngrabPointer`, `XWarpPointer`, `analogtv`, `analogtv.h`, `analogtv_allocate`, `analogtv_draw`, `analogtv_draw_solid`, `analogtv_draw_string`, `analogtv_font`, `analogtv_font_set_char`, `analogtv_input`, `analogtv_input_allocate`, `analogtv_lcp_to_ntsc`, `analogtv_make_font`, `analogtv_reception`, `analogtv_reception_update`, `analogtv_reconfigure`, `analogtv_release`, `analogtv_set_defaults`, `analogtv_setup_sync` | pixmaps | 1109 |
| rdbomb | 2d | L | - | - | `XListPixmapFormats`, `XPixmapFormatValues`, `error: expected ')'`, `error: expected function body after function declarator`, `error: expected parameter declarator`, `error: incompatible integer to pointer conversion passing 'int' to parameter of type 'void *' [-Wint-conversion]`, `error: subscripted value is not an array, pointer, or vector`, `pfv` | - | 571 |
| ripples | 2d | L | - | - | `visual_rgb_masks` | readback | 1127 |
| slidescreen | 2d | L | - | - | `XFree`, `XQueryColors`, `visual_cells` | pixmaps | 507 |
| slip | 2d | L | - | - | `ScreenOfDisplay`, `load_image_async` | pixmaps | 376 |
| speedmine | 2d | L | - | - | `error: too few arguments to function call, expected 2, have 1` | pixmaps, clipmask | 1659 |
| spotlight | 2d | L | - | - | `error: too few arguments to function call, expected 2, have 1` | pixmaps, clipmask | 355 |
| strange | 2d | L | - | - | `GCGraphicsExposures`, `StaticColor`, `TrueColor`, `XGCValues.graphics_exposures`, `XImage.byte_order`, `XQueryColors`, `aligned_free`, `aligned_malloc`, `error: invalid operands to binary expression ('int (Display *, GC, Bool)' (aka 'int (struct XshimDisplay *, struct XshimGC *, int)') and 'long')`, `i_log2`, `pow2.h`, `visual_class`, `visual_rgb_masks` | pixmaps, xor | 1353 |
| swirl | 2d | L | - | - | `XCreateColormap`, `XFree`, `XFreeColormap`, `XInstallColormap`, `XSetWMColormapWindows`, `XSetWindowColormap`, `XStoreColors`, `free_colors`, `make_smooth_colormap`, `rotate_colors` | - | 1447 |
| t3d | 2d | L | - | - | `Button1Mask`, `Button2Mask`, `Button3Mask`, `GXandInverted`, `XAllocColorCells`, `XStoreColors`, `error: too few arguments to function call, expected 2, have 1` | pixmaps, readback, float-heavy | 991 |
| tessellimage | 2d | L | - | - | `ITRIANGLE`, `X11/keysymdef.h`, `XYZ`, `delaunay`, `delaunay.h`, `delaunay_xyzcompare`, `error: expected expression`, `p`, `tt`, `v`, `visual_rgb_masks` | pixmaps, readback | 996 |
| testx11 | 2d | L | - | - | `GCFont`, `XDrawString`, `XGCValues.font`, `XLoadFont`, `colorbars.h`, `draw_colorbars`, `error: expected ')'`, `error: expected function body after function declarator`, `error: expected parameter declarator` | pixmaps, readback, text, clipmask | 968 |
| vfeedback | 2d | L | - | - | `ANALOGTV_DEFAULTS`, `ANALOGTV_OPTIONS`, `ANALOGTV_SIGNAL_LEN`, `RANDSIGN`, `analogtv`, `analogtv.h`, `analogtv_allocate`, `analogtv_draw`, `analogtv_input_allocate`, `analogtv_load_ximage`, `analogtv_reception`, `analogtv_reception_update`, `analogtv_reconfigure`, `analogtv_release`, `analogtv_set_defaults`, `analogtv_setup_sync` | pixmaps, readback | 592 |
| whirlygig | 2d | L | - | - | `XDrawString` | pixmaps, text, float-heavy | 741 |
| xanalogtv | 2d | L | - | - | `ANALOGTV_DEFAULTS`, `ANALOGTV_OPTIONS`, `ANALOGTV_PIC_LEN`, `ANALOGTV_SCALE`, `ANALOGTV_SIGNAL_LEN`, `ANALOGTV_V`, `ANALOGTV_VIS_LEN`, `ANALOGTV_VIS_START`, `XrmDatabase`, `XrmPutResource`, `XrmValue`, `analogtv`, `analogtv.h`, `analogtv_allocate`, `analogtv_draw`, `analogtv_draw_solid_rel_lcp`, `analogtv_draw_string_centered`, `analogtv_font`, `analogtv_input`, `analogtv_input_allocate`, `analogtv_lcp_to_ntsc`, `analogtv_load_ximage`, `analogtv_make_font`, `analogtv_reception`, `analogtv_reception_update`, `analogtv_reconfigure`, `analogtv_release`, `analogtv_set_defaults`, `analogtv_setup_sync`, `analogtv_setup_teletext`, `db`, `file_to_pixmap`, `gethostname`, `images/gen/testcard_bbcf_png.h`, `images/gen/testcard_pm5544_png.h`, `images/gen/testcard_rca_png.h`, `inp`, `input`, `load_image_async`, `rec`, `testcard_bbcf_png`, `testcard_pm5544_png`, `testcard_rca_png`, `value` | pixmaps, readback | 689 |
| xflame | 2d | L | - | - | `bob_png`, `images/gen/bob_png.h` | readback | 826 |
| xlyap | 2d | L | - | - | `Cursor`, `X11/cursorfont.h`, `XComposeStatus`, `XPending`, `XStoreColors`, `error: expected expression` | pixmaps | 1939 |
| zoom | 2d | L | - | - | `error: too few arguments to function call, expected 2, have 1` | pixmaps, readback | 290 |
| antinspect | gl | XL | - | - | - | - | 696 |
| antmaze | gl | XL | - | - | - | - | 1612 |
| antspotlight | gl | XL | - | - | - | - | 797 |
| atlantis | gl | XL | - | - | - | - | 607 |
| atunnel | gl | XL | - | - | - | - | 315 |
| b_lockglue | gl | XL | - | - | - | - | 240 |
| beats | gl | XL | - | - | - | - | 439 |
| blinkbox | gl | XL | - | - | - | - | 615 |
| blocktube | gl | XL | - | ❌ | - | - | 454 |
| boing | gl | XL | - | ❌ | - | float-heavy | 666 |
| bouncingcow | gl | XL | - | - | - | - | 649 |
| boxed | gl | XL | - | - | - | - | 1361 |
| cage | gl | XL | - | - | - | - | 498 |
| carousel | gl | XL | - | - | - | - | 982 |
| chompytower | gl | XL | - | - | - | - | 1131 |
| circuit | gl | XL | - | - | - | - | 2106 |
| cityflow | gl | XL | - | - | - | - | 561 |
| companion | gl | XL | - | ❌ | - | - | 605 |
| covid19 | gl | XL | - | - | - | - | 657 |
| crackberg | gl | XL | - | - | `XNextEvent`, `XPeekEvent`, `XPending` | float-heavy | 1484 |
| crumbler | gl | XL | - | - | - | - | 905 |
| cube21 | gl | XL | - | - | - | - | 943 |
| cubenetic | gl | XL | - | - | - | - | 616 |
| cubestack | gl | XL | - | - | - | - | 453 |
| cubestorm | gl | XL | - | - | - | - | 485 |
| cubetwist | gl | XL | - | - | - | - | 579 |
| cubocteversion | gl | XL | - | - | - | - | 5657 |
| dangerball | gl | XL | - | - | - | - | 377 |
| deepstars | gl | XL | - | - | - | - | 385 |
| discoball | gl | XL | - | - | - | - | 709 |
| dnalogo | gl | XL | - | - | - | float-heavy | 3657 |
| dumpsterfire | gl | XL | - | - | - | - | 845 |
| dymaxionmap | gl | XL | - | - | - | readback | 1729 |
| endgame | gl | XL | - | - | - | - | 1451 |
| energystream | gl | XL | - | - | - | - | 536 |
| engine | gl | XL | - | - | - | - | 1015 |
| esper | gl | XL | - | - | - | - | 2463 |
| extrusion | gl | XL | - | - | - | - | 552 |
| flipflop | gl | XL | - | - | - | - | 875 |
| flipscreen3d | gl | XL | - | - | - | - | 524 |
| fliptext | gl | XL | - | - | - | - | 1005 |
| floppy | gl | XL | - | ❌ | - | - | 584 |
| flurry | gl | XL | - | ❌ | - | - | 550 |
| flyingtoasters | gl | XL | - | - | - | - | 924 |
| geodesic | gl | XL | - | - | - | float-heavy | 825 |
| geodesicgears | gl | XL | - | - | - | float-heavy | 1802 |
| gflux | gl | XL | - | - | - | - | 799 |
| gibson | gl | XL | - | - | - | - | 1317 |
| glblur | gl | XL | - | - | - | - | 630 |
| glcells | gl | XL | - | - | - | - | 1382 |
| gleidescope | gl | XL | - | - | `XOFFSET` | float-heavy | 1620 |
| glforestfire | gl | XL | - | - | - | - | 1089 |
| glhanoi | gl | XL | - | - | - | float-heavy | 2080 |
| glknots | gl | XL | - | - | - | - | 455 |
| glmatrix | gl | XL | - | - | - | readback | 1077 |
| glplanet | gl | XL | - | - | - | float-heavy | 1111 |
| glschool | gl | XL | - | - | - | - | 229 |
| glslideshow | gl | XL | - | - | - | - | 1917 |
| glsnake | gl | XL | - | - | - | - | 2699 |
| gltext | gl | XL | - | - | - | - | 619 |
| gravitywell | gl | XL | - | - | - | - | 770 |
| handsy | gl | XL | - | - | - | - | 1158 |
| headroom | gl | XL | - | - | - | - | 628 |
| hexstrut | gl | XL | - | - | - | - | 511 |
| highvoltage | gl | XL | - | - | - | - | 949 |
| hilbert | gl | XL | - | - | - | - | 1165 |
| hopffibration | gl | XL | - | - | - | - | 3580 |
| hydrostat | gl | XL | - | - | - | - | 796 |
| hypertorus | gl | XL | - | - | - | float-heavy | 2150 |
| hypnowheel | gl | XL | - | ❌ | - | - | 335 |
| jigglypuff | gl | XL | - | - | - | - | 1125 |
| jigsaw | gl | XL | - | - | - | - | 1512 |
| juggler3d | gl | XL | - | - | - | float-heavy | 3023 |
| kaleidocycle | gl | XL | - | - | - | - | 584 |
| kallisti | gl | XL | - | - | - | - | 357 |
| klein | gl | XL | - | - | - | float-heavy | 3574 |
| klondike | gl | XL | - | - | - | - | 803 |
| lament | gl | XL | - | - | - | - | 1801 |
| lavalite | gl | XL | - | - | - | - | 1552 |
| lockward | gl | XL | - | - | - | - | 964 |
| mapscroller | gl | XL | - | - | - | readback | 1672 |
| maze3d | gl | XL | - | - | - | - | 1958 |
| menger | gl | XL | - | - | - | - | 574 |
| mirrorblob | gl | XL | - | - | - | - | 1822 |
| moebius | gl | XL | - | - | - | - | 794 |
| moebiusgears | gl | XL | - | - | - | - | 446 |
| molecule | gl | XL | - | - | - | - | 1716 |
| nakagin | gl | XL | - | - | - | - | 1636 |
| noof | gl | XL | - | - | - | - | 530 |
| papercube | gl | XL | - | - | - | - | 1111 |
| peepers | gl | XL | - | - | `XChangeProperty`, `XInternAtom` | float-heavy | 1470 |
| photopile | gl | XL | - | - | - | - | 869 |
| pinion | gl | XL | - | - | - | - | 1497 |
| pipes | gl | XL | - | - | - | - | 1208 |
| platonicfolding | gl | XL | - | - | - | - | 3465 |
| polyhedra-gl | gl | XL | - | - | - | - | 687 |
| polytopes | gl | XL | - | - | - | - | 3194 |
| projectiveplane | gl | XL | - | - | - | float-heavy | 2643 |
| providence | gl | XL | - | - | - | float-heavy | 811 |
| pulsar | gl | XL | - | - | - | - | 509 |
| quasicrystal | gl | XL | - | - | - | - | 494 |
| queens | gl | XL | - | - | - | - | 608 |
| raverhoop | gl | XL | - | - | - | - | 771 |
| razzledazzle | gl | XL | - | - | - | - | 728 |
| romanboy | gl | XL | - | - | - | - | 2565 |
| rubik | gl | XL | - | - | - | - | 2156 |
| rubikblocks | gl | XL | - | - | - | - | 651 |
| sballs | gl | XL | - | - | - | - | 830 |
| sierpinski3d | gl | XL | - | - | - | - | 579 |
| skulloop | gl | XL | - | - | - | - | 651 |
| skytentacles | gl | XL | - | - | - | float-heavy | 1124 |
| sonar | gl | XL | - | - | - | - | 1266 |
| sphereeversion | gl | XL | - | - | - | - | 1420 |
| spheremonics | gl | XL | - | - | - | - | 926 |
| splitflap | gl | XL | - | - | - | - | 1427 |
| splodesic | gl | XL | - | - | - | float-heavy | 645 |
| sproingiewrap | gl | XL | - | - | - | - | 227 |
| squirtorus | gl | XL | - | - | - | - | 1006 |
| stairs | gl | XL | - | - | - | - | 601 |
| starwars | gl | XL | - | - | - | - | 1080 |
| superquadrics | gl | XL | - | - | - | - | 811 |
| surfaces | gl | XL | - | - | - | float-heavy | 654 |
| tangram | gl | XL | - | - | - | - | 1074 |
| timetunnel | gl | XL | - | - | - | - | 1259 |
| topblock | gl | XL | - | - | - | - | 891 |
| tronbit | gl | XL | - | - | - | - | 532 |
| unicrud | gl | XL | - | - | - | - | 1039 |
| vigilance | gl | XL | - | - | - | - | 1171 |
| voronoi | gl | XL | - | - | - | - | 543 |
| winduprobot | gl | XL | - | - | - | float-heavy | 2505 |
| worldpieces | gl | XL | - | - | - | float-heavy | 2194 |
| xshadertoy | gl | XL | - | - | `XFetchName`, `XStoreName` | - | 1192 |
| abstractile | 2d | - | 2.4-11.1 ms measured | ✅ | - | - | 1625 |
| anemone | 2d | - | 25.5-46.3 ms measured | ✅ | - | pixmaps | 458 |
| anemotaxis | 2d | - | 19.8-22.5 ms measured | ✅ | - | pixmaps | 760 |
| attraction | 2d | - | 5.6 ms measured | ✅ | - | - | 1115 |
| binaryring | 2d | - | - | ✅ | - | pixmaps, readback | 577 |
| blaster | 2d | - | 3.6-3.7 ms measured | ✅ | - | - | 1208 |
| boxfit | 2d | - | 9.1-17 ms measured | ✅ | - | pixmaps, readback | 573 |
| braid | 2d | - | 95-296 ms measured | ✅ | - | - | 444 |
| celtic | 2d | - | 12.6-76.8 ms measured | ✅ | - | - | 1141 |
| cloudlife | 2d | - | 8.8-8.9 ms measured | ✅ | - | - | 440 |
| compass | 2d | - | 30.5-31.5 ms measured | ✅ | - | pixmaps, float-heavy | 999 |
| coral | 2d | - | 15-25 ms measured | ✅ | - | - | 328 |
| critical | 2d | - | 0.7 ms measured | ✅ | - | - | 462 |
| cwaves | 2d | - | 16.1-16.2 ms measured | ✅ | - | - | 219 |
| decayscreen | 2d | - | 3.3-3.9 ms measured | ✅ | - | pixmaps | 392 |
| deluxe | 2d | - | 44.6-62.6 ms measured | ✅ | - | pixmaps, float-heavy | 480 |
| discrete | 2d | - | 149-160 ms measured | ✅ | - | - | 442 |
| drift | 2d | - | 11-13 ms measured | ✅ | - | - | 674 |
| epicycle | 2d | - | 0.3-2.1 ms measured | ✅ | - | - | 803 |
| euler2d | 2d | - | 27.9 ms measured | ✅ | - | float-heavy | 893 |
| fadeplot | 2d | - | 2.9-3.2 ms measured | ✅ | - | - | 243 |
| fireworkx | 2d | - | - | ✅ | - | - | 882 |
| flame | 2d | - | 26-107 ms measured | ✅ | - | - | 457 |
| forest | 2d | - | 13-58.9 ms measured | ✅ | - | - | 241 |
| fuzzyflakes | 2d | - | 30.2-30.9 ms measured | ✅ | - | pixmaps | 655 |
| galaxy | 2d | - | 58-75 ms measured | ✅ | - | - | 462 |
| grav | 2d | - | 1.5-1.9 ms measured | ✅ | - | - | 360 |
| greynetic | 2d | - | 1.8-2 ms measured | ✅ | - | - | 297 |
| halo | 2d | - | - | ✅ | - | pixmaps | 459 |
| helix | 2d | - | 0.9-2.1 ms measured | ✅ | - | - | 358 |
| hexadrop | 2d | - | - | ✅ | - | - | 446 |
| hopalong | 2d | - | 7.3-20.1 ms measured | ✅ | - | - | 563 |
| hypercube | 2d | - | 2.7-2.9 ms measured | ✅ | - | - | 576 |
| ifs | 2d | - | 19-23.2 ms measured | ✅ | - | pixmaps | 560 |
| interaggregate | 2d | - | 41.8-47.4 ms measured | ✅ | - | - | 989 |
| kaleidescope | 2d | - | 7-9.8 ms measured | ✅ | - | - | 514 |
| kumppa | 2d | - | 44.5 ms measured | ✅ | - | pixmaps | 545 |
| laser | 2d | - | 21.6-25.1 ms measured | ✅ | - | - | 356 |
| lightning | 2d | - | 1.8-1.9 ms measured | ✅ | - | - | 602 |
| lisa | 2d | - | 51.9-52.5 ms measured | ✅ | - | - | 744 |
| lissie | 2d | - | 0.3-0.4 ms measured | ✅ | - | - | 323 |
| lmorph | 2d | - | 24.7-34.3 ms measured | ✅ | - | float-heavy | 580 |
| maze | 2d | - | 0.1-1.3 ms measured | ✅ | - | pixmaps, clipmask | 1681 |
| moire | 2d | - | 9.5-9.7 ms measured | ✅ | - | - | 253 |
| moire2 | 2d | - | 28.1-28.3 ms measured | ✅ | - | pixmaps, xor | 363 |
| mountain | 2d | - | 0-0.4 ms measured | ✅ | - | - | 283 |
| pedal | 2d | - | 126-519 ms measured | ✅ | - | - | 339 |
| penrose | 2d | - | 0-0.9 ms measured | ✅ | - | - | 1342 |
| petri | 2d | - | 0.3-2.5 ms measured | ✅ | - | - | 780 |
| piecewise | 2d | - | 41.2-44 ms measured | ✅ | - | pixmaps | 1036 |
| popsquares | 2d | - | 15.3 ms measured | ✅ | - | pixmaps | 310 |
| pyro | 2d | - | 0.8-1.1 ms measured | ✅ | - | - | 373 |
| qix | 2d | - | 8.8-17.9 ms measured | ✅ | - | - | 642 |
| rocks | 2d | - | 2.5-3.2 ms measured | ✅ | - | pixmaps | 562 |
| rorschach | 2d | - | 0.6-1.7 ms measured | ✅ | - | - | 227 |
| rotor | 2d | - | 0.5 ms measured | ✅ | - | - | 394 |
| scooter | 2d | - | 19-23.1 ms measured | ✅ | - | - | 975 |
| sierpinski | 2d | - | 1.8-4 ms measured | ✅ | - | - | 215 |
| sphere | 2d | - | 0.6-1.1 ms measured | ✅ | - | - | 304 |
| spiral | 2d | - | 0.9-1.2 ms measured | ✅ | - | - | 331 |
| squiral | 2d | - | 0.1-0.3 ms measured | ✅ | - | - | 335 |
| starfish | 2d | - | - | ✅ | - | - | 564 |
| substrate | 2d | - | 2.9-43.9 ms measured | ✅ | - | - | 780 |
| thornbird | 2d | - | 2-2.2 ms measured | ✅ | - | - | 270 |
| truchet | 2d | - | 652-900 ms measured | ✅ | - | pixmaps | 541 |
| vermiculate | 2d | - | 1.3-2.2 ms measured | ✅ | - | - | 1229 |
| vines | 2d | - | 133-160 ms measured | ✅ | - | - | 190 |
| wander | 2d | - | 4.4 ms measured | ✅ | - | pixmaps | 284 |
| whirlwindwarp | 2d | - | 6.2-26.4 ms measured | ✅ | - | - | 509 |
| worm | 2d | - | 0.3 ms measured | ✅ | - | xor | 434 |
| wormhole | 2d | - | 22.4-23.1 ms measured | ✅ | - | pixmaps | 734 |
| xspirograph | 2d | - | 54-56 ms measured | ✅ | - | - | 338 |
| cynosure | 2d | - | 17-17.8 ms measured | ✅ | `XCreateBitmapFromData` | - | 457 |
| eruption | 2d | - | 39-39.1 ms measured | ✅ | `error: typedef redefinition with different types ('unsigned long' vs 'unsigned int')` | - | 608 |
| triangle | 2d | - | 3.6-6 ms measured | ✅ | `free_colors`, `make_smooth_colormap` | - | 355 |
| xrayswarm | 2d | - | 21.2-21.6 ms measured | ✅ | `initTime` | - | 1235 |
| goop | 2d | - | - | ✅ | `AllPlanes`, `GXclear`, `XSetPlaneMask` | pixmaps, xor | 651 |
| interference | 2d | - | 67.9-68 ms measured | ✅ | `XShmGetEventBase`, `error: too few arguments to function call, expected 2, have 1` | pixmaps | 1002 |
| loop | 2d | - | - | ✅ | `STIPPLESIZE`, `automata.h`, `hexagonUnit`, `stipples`, `triangleUnit` | - | 1700 |
| pacman | 2d | - | 1.3-1.9 ms measured | ✅ | `BLUE`, `GHOSTS`, `GHOST_DANGER`, `JAILHEIGHT`, `LEVHEIGHT`, `LEVWIDTH`, `MAXGDIR`, `MAXGFLASH`, `MAXGWAG`, `MAXMOUTH`, `MINGRIDSIZE`, `MINSIZE`, `NOWHERE`, `NUM_BONUS_DOTS`, `PAC_DEATH_FRAMES`, `START`, `XDrawString`, `XLoadQueryFont`, `chasing`, `error: expected expression`, `error: invalid application of 'sizeof' to an incomplete type 'argtype[]'`, `ghoststruct`, `goingin`, `goingout`, `hiding`, `images/gen/pacman_png.h`, `inbox`, `pacman.h`, `pacman_ai.h`, `pacman_bonus_dot_eaten`, `pacman_bonus_dot_pos`, `pacman_createnewlevel`, `pacman_eat_bonus_dot`, `pacman_ghost_update`, `pacman_is_bonus_dot`, `pacman_level.h`, `pacman_png`, `pacman_trackmouse`, `pacman_update`, `pacmangamestruct`, `pp`, `ps_chasing`, `ps_dieing`, `ps_eating` | pixmaps, text, clipmask | 1479 |
| shadebobs | 2d | - | 2-2.3 ms measured | ✅ | `XFree`, `XListPixmapFormats` | readback | 474 |
| xjack | 2d | - | 0.1-13.8 ms measured | ✅ | `FcChar8`, `XGlyphInfo`, `XftColor`, `XftColorAllocName`, `XftDraw`, `XftDrawCreate`, `XftDrawDestroy`, `XftDrawStringUtf8`, `XftFont`, `XftFontClose`, `XftTextExtentsUtf8`, `error: expected expression`, `load_xft_font_retry`, `overall`, `screen_number` | pixmaps | 508 |
| xmatrix | 2d | - | - | ✅ | `images/gen/matrix1_png.h`, `images/gen/matrix1b_png.h`, `images/gen/matrix2_png.h`, `images/gen/matrix2b_png.h`, `matrix1_png`, `matrix1b_png`, `matrix2_png`, `matrix2b_png` | pixmaps, readback | 1915 |
| cubicgrid | gl | - | - | ✅ | - | - | 322 |
| etruscanvenus | gl | - | 53.2-78.6 ms measured | ✅ | - | - | 2761 |
| gears | gl | - | 65-330 ms measured | ✅ | - | - | 953 |
| graphstat | gl | - | 35-67.8 ms measured | ✅ | - | - | 750 |
| hextrail | gl | - | - | ✅ | - | - | 782 |
| morph3d | gl | - | 48-86 ms measured | ✅ | - | - | 841 |
| stonerview | gl | - | 12.6-15.6 ms measured | ✅ | - | - | 156 |
| unknownpleasures | gl | - | 162-169 ms measured | ✅ | - | readback | 736 |

## Failed ports

- **blocktube**: 8.8-10 fps at 400 blocks, half size; Rod dropped it (slow,
  little going on)
- **boing**: 8.3 fps and plain on a small screen; Rod dropped it, scanlines cost
  83 ms
- **companion**: about 71,000 vertices a cube, est. 2-3 fps; Rod dropped it
- **floppy**: 630 KB model won't fit the 3 MB app partition, est. 5 fps; Rod
  dropped it
- **flurry**: needs GL TinyGL lacks, est. under 1 fps, issue #70
- **halftone**: 5 fps; the shim's ellipse fill is 80% of it, issue #57
- **hypnowheel**: 4.0 fps at half resolution, fill-bound; Rod dropped it, #73

## Excluded files

These files in `hacks/` and `hacks/glx/` have no `XSCREENSAVER_MODULE`
entry point, so they are models, helper libraries or command-line
tools rather than screensavers:

analogtv, analogtv-cli, ansi-tty, apple2, asm6502, b_draw, b_sphere, bubble3d,
bubbles-default, buildlwo, chessmodels, companion_disc, companion_heart,
companion_quad, countries, cow_face, cow_hide, cow_hoofs, cow_horns, cow_tail,
cow_udder, delaunay, dolphin, dropshadow, dumpster_model, dymaxionmap-coords,
earth, erase-gl, extrusion-helix2, extrusion-helix3, extrusion-helix4,
extrusion-joinoffset, extrusion-screw, extrusion-taper, extrusion-twistoid,
ffmpeg-out, floppy_model, flurry-smoke, flurry-spark, flurry-star, flurry-
texture, fps, fps-gl, gllist, glschool_alg, glschool_gl, glsl-utils,
gltrackball, glut_stroke, glut_swidth, grab-ximage, handsy_model,
headroom_model, highvoltage_model, hopfanimations, involute, kallisti_model,
klondike-game, lament_model, marching, normals, pacman_ai, pacman_level,
pipeobjs, polyhedra, quaternion, quickhull, recanim, robot, robot-wireframe,
rotator, s1_1, s1_2, s1_3, s1_4, s1_5, s1_6, s1_b, screenhack, seccam, shark,
ships, skull_model, sonar-icmp, sonar-sim, sphere, sphereeversion-analytic,
sphereeversion-corrugations, splitflap_obj, sproingies, stonerview-move,
stonerview-osc, stonerview-view, swim, tangram_shapes, teapot, teeth_model,
texfont, timezones, toast, toast2, toaster, toaster_base, toaster_handle,
toaster_handle2, toaster_jet, toaster_knob, toaster_slots, toaster_wing,
trackball, triangle, tronbit_idle1, tronbit_idle2, tronbit_no, tronbit_yes,
tube, tunnel_draw, webcollage-helper, whale, ximage-loader, xlock-gl-utils,
xlockmore, xscreensaver-getimage, xscreensaver-gl-visual, xsublim
