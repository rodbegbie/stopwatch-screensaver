# xscreensaver hacks on the M5Stack StopWatch: design

## Purpose

Run xscreensaver "hacks" on the M5Stack StopWatch (SKU C152, ESP32-S3,
466×466 round AMOLED), each with its default settings. This is primarily a
learning exercise in embedded development, so clarity of toolchain and
workflow matters more than polish.

## Goals

- Pyro (the first target) running on the device, for nostalgia.
- A reusable X11 shim, so further 2D hacks can be added cheaply.
- Everything installed inside this folder where possible, so cleanup is
  `rm -rf`.
- A porting-assessment document ranking every hack by porting effort
  (see Deliverables).

## Non-goals

- Per-hack settings, preferences UI, or the xscreensaver daemon.
- Sound, touch, IMU, vibration or Wi-Fi features.
- GL hacks in this phase (see Future work).

## Key decisions

- **Framework:** Arduino on PlatformIO, using M5Unified and M5GFX for the
  display. Chosen over raw ESP-IDF for the vendor-supported path and lower
  setup friction.
- **Porting approach:** an X11 shim that implements the Xlib calls the
  hacks use and draws into an off-screen canvas, so hack source compiles
  nearly unmodified. Chosen over rewriting each hack natively, which would
  repeat work and drift from the originals.
- **Canvas, not direct drawing:** a 466×466 RGB565 canvas (about 434 KB) in
  PSRAM, pushed to the display once per frame. Avoids flicker and allows
  pixel read-back and XOR drawing. The ESP32-S3 has 8 MB of PSRAM.
- **Toolchain isolation:** PlatformIO installed into `.venv` with `uv`, with
  `PLATFORMIO_CORE_DIR` pointed inside the project so toolchain downloads
  stay in this folder.

## Architecture

```text
firmware/
  platformio.ini        Arduino framework, M5Unified/M5GFX pinned
  src/main.cpp          setup/loop, hack selection, button handling
  src/x11shim/          Display, GC, Window, Pixmap and Draw* functions
  src/hacks/            hack sources copied from vendor, minimally patched
tools/                  helper scripts (coverage scoring, assessment)
docs/                   specs, plans, porting assessment
vendor/                 pristine xscreensaver 6.16 (untracked, read-only)
```

- Each hack follows the standard `screenhack` shape: `init`, `draw`
  (returns a delay in microseconds) and `free`.
- The `main` loop calls the current hack's `draw`, pushes the canvas to the
  display, then waits for the returned delay.
- Button A selects the next hack and button B the previous one. Switching
  clears the canvas and re-runs `init`.
- Hacks render to the full square. The round display clips the corners.
- Hack resource lookups (`get_integer_resource` and similar) are served
  from each hack's built-in defaults table.
- Vendored sources are copied, not edited in place, so our patches show up
  in git.

## Shim scope

Stage 1 (needed for Pyro and similar hacks):

- `XCreateGC`, `XFreeGC`, `XSetForeground`, `XGetWindowAttributes`,
  `XClearWindow`
- `XDrawPoint`, `XDrawLine`, `XDrawLines`, `XFillRectangle`, `XFillArc`,
  `XFillPolygon`
- `XAllocColor`, `XFreeColors`, mapped directly to RGB565 values
- The resource-lookup helpers

Stage 2 (later): pixmaps, `XCopyArea`, `XGetImage`/`XPutPixel`,
`XSetFunction` (XOR), clip masks, text drawing.

Pyro's call list (checked against `hacks/pyro.c`) falls entirely inside
stage 1.

## Verification

- **Desktop build:** the shim and hacks also compile on the Mac against a
  plain RGB565 buffer that can be dumped to PNG after N iterations. This
  lets us check a hack draws something sensible without hardware, and
  doubles as a debugging tool.
- **On-device:** build and flash from the terminal. Rod reports what he
  sees on the screen. Compile success and on-device success are reported
  separately.

## Flashing, safety and recovery

- Keep the default partition scheme `app3M_fat9M_16MB` so the browser
  restore at <https://workos.com/init/badge/install> keeps working. Do not
  touch the `ffat` partition.
- Before the first flash, run read-only `esptool` commands to confirm the
  board and its flash layout, then offer a full 16 MB flash backup to a
  file in this folder.
- First hardware milestone is a hello-world (solid colour plus serial
  output), proving toolchain and display before any X11 code runs.
- Serial port: `/dev/cu.usbmodem112401`. The other port,
  `/dev/cu.usbmodem14201`, is unidentified and will not be touched.
- If auto-reset into the bootloader fails, Rod holds the power button for
  about 2 seconds until the green LED lights.
- An unexplained flash failure means stop and diagnose, not repeated
  full-chip erases.
- Work happens on `feature/pyro-m5-stopwatch`. `vendor/` stays untracked.

## Deliverables

1. Firmware running Pyro on the StopWatch.
2. The reusable shim with the desktop PNG test harness.
3. **Porting assessment** (`docs/porting-assessment.md`): a table of every
   hack in the tree with:
   - the X calls it needs that the shim doesn't yet provide (the extra shim
     work required);
   - whether it is 2D or GL;
   - an estimated CPU and memory cost (for example heavy pixel read-back,
     large pixmaps, floating-point heavy loops);
   - an overall effort rating.

   The static ratings come from a script in `tools/` that scores each hack
   against the shim's coverage. CPU and memory estimates are heuristic and
   will be labelled as such. Measured numbers are added for the hacks we
   actually run.

## Future work

- Stage 2 shim, then more 2D hacks, prioritised by the assessment.
- GL hacks: possible via a software rasteriser (for example TinyGL) at
  reduced resolution. This is a separate project, with no performance
  numbers yet.

## Open questions

- None blocking. The exact set of follow-up hacks (provisionally
  `attraction` and `hypercube`) is chosen after the coverage script runs.
