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
  count of Xlib calls cannot see. It does not run anything, so treat the
  effort ratings as a prioritisation aid, not an estimate.

## Effort ratings

| Rating | Meaning | Count |
| --- | --- | --- |
| S | 2D, and the unmodified source compiles against the shim | 26 |
| M | 2D, 1-4 shim gaps, no pixmaps or pixel read-back | 15 |
| L | 2D, 5+ shim gaps, or uses pixmaps or pixel read-back | 102 |
| XL | GL: needs a software rasteriser (see below) | 140 |

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

## Measured on the device

Twenty-four hacks have been run so far (default settings, 466×466 canvas
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

Free heap and free PSRAM return to exactly the same values every time a
hack is switched back to, so switching does not leak.

## What limits the frame rate

- Pushing the 434 KB canvas costs a steady 31 ms whatever the hack draws, so
  about 31 fps is the real ceiling today. The best hacks reach about 30.
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

## Suggested order for shim stage 2

Shim gaps across 2D hacks, ranked so gaps that block hacks
needing few additions come first.

| Gap | Score | 2D hacks needing it |
| --- | --- | --- |
| `XCreatePixmap` | 9.12 | 57 |
| `XSetLineAttributes` | 4.21 | 27 |
| `XDrawArc` | 3.2 | 17 |
| `LineSolid` | 3.14 | 24 |
| `XParseColor` | 2.57 | 8 |
| `XImage` | 2.19 | 39 |
| `XSetGraphicsExposures` | 2.17 | 8 |
| `XSetWindowBackground` | 2.11 | 14 |
| `CapRound` | 2.03 | 19 |
| `JoinRound` | 1.97 | 14 |

## All hacks

| Hack | Kind | Effort | Shim gaps | Flags | LOC |
| --- | --- | --- | --- | --- | --- |
| abstractile | 2d | L | `BlackPixelOfScreen`, `Convex`, `make_color_loop`, `make_color_ramp`, `rgb_to_hsv` | - | 1625 |
| anemone | 2d | L | `CapRound`, `JoinBevel`, `LineSolid`, `XCreatePixmap`, `XSetLineAttributes` | pixmaps | 458 |
| anemotaxis | 2d | L | `CapRound`, `JoinRound`, `LineSolid`, `XCreatePixmap`, `XSetLineAttributes` | pixmaps | 760 |
| ant | 2d | L | `CapNotLast`, `Convex`, `CoordModePrevious`, `JoinMiter`, `LineSolid`, `NUMSTIPPLES`, `XCreatePixmapFromBitmapData`, `XDrawArc`, `XSetLineAttributes`, `automata.h`, `hexagonUnit`, `triangleUnit` | needs-xlockmore | 1351 |
| apollonian | 2d | L | `FcChar8`, `XColor.color`, `XDrawArc`, `XGlyphInfo`, `XQueryColor`, `XftColor`, `XftColorAllocName`, `XftDraw`, `XftDrawCreate`, `XftDrawDestroy`, `XftDrawStringUtf8`, `XftFont`, `XftFontClose`, `XftTextExtentsUtf8`, `error: call to undeclared library function 'strlen' with type 'unsigned long (const char *)'; ISO C99 and later do not support implicit function declarations [-Wimplicit-function-declaration]`, `error: expected expression`, `load_xft_font_retry`, `overall` | needs-xlockmore | 820 |
| apple2-main | 2d | L | `A2CONTROLLER_DONE`, `A2CONTROLLER_FREE`, `A2_GR_FULL`, `A2_GR_HIRES`, `A2_GR_LORES`, `ANALOGTV_DEFAULTS`, `ANALOGTV_OPTIONS`, `DisplayOfScreen`, `GrayScale`, `KeyPress`, `PseudoColor`, `TTY_BLINK`, `TTY_BOLD`, `TTY_INVERSE`, `TTY_SYMBOLS`, `XCreatePixmap`, `XDestroyImage`, `XEvent.xany`, `XEvent.xkey`, `XGetImage`, `XGetPixel`, `XImage`, `XQueryColors`, `ZPixmap`, `a2_clear_gr`, `a2_clear_hgr`, `a2_cls`, `a2_display_image_loading`, `a2_goto`, `a2_hline`, `a2_hplot`, `a2_invalidate`, `a2_plot`, `a2_printc`, `a2_printc_noscroll`, `a2_prints`, `analogtv_reconfigure`, `ansi-tty.h`, `ansi_tty`, `ansi_tty_free`, `ansi_tty_init`, `ansi_tty_print`, `apple2.h`, `apple2_one_frame`, `apple2_sim_t`, `apple2_start`, `apple2_state_t`, `error: expected expression`, `error: incompatible integer to pointer conversion initializing 'Display *' (aka 'struct XshimDisplay *') with an expression of type 'int' [-Wint-conversion]`, `error: incompatible integer to pointer conversion initializing 'char *' with an expression of type 'int' [-Wint-conversion]`, `flag`, `image`, `load_image_async`, `sim`, `st`, `tc`, `text_data`, `textclient.h`, `textclient_close`, `textclient_getc`, `textclient_open`, `textclient_putc_event`, `textclient_puts`, `textclient_reshape`, `tty`, `tty_char`, `tty_flag`, `utf8_encode`, `utf8_to_latin1`, `utf8wc.h`, `visual_cells`, `visual_class`, `visual_rgb_masks` | pixmaps, readback | 1642 |
| attraction | 2d | L | `ButtonRelease`, `CapButt`, `CapRound`, `Convex`, `GCCapStyle`, `XEvent.x`, `XEvent.xany`, `XEvent.y`, `XGCValues.cap_style`, `XQueryPointer`, `compute_closed_spline`, `error: incompatible integer to pointer conversion assigning to 'int *' from 'int' [-Wint-conversion]`, `error: member reference base type 'int' is not a structure or union`, `error: type name does not allow function specifier to be specified`, `error: type specifier missing, defaults to 'int'; ISO C99 and later do not support implicit int [-Wimplicit-int]`, `free_spline`, `make_color_ramp`, `make_spline`, `spline` | - | 1115 |
| barcode | 2d | L | `ButtonRelease`, `LSBFirst`, `XCreateImage`, `XDestroyImage`, `XEvent.xany`, `XImage`, `XPutImage`, `XYBitmap` | - | 2055 |
| binaryhorizon | 2d | L | `KeyPress`, `XCreatePixmap`, `XDestroyImage`, `XEvent.xany`, `XGetImage`, `XImage`, `XPutImage`, `XPutPixel`, `ZPixmap`, `visual_depth` | pixmaps, readback | 624 |
| binaryring | 2d | L | `KeyPress`, `XCreatePixmap`, `XDestroyImage`, `XEvent.xany`, `XGetImage`, `XImage`, `XPutImage`, `XPutPixel`, `ZPixmap`, `visual_depth` | pixmaps, readback | 577 |
| blitspin | 2d | L | `GXand`, `GXclear`, `GXor`, `GXset`, `GXxor`, `XCreatePixmap`, `XDestroyImage`, `XDisplayHeight`, `XDisplayWidth`, `XGetImage`, `XPutImage`, `XScreenNumberOfScreen`, `async_load_state`, `file_to_pixmap`, `images/gen/som_png.h`, `load_image_async_simple`, `pow2.h`, `som_png`, `to_pow2` | pixmaps, readback, clipmask | 467 |
| bouboule | 2d | L | `GXor`, `XArc`, `XFillArcs`, `XSetFunction`, `arc`, `arcleft`, `error: expected expression`, `oarc`, `oarcleft` | xor, needs-xlockmore | 860 |
| boxfit | 2d | L | `XCreatePixmap`, `XDestroyImage`, `XDrawArc`, `XGetImage`, `XGetPixel`, `XImage`, `XSetWindowBackground`, `ZPixmap`, `async_load_state`, `load_image_async_simple` | pixmaps, readback | 573 |
| braid | 2d | L | `CapButt`, `CapNotLast`, `CapRound`, `JoinMiter`, `JoinRound`, `LineSolid`, `XSetLineAttributes`, `error: call to undeclared library function 'memset' with type 'void *(void *, int, unsigned long)'; ISO C99 and later do not support implicit function declarations [-Wimplicit-function-declaration]` | needs-xlockmore | 444 |
| bsod | 2d | L | `A2CONTROLLER_DONE`, `A2CONTROLLER_FREE`, `A2_GR_FULL`, `A2_GR_HIRES`, `A2_GR_LORES`, `ANALOGTV_DEFAULTS`, `ANALOGTV_OPTIONS`, `CapButt`, `CapRound`, `FcChar8`, `JoinMiter`, `LineSolid`, `XClearArea`, `XColor.color`, `XCreateImage`, `XCreatePixmap`, `XCreatePixmapFromBitmapData`, `XDestroyImage`, `XFetchName`, `XGetImage`, `XGetPixel`, `XGlyphInfo`, `XImage`, `XPutImage`, `XPutPixel`, `XQueryColor`, `XSetLineAttributes`, `XSetPlaneMask`, `XSetWindowBackground`, `XStoreName`, `XYPixmap`, `XftColor`, `XftDraw`, `XftDrawCreate`, `XftDrawDestroy`, `XftDrawStringUtf8`, `XftFont`, `XftFontClose`, `XftTextExtentsUtf8`, `XftTextExtentsUtf8_multi`, `ZPixmap`, `a2_cls`, `a2_goto`, `a2_init_memory_active`, `a2_invalidate`, `a2_poke`, `a2_printc`, `a2_printc_noscroll`, `a2_prints`, `amiga_png`, `android_png`, `apple2.h`, `apple2_one_frame`, `apple2_sim_t`, `apple2_start`, `apple2_state_t`, `apple_png`, `async_load_state`, `atari_png`, `atm_png`, `dvd_png`, `em`, `error: expected expression`, `font`, `gnome1_png`, `gnome2_png`, `hmac_png`, `i1`, `i2`, `images/gen/amiga_png.h`, `images/gen/android_png.h`, `images/gen/apple_png.h`, `images/gen/atari_png.h`, `images/gen/atm_png.h`, `images/gen/dvd_png.h`, `images/gen/gnome1_png.h`, `images/gen/gnome2_png.h`, `images/gen/hmac_png.h`, `images/gen/mac_png.h`, `images/gen/macbomb_png.h`, `images/gen/osx_10_2_png.h`, `images/gen/osx_10_3_png.h`, `images/gen/ransomware_png.h`, `images/gen/sun_png.h`, `load_image_async_simple`, `load_xft_font_retry`, `mac_png`, `macbomb_png`, `osx_10_2_png`, `osx_10_3_png`, `ov`, `ov2`, `ransomware_png`, `screen_number`, `sim`, `st`, `sun_png`, `utf8_decode_combining`, `utf8wc.h`, `xft.h`, `xft_word_wrap`, `xftwrap.h` | pixmaps, readback, clipmask | 7809 |
| bubbles | 2d | L | `BUBBLE_MAGIC`, `Bubble`, `Bubble_Step`, `DELETE_BUBBLE`, `KEEP_BUBBLE`, `MAX`, `MAX_DROPPAGE`, `MIN`, `XDrawArc`, `bubbles.h`, `default_bubbles`, `error: expected expression`, `head`, `init_default_bubbles`, `least`, `newpix`, `nextbub`, `num_default_bubbles`, `pixmap_list`, `rv`, `tmp`, `tmppix`, `touch` | pixmaps, clipmask | 1468 |
| bumps | 2d | L | `XCreatePixmap`, `XDestroyImage`, `XGetImage`, `XGetPixel`, `XImage`, `XParseColor`, `XQueryColors`, `XSetWindowBackground`, `XShmSegmentInfo`, `ZPixmap`, `async_load_state`, `create_xshm_image`, `destroy_xshm_image`, `load_image_async_simple`, `pScreenImage`, `put_xshm_image`, `xshm.h` | pixmaps, readback | 705 |
| ccurve | 2d | L | `XCreatePixmap`, `make_color_loop` | pixmaps | 872 |
| celtic | 2d | L | `CapRound`, `GCCapStyle`, `JoinRound`, `LineSolid`, `XDrawArc`, `XGCValues.cap_style`, `XSetLineAttributes` | - | 1141 |
| compass | 2d | L | `Convex`, `GCJoinStyle`, `JoinBevel`, `XCreatePixmap`, `XDrawSegments`, `XGCValues.join_style`, `XSegment`, `segs` | pixmaps, float-heavy | 999 |
| crystal | 2d | L | `Convex`, `GXxor`, `XCreateColormap`, `XFreeColormap`, `XInstallColormap`, `XParseColor`, `XSetFunction`, `XSetWindowColormap`, `free_colors`, `has_writable_cells`, `make_random_colormap`, `make_smooth_colormap`, `make_uniform_colormap`, `rotate_colors` | xor, float-heavy, needs-xlockmore | 1286 |
| cwaves | 2d | L | `BlackPixelOfScreen`, `CapRound`, `JoinRound`, `LineSolid`, `XSetLineAttributes` | - | 219 |
| decayscreen | 2d | L | `XCreatePixmap`, `async_load_state`, `load_image_async_simple` | pixmaps | 392 |
| deco | 2d | L | `CapButt`, `DisplayOfScreen`, `JoinBevel`, `LineSolid`, `XSetLineAttributes`, `XStoreColors`, `allocate_writable_colors`, `error: incompatible integer to pointer conversion initializing 'Display *' (aka 'struct XshimDisplay *') with an expression of type 'int' [-Wint-conversion]`, `has_writable_cells` | - | 345 |
| deluxe | 2d | L | `CapProjecting`, `GCCapStyle`, `GCJoinStyle`, `GCPlaneMask`, `JoinMiter`, `XCreatePixmap`, `XDrawArc`, `XGCValues.cap_style`, `XGCValues.join_style`, `XGCValues.plane_mask`, `allocate_alpha_colors`, `alpha.h` | pixmaps, float-heavy | 480 |
| demon | 2d | L | `Convex`, `CoordModePrevious`, `FillOpaqueStippled`, `GCFillStyle`, `GCStipple`, `NUMSTIPPLES`, `STIPPLESIZE`, `XCreatePixmapFromBitmapData`, `XGCValues.fill_style`, `XGCValues.stipple`, `automata.h`, `error: call to undeclared library function 'memcpy' with type 'void *(void *, const void *, unsigned long)'; ISO C99 and later do not support implicit function declarations [-Wimplicit-function-declaration]`, `hexagonUnit`, `stipples`, `triangleUnit` | needs-xlockmore | 953 |
| distort | 2d | L | `BlackPixelOfScreen`, `XCreatePixmap`, `XDestroyImage`, `XGetImage`, `XGetPixel`, `XImage`, `XPutImage`, `XPutPixel`, `XShmSegmentInfo`, `ZPixmap`, `async_load_state`, `create_xshm_image`, `destroy_xshm_image`, `load_image_async_simple`, `put_xshm_image`, `xshm.h` | pixmaps, readback | 894 |
| droste | 2d | L | `BlackPixelOfScreen`, `GET_PARENT_OBJ`, `KeyPress`, `KeySym`, `THREAD_DEFAULTS`, `THREAD_OPTIONS`, `XCreatePixmap`, `XDestroyImage`, `XEvent.xkey`, `XGetImage`, `XGetPixel`, `XImage`, `XK_Down`, `XK_Left`, `XK_Right`, `XK_Up`, `XLookupString`, `XPutPixel`, `XShmSegmentInfo`, `ZPixmap`, `async_load_state`, `create_xshm_image`, `destroy_xshm_image`, `double_time`, `doubletime.h`, `error: expected expression`, `error: field has incomplete type 'struct threadpool'`, `error: variable has incomplete type 'const struct threadpool_class'`, `hardware_concurrency`, `i_log2_fast`, `keysym`, `load_image_async_simple`, `pow2.h`, `put_xshm_image`, `thread_util.h`, `threadpool`, `threadpool_create`, `threadpool_destroy`, `threadpool_run`, `threadpool_wait`, `xshm.h` | pixmaps, readback | 686 |
| epicycle | 2d | L | `CapRound`, `GCCapStyle`, `GCJoinStyle`, `JoinRound`, `XGCValues.cap_style`, `XGCValues.join_style` | - | 803 |
| eruption | 2d | L | `XEvent.x`, `XEvent.y`, `XImage`, `XPutPixel`, `XSetWindowBackground`, `XShmSegmentInfo`, `ZPixmap`, `create_xshm_image`, `destroy_xshm_image`, `error: typedef redefinition with different types ('unsigned long' vs 'unsigned int')`, `img`, `put_xshm_image`, `xshm.h` | - | 608 |
| euler2d | 2d | L | `CapRound`, `JoinRound`, `LineSolid`, `XDrawArc`, `XDrawSegments`, `XSegment`, `XSetLineAttributes`, `error: call to undeclared library function 'memcpy' with type 'void *(void *, const void *, unsigned long)'; ISO C99 and later do not support implicit function declarations [-Wimplicit-function-declaration]`, `error: call to undeclared library function 'memset' with type 'void *(void *, int, unsigned long)'; ISO C99 and later do not support implicit function declarations [-Wimplicit-function-declaration]`, `error: expected expression` | float-heavy, needs-xlockmore | 893 |
| fiberlamp | 2d | L | `CapNotLast`, `JoinMiter`, `LineSolid`, `RootWindow`, `XAllocNamedColor`, `XCreatePixmap`, `XSetGraphicsExposures`, `XSetLineAttributes`, `XTranslateCoordinates` | pixmaps, needs-xlockmore | 480 |
| filmleader | 2d | L | `ANALOGTV_DEFAULTS`, `ANALOGTV_OPTIONS`, `ANALOGTV_SIGNAL_LEN`, `ButtonRelease`, `CapRound`, `FcChar8`, `JoinRound`, `KeyPress`, `KeySym`, `LineSolid`, `XCreateImage`, `XCreatePixmap`, `XDestroyImage`, `XDrawArc`, `XEvent.xany`, `XEvent.xkey`, `XGetImage`, `XGetPixel`, `XGlyphInfo`, `XImage`, `XLookupString`, `XPutImage`, `XPutPixel`, `XSetLineAttributes`, `XftColor`, `XftColorAllocName`, `XftColorFree`, `XftDraw`, `XftDrawCreate`, `XftDrawDestroy`, `XftDrawStringUtf8`, `XftFont`, `XftTextExtentsUtf8`, `ZPixmap`, `analogtv`, `analogtv.h`, `analogtv_allocate`, `analogtv_draw`, `analogtv_input`, `analogtv_input_allocate`, `analogtv_load_ximage`, `analogtv_reception`, `analogtv_reception_update`, `analogtv_reconfigure`, `analogtv_release`, `analogtv_set_defaults`, `analogtv_setup_sync`, `double_time`, `doubletime.h`, `error: expected expression`, `extents`, `img`, `img1`, `img2`, `keysym`, `load_xft_font_retry`, `screen_number`, `xftfont` | pixmaps, readback | 548 |
| fireworkx | 2d | L | `ButtonRelease`, `ImageByteOrder`, `MSBFirst`, `XCreateImage`, `XDestroyImage`, `XEvent.x`, `XEvent.y`, `XImage`, `XPutImage`, `ZPixmap` | - | 882 |
| flag | 2d | L | `GCFont`, `XCharStruct`, `XCreateImage`, `XCreatePixmap`, `XDestroyImage`, `XDrawString`, `XFontStruct`, `XFreeFont`, `XGCValues.font`, `XGetImage`, `XGetPixel`, `XImage`, `XLoadQueryFont`, `XPutPixel`, `XSetGraphicsExposures`, `XTextExtents`, `XYBitmap`, `XYPixmap`, `ZPixmap`, `bob_png`, `error: call to undeclared library function 'memset' with type 'void *(void *, int, unsigned long)'; ISO C99 and later do not support implicit function declarations [-Wimplicit-function-declaration]`, `error: call to undeclared library function 'strcmp' with type 'int (const char *, const char *)'; ISO C99 and later do not support implicit function declarations [-Wimplicit-function-declaration]`, `error: call to undeclared library function 'strdup' with type 'char *(const char *)'; ISO C99 and later do not support implicit function declarations [-Wimplicit-function-declaration]`, `error: call to undeclared library function 'strlen' with type 'unsigned long (const char *)'; ISO C99 and later do not support implicit function declarations [-Wimplicit-function-declaration]`, `error: call to undeclared library function 'strtok' with type 'char *(char *, const char *)'; ISO C99 and later do not support implicit function declarations [-Wimplicit-function-declaration]`, `file_to_pixmap`, `font`, `im`, `image_data_to_ximage`, `images/gen/bob_png.h`, `o2`, `overall` | pixmaps, readback, text, needs-xlockmore | 570 |
| flow | 2d | L | `CapNotLast`, `JoinMiter`, `LineSolid`, `XCreatePixmap`, `XDrawSegments`, `XSegment`, `XSetGraphicsExposures`, `XSetLineAttributes`, `error: call to undeclared library function 'memcpy' with type 'void *(void *, const void *, unsigned long)'; ISO C99 and later do not support implicit function declarations [-Wimplicit-function-declaration]`, `error: call to undeclared library function 'memmove' with type 'void *(void *, const void *, unsigned long)'; ISO C99 and later do not support implicit function declarations [-Wimplicit-function-declaration]`, `error: call to undeclared library function 'memset' with type 'void *(void *, int, unsigned long)'; ISO C99 and later do not support implicit function declarations [-Wimplicit-function-declaration]`, `error: expected expression` | pixmaps, needs-xlockmore | 1216 |
| fluidballs | 2d | L | `ButtonRelease`, `FcChar8`, `RootWindow`, `XCreatePixmap`, `XEvent.x`, `XEvent.xany`, `XEvent.y`, `XQueryPointer`, `XTranslateCoordinates`, `XftColor`, `XftColorAllocName`, `XftDraw`, `XftDrawCreate`, `XftDrawDestroy`, `XftDrawStringUtf8`, `XftFont`, `XftFontClose`, `error: expected expression`, `error: too few arguments to function call, expected 2, have 1`, `load_xft_font_retry`, `screen_number` | pixmaps | 881 |
| fontglide | 2d | L | `BlackPixelOfScreen`, `DisplayOfScreen`, `FcChar8`, `XCreateImage`, `XCreatePixmap`, `XDestroyImage`, `XDrawString`, `XDrawString16`, `XFreeFont`, `XGetAtomName`, `XGetImage`, `XGetPixel`, `XGlyphInfo`, `XImage`, `XLoadQueryFont`, `XLookupString`, `XPutImage`, `XPutPixel`, `XRenderColor`, `XSetFont`, `XTextExtents`, `XTextExtents16`, `XYPixmap`, `XftColor`, `XftColorAllocValue`, `XftColorFree`, `XftDraw`, `XftDrawCreate`, `XftDrawDestroy`, `XftDrawStringUtf8`, `XftFont`, `XftFontClose`, `XftTextExtentsUtf8`, `ZPixmap`, `bg`, `error: Xft is required under X11`, `error: expected expression`, `error: incompatible integer to pointer conversion initializing 'Display *' (aka 'struct XshimDisplay *') with an expression of type 'int' [-Wint-conversion]`, `extents`, `fg`, `in`, `load_xft_font_retry`, `out`, `screen_number`, `swap`, `text_data`, `textclient.h`, `textclient_close`, `textclient_getc`, `textclient_open`, `utf8_decode_combining`, `utf8wc.h`, `xftdraw` | pixmaps, readback, text, clipmask | 2474 |
| forest | 2d | L | `CapButt`, `JoinMiter`, `LineSolid`, `XArc`, `XFillArcs`, `XSetLineAttributes`, `error: subscripted value is not an array, pointer, or vector`, `leaf` | needs-xlockmore | 241 |
| fuzzyflakes | 2d | L | `CapProjecting`, `GCCapStyle`, `GCJoinStyle`, `JoinMiter`, `XCreatePixmap`, `XGCValues.cap_style`, `XGCValues.join_style`, `XParseColor` | pixmaps | 655 |
| glitchpeg | 2d | L | `BitmapBitOrder`, `ButtonRelease`, `ImageByteOrder`, `XCreateImage`, `XDestroyImage`, `XEvent.xany`, `XGetPixel`, `XImage`, `XPutImage`, `XPutPixel`, `XtAppAddInput`, `XtDisplayToApplicationContext`, `XtInputExceptMask`, `XtInputId`, `XtInputReadMask`, `XtPointer`, `XtRemoveInput`, `ZPixmap`, `error: operand of type 'XPoint' where arithmetic or pointer type is required`, `image`, `image_data_to_ximage`, `out` | readback | 466 |
| goop | 2d | L | `AllPlanes`, `DefaultScreenOfDisplay`, `DisplayOfScreen`, `GXclear`, `GXxor`, `Nonconvex`, `WhitePixelOfScreen`, `XCreatePixmap`, `XSetFunction`, `XSetPlaneMask`, `allocate_alpha_colors`, `alpha.h`, `compute_closed_spline`, `error: incompatible integer to pointer conversion assigning to 'int *' from 'int' [-Wint-conversion]`, `error: incompatible integer to pointer conversion initializing 'Display *' (aka 'struct XshimDisplay *') with an expression of type 'int' [-Wint-conversion]`, `error: member reference base type 'int' is not a structure or union`, `error: type name does not allow function specifier to be specified`, `error: type specifier missing, defaults to 'int'; ISO C99 and later do not support implicit int [-Wimplicit-int]`, `free_spline`, `has_writable_cells`, `make_spline`, `spline` | pixmaps, xor | 651 |
| greynetic | 2d | L | `FillOpaqueStippled`, `GCFillStyle`, `GCStipple`, `XCreatePixmapFromBitmapData`, `XGCValues.fill_style`, `XGCValues.stipple` | - | 297 |
| halftone | 2d | L | `XCreatePixmap` | pixmaps | 413 |
| halo | 2d | L | `GXxor`, `XCreatePixmap` | pixmaps | 459 |
| ifs | 2d | L | `XCreatePixmap` | pixmaps | 560 |
| imsmap | 2d | L | `XCreateImage`, `XDestroyImage`, `XImage`, `XPutImage`, `XPutPixel`, `XYBitmap`, `image` | - | 426 |
| interference | 2d | L | `GET_PARENT_OBJ`, `THREAD_DEFAULTS`, `THREAD_OPTIONS`, `XCreatePixmap`, `XImage`, `XPutPixel`, `XShmGetEventBase`, `XShmSegmentInfo`, `ZPixmap`, `create_xshm_image`, `destroy_xshm_image`, `error: expected expression`, `error: field has incomplete type 'struct threadpool'`, `error: too few arguments to function call, expected 2, have 1`, `error: variable has incomplete type 'const struct threadpool_class'`, `hardware_concurrency`, `make_color_loop`, `put_xshm_image`, `thread_memory_alignment`, `thread_util.h`, `threadpool`, `threadpool_create`, `threadpool_destroy`, `threadpool_run`, `threadpool_wait`, `visual_pixmap_depth`, `xshm.h` | pixmaps | 1002 |
| intermomentary | 2d | L | `XCreatePixmap`, `XQueryColor`, `XSetFillStyle`, `XSetTile`, `make_color_ramp`, `rgb_to_hsv` | pixmaps | 605 |
| juggle | 2d | L | `CapRound`, `Convex`, `JoinRound`, `LineSolid`, `XDrawArc`, `XDrawImageString`, `XDrawString`, `XFontStruct`, `XFreeFontInfo`, `XLoadQueryFont`, `XSetLineAttributes`, `XTextWidth`, `error: call to undeclared library function 'strcasecmp' with type 'int (const char *, const char *)'; ISO C99 and later do not support implicit function declarations [-Wimplicit-function-declaration]`, `error: call to undeclared library function 'strcmp' with type 'int (const char *, const char *)'; ISO C99 and later do not support implicit function declarations [-Wimplicit-function-declaration]`, `error: call to undeclared library function 'strdup' with type 'char *(const char *)'; ISO C99 and later do not support implicit function declarations [-Wimplicit-function-declaration]`, `error: call to undeclared library function 'strlen' with type 'unsigned long (const char *)'; ISO C99 and later do not support implicit function declarations [-Wimplicit-function-declaration]`, `gettimeofday` | text, float-heavy, needs-xlockmore | 2798 |
| julia | 2d | L | `Button1`, `ButtonRelease`, `Cursor`, `FillOpaqueStippled`, `MotionNotify`, `XCreatePixmap`, `XCreatePixmapCursor`, `XCreatePixmapFromBitmapData`, `XDefineCursor`, `XDrawArc`, `XEvent.x`, `XEvent.xany`, `XEvent.xmotion`, `XEvent.y`, `XFreeCursor`, `XSetFillStyle`, `XSetStipple`, `XSetTSOrigin`, `XUndefineCursor`, `error: call to undeclared library function 'memset' with type 'void *(void *, int, unsigned long)'; ISO C99 and later do not support implicit function declarations [-Wimplicit-function-declaration]` | pixmaps, float-heavy, needs-xlockmore | 451 |
| kaleidescope | 2d | L | `CapRound`, `GCCapStyle`, `JoinRound`, `LineSolid`, `XDrawSegments`, `XGCValues.cap_style`, `XSegment`, `XSetLineAttributes`, `error: expected expression` | - | 514 |
| kumppa | 2d | L | `XSetGraphicsExposures` | pixmaps | 545 |
| lcdscrub | 2d | L | `XCreateImage`, `XCreatePixmap`, `XDestroyImage`, `XGetPixel`, `XImage`, `XPutImage`, `XPutPixel`, `XYPixmap`, `error: too few arguments to function call, expected 2, have 1` | pixmaps, readback, clipmask | 399 |
| lisa | 2d | L | `CapNotLast`, `JoinBevel`, `LineSolid`, `XMaxRequestSize`, `XSetLineAttributes` | needs-xlockmore | 744 |
| lmorph | 2d | L | `CapButt`, `CapRound`, `JoinBevel`, `JoinRound`, `LineSolid`, `XSetLineAttributes` | float-heavy | 580 |
| loop | 2d | L | `Convex`, `CoordModePrevious`, `FillOpaqueStippled`, `GCFillStyle`, `GCStipple`, `STIPPLESIZE`, `XCreatePixmapFromBitmapData`, `XGCValues.fill_style`, `XGCValues.stipple`, `automata.h`, `hexagonUnit`, `stipples`, `triangleUnit` | needs-xlockmore | 1700 |
| m6502 | 2d | L | `ANALOGTV_BLACK_LEVEL`, `ANALOGTV_BOT`, `ANALOGTV_DEFAULTS`, `ANALOGTV_OPTIONS`, `ANALOGTV_TOP`, `ANALOGTV_VISLINES`, `ANALOGTV_VIS_END`, `ANALOGTV_VIS_LEN`, `ANALOGTV_VIS_START`, `ANALOGTV_WHITE_LEVEL`, `Bit8`, `analogtv`, `analogtv.h`, `analogtv_allocate`, `analogtv_draw`, `analogtv_draw_solid`, `analogtv_input`, `analogtv_input_allocate`, `analogtv_lcp_to_ntsc`, `analogtv_reception`, `analogtv_reception_update`, `analogtv_reconfigure`, `analogtv_release`, `analogtv_set_defaults`, `analogtv_setup_sync`, `asm6502.h`, `double_time`, `doubletime.h`, `m6502.h`, `m6502_build`, `m6502_destroy6502`, `m6502_next_eval`, `m6502_start_eval_file`, `m6502_start_eval_string`, `machine_6502` | - | 288 |
| marbling | 2d | L | `DefaultScreenOfDisplay`, `GET_PARENT_OBJ`, `KeyPress`, `KeySym`, `THREAD_DEFAULTS`, `THREAD_OPTIONS`, `XEvent.xany`, `XEvent.xkey`, `XImage`, `XK_Down`, `XK_Left`, `XK_Right`, `XK_Up`, `XLookupString`, `XPutPixel`, `XShmSegmentInfo`, `ZPixmap`, `create_xshm_image`, `destroy_xshm_image`, `error: expected expression`, `error: field has incomplete type 'struct threadpool'`, `error: incompatible integer to pointer conversion passing 'int' to parameter of type 'Screen *' (aka 'struct XshimScreen *') [-Wint-conversion]`, `error: variable has incomplete type 'const struct threadpool_class'`, `hardware_concurrency`, `keysym`, `put_xshm_image`, `thread_memory_alignment`, `thread_util.h`, `threadpool_create`, `threadpool_destroy`, `threadpool_run`, `threadpool_wait`, `visual_pixmap_depth`, `xshm.h` | - | 635 |
| maze | 2d | L | `XSetLineAttributes` | pixmaps, clipmask | 1681 |
| memscroller | 2d | L | `FcChar8`, `XGlyphInfo`, `XImage`, `XShmSegmentInfo`, `XftColor`, `XftColorAllocName`, `XftDraw`, `XftDrawCreate`, `XftDrawDestroy`, `XftDrawStringUtf8`, `XftFont`, `XftFontClose`, `XftTextExtentsUtf8`, `ZPixmap`, `create_xshm_image`, `destroy_xshm_image`, `error: expected expression`, `load_xft_font_retry`, `overall`, `put_xshm_image`, `screen_number`, `xft.h`, `xshm.h` | pixmaps | 626 |
| metaballs | 2d | L | `BitmapPad`, `XCreateImage`, `XDestroyImage`, `XFree`, `XImage`, `XListPixmapFormats`, `XParseColor`, `XPutImage`, `XPutPixel`, `XSetWindowBackground`, `ZPixmap` | - | 438 |
| moire | 2d | L | `BlackPixelOfScreen`, `DefaultScreenOfDisplay`, `WhitePixelOfScreen`, `XImage`, `XPutPixel`, `XQueryColor`, `XShmSegmentInfo`, `ZPixmap`, `create_xshm_image`, `destroy_xshm_image`, `make_color_ramp`, `put_xshm_image`, `rgb_to_hsv`, `visual_depth`, `xshm.h` | - | 253 |
| moire2 | 2d | L | `GXor`, `GXxor`, `XCreatePixmap`, `XDrawArc`, `XSetFunction` | pixmaps, xor | 363 |
| nerverot | 2d | L | `XCreatePixmap`, `make_color_ramp`, `rgb_to_hsv` | pixmaps, float-heavy | 1367 |
| noseguy | 2d | L | `FcChar8`, `XCreateImage`, `XCreatePixmap`, `XDestroyImage`, `XGetImage`, `XGetPixel`, `XGlyphInfo`, `XImage`, `XPutImage`, `XPutPixel`, `XSetLineAttributes`, `XYPixmap`, `XftColor`, `XftColorAllocName`, `XftDraw`, `XftDrawCreate`, `XftDrawDestroy`, `XftDrawStringUtf8`, `XftFont`, `XftFontClose`, `XftTextExtentsUtf8`, `ZPixmap`, `error: expected expression`, `extents`, `i1`, `i2`, `images/gen/nose-f1_png.h`, `images/gen/nose-f2_png.h`, `images/gen/nose-f3_png.h`, `images/gen/nose-f4_png.h`, `images/gen/nose-l1_png.h`, `images/gen/nose-l2_png.h`, `images/gen/nose-r1_png.h`, `images/gen/nose-r2_png.h`, `load_xft_font_retry`, `nose_f1_png`, `nose_f2_png`, `nose_f3_png`, `nose_f4_png`, `nose_l1_png`, `nose_l2_png`, `nose_r1_png`, `nose_r2_png`, `screen_number`, `text_data`, `textclient.h`, `textclient_close`, `textclient_getc`, `textclient_open`, `textclient_reshape` | pixmaps, readback, clipmask | 720 |
| pacman | 2d | L | `BLUE`, `CapRound`, `FillSolid`, `GHOSTS`, `GHOST_DANGER`, `JAILHEIGHT`, `JoinMiter`, `LEVHEIGHT`, `LEVWIDTH`, `LineSolid`, `MAXGDIR`, `MAXGFLASH`, `MAXGWAG`, `MAXMOUTH`, `MINGRIDSIZE`, `MINSIZE`, `NOWHERE`, `NUM_BONUS_DOTS`, `PAC_DEATH_FRAMES`, `START`, `XCreatePixmap`, `XDrawArc`, `XDrawString`, `XLoadQueryFont`, `XSetFillStyle`, `XSetLineAttributes`, `chasing`, `error: expected expression`, `error: invalid application of 'sizeof' to an incomplete type 'argtype[]'`, `ghoststruct`, `goingin`, `goingout`, `hiding`, `images/gen/pacman_png.h`, `inbox`, `pacman.h`, `pacman_ai.h`, `pacman_bonus_dot_eaten`, `pacman_bonus_dot_pos`, `pacman_createnewlevel`, `pacman_eat_bonus_dot`, `pacman_ghost_update`, `pacman_is_bonus_dot`, `pacman_level.h`, `pacman_png`, `pacman_trackmouse`, `pacman_update`, `pacmangamestruct`, `pp`, `ps_chasing`, `ps_dieing`, `ps_eating` | pixmaps, text, clipmask, needs-xlockmore | 1479 |
| penetrate | 2d | L | `FcChar8`, `XDrawArc`, `XGlyphInfo`, `XSetLineAttributes`, `XftColor`, `XftColorAllocName`, `XftDraw`, `XftDrawCreate`, `XftDrawDestroy`, `XftDrawStringUtf8`, `XftFont`, `XftFontClose`, `XftTextExtentsUtf8`, `error: expected expression`, `load_xft_font_retry`, `overall`, `screen_number`, `usleep` | - | 1037 |
| penrose | 2d | L | `CapNotLast`, `Convex`, `JoinMiter`, `LineOnOffDash`, `LineSolid`, `XSetLineAttributes`, `error: call to undeclared library function 'memcmp' with type 'int (const void *, const void *, unsigned long)'; ISO C99 and later do not support implicit function declarations [-Wimplicit-function-declaration]` | needs-xlockmore | 1342 |
| phosphor | 2d | L | `BlackPixelOfScreen`, `CapRound`, `DefaultScreenOfDisplay`, `FALSE`, `FcChar8`, `GCCapStyle`, `KeyPress`, `TTY_BOLD`, `TTY_INVERSE`, `TTY_ITALIC`, `TTY_SYMBOLS`, `Time`, `XCreateImage`, `XCreatePixmap`, `XDestroyImage`, `XEvent.xany`, `XEvent.xkey`, `XGCValues.cap_style`, `XGetImage`, `XGetPixel`, `XGlyphInfo`, `XImage`, `XPutImage`, `XPutPixel`, `XQueryColor`, `XWriteBitmapFile`, `XYBitmap`, `XYPixmap`, `XftColor`, `XftDraw`, `XftDrawCreate`, `XftDrawStringUtf8`, `XftFont`, `XftTextExtentsUtf8`, `XtAppAddTimeOut`, `XtAppContext`, `XtIntervalId`, `XtPointer`, `XtRemoveTimeOut`, `ZPixmap`, `_6x10font_png`, `ansi-tty.h`, `ansi_graphics_unicode`, `ansi_tty`, `ansi_tty_free`, `ansi_tty_init`, `ansi_tty_print`, `ansi_tty_resize`, `app`, `error: expected expression`, `font`, `font_bits`, `im`, `im2`, `images/gen/6x10font_png.h`, `load_xft_font_retry`, `make_color_ramp`, `mm`, `overall`, `rgb_to_hsv`, `screen_number`, `tcell`, `text_data`, `textclient.h`, `textclient_close`, `textclient_getc`, `textclient_open`, `textclient_putc_event`, `textclient_puts`, `textclient_reshape`, `tty`, `tty_char`, `utf8_encode`, `utf8_to_latin1`, `utf8wc.h`, `xft_fg`, `xftdraw`, `xim_color`, `xim_mono` | pixmaps, readback, clipmask | 1260 |
| piecewise | 2d | L | `XArc`, `XCreatePixmap`, `XDrawArcs`, `make_color_loop` | pixmaps | 1036 |
| polyominoes | 2d | L | `CapRound`, `JoinRound`, `LSBFirst`, `LineSolid`, `MSBFirst`, `XCreateImage`, `XDestroyImage`, `XDrawSegments`, `XImage`, `XPutImage`, `XSegment`, `XSetLineAttributes`, `XYBitmap`, `countof`, `error: call to undeclared library function 'memset' with type 'void *(void *, int, unsigned long)'; ISO C99 and later do not support implicit function declarations [-Wimplicit-function-declaration]`, `error: expected expression` | needs-xlockmore | 2370 |
| pong | 2d | L | `ANALOGTV_BLACK_LEVEL`, `ANALOGTV_BOT`, `ANALOGTV_DEFAULTS`, `ANALOGTV_OPTIONS`, `ANALOGTV_TOP`, `ANALOGTV_VISLINES`, `ANALOGTV_VIS_END`, `ANALOGTV_VIS_LEN`, `ANALOGTV_VIS_START`, `ButtonPressMask`, `ButtonRelease`, `ButtonReleaseMask`, `CurrentTime`, `Cursor`, `FocusChangeMask`, `FocusIn`, `FocusOut`, `GrabModeAsync`, `KeyPress`, `KeyPressMask`, `KeyRelease`, `KeyReleaseMask`, `KeySym`, `MotionNotify`, `X11/keysym.h`, `XCreatePixmap`, `XCreatePixmapCursor`, `XDefineCursor`, `XDestroyImage`, `XEvent.x`, `XEvent.xkey`, `XEvent.xmotion`, `XGrabPointer`, `XHeightMMOfScreen`, `XHeightOfScreen`, `XK_Down`, `XK_Up`, `XLookupString`, `XQueryPointer`, `XUngrabPointer`, `XWarpPointer`, `analogtv`, `analogtv.h`, `analogtv_allocate`, `analogtv_draw`, `analogtv_draw_solid`, `analogtv_draw_string`, `analogtv_font`, `analogtv_font_set_char`, `analogtv_input`, `analogtv_input_allocate`, `analogtv_lcp_to_ntsc`, `analogtv_make_font`, `analogtv_reception`, `analogtv_reception_update`, `analogtv_reconfigure`, `analogtv_release`, `analogtv_set_defaults`, `analogtv_setup_sync`, `double_time`, `doubletime.h`, `key` | pixmaps | 1109 |
| popsquares | 2d | L | `XCreatePixmap`, `XQueryColor`, `make_color_ramp`, `rgb_to_hsv` | pixmaps | 310 |
| qix | 2d | L | `CellsOfScreen`, `DefaultScreenOfDisplay`, `GCPlaneMask`, `GXxor`, `XGCValues.plane_mask`, `XQueryColor`, `XSetWindowBackground`, `allocate_alpha_colors`, `alpha.h`, `has_writable_cells`, `rgb_to_hsv` | - | 642 |
| rdbomb | 2d | L | `DefaultScreenOfDisplay`, `XImage`, `XListPixmapFormats`, `XPixmapFormatValues`, `XSetWindowBackground`, `XShmSegmentInfo`, `ZPixmap`, `create_xshm_image`, `destroy_xshm_image`, `error: expected ')'`, `error: expected function body after function declarator`, `error: expected parameter declarator`, `error: incompatible integer to pointer conversion passing 'int' to parameter of type 'void *' [-Wint-conversion]`, `error: subscripted value is not an array, pointer, or vector`, `has_writable_cells`, `pfv`, `put_xshm_image`, `visual_depth`, `xshm.h` | - | 571 |
| ripples | 2d | L | `XDestroyImage`, `XGetImage`, `XGetPixel`, `XImage`, `XPutPixel`, `XQueryColor`, `XShmSegmentInfo`, `ZPixmap`, `async_load_state`, `create_xshm_image`, `destroy_xshm_image`, `load_image_async_simple`, `put_xshm_image`, `visual_rgb_masks`, `xshm.h` | readback | 1127 |
| rocks | 2d | L | `Nonconvex`, `XCreatePixmap`, `XQueryColor`, `XSetGraphicsExposures` | pixmaps | 562 |
| rotzoomer | 2d | L | `XCreatePixmap`, `XDestroyImage`, `XGetImage`, `XGetPixel`, `XImage`, `XPutPixel`, `XShmSegmentInfo`, `ZPixmap`, `async_load_state`, `create_xshm_image`, `destroy_xshm_image`, `load_image_async_simple`, `put_xshm_image`, `xshm.h` | pixmaps, readback | 601 |
| shadebobs | 2d | L | `BlackPixelOfScreen`, `XCreateImage`, `XDestroyImage`, `XFree`, `XGetPixel`, `XImage`, `XListPixmapFormats`, `XParseColor`, `XPutImage`, `XPutPixel`, `XSetWindowBackground`, `ZPixmap` | readback | 474 |
| slidescreen | 2d | L | `Convex`, `XFree`, `XParseColor`, `XQueryColors`, `async_load_state`, `load_image_async_simple`, `visual_cells` | pixmaps | 507 |
| slip | 2d | L | `DisplayOfScreen`, `ScreenOfDisplay`, `XCreatePixmap`, `XSetGraphicsExposures`, `error: incompatible integer to pointer conversion initializing 'Display *' (aka 'struct XshimDisplay *') with an expression of type 'int' [-Wint-conversion]`, `load_image_async` | pixmaps, needs-xlockmore | 376 |
| speedmine | 2d | L | `Nonconvex`, `XCreatePixmap`, `XQueryColor`, `error: too few arguments to function call, expected 2, have 1`, `make_color_ramp`, `rgb_to_hsv` | pixmaps, clipmask | 1659 |
| spotlight | 2d | L | `XCreatePixmap`, `async_load_state`, `error: too few arguments to function call, expected 2, have 1`, `load_image_async_simple` | pixmaps, clipmask | 355 |
| starfish | 2d | L | `EvenOddRule`, `GCFillRule`, `XGCValues.fill_rule`, `XSetWindowBackground`, `compute_closed_spline`, `error: incompatible integer to pointer conversion assigning to 'int *' from 'int' [-Wint-conversion]`, `error: member reference base type 'int' is not a structure or union`, `error: type name does not allow function specifier to be specified`, `error: type specifier missing, defaults to 'int'; ISO C99 and later do not support implicit int [-Wimplicit-int]`, `make_spline`, `spline` | - | 564 |
| strange | 2d | L | `GCGraphicsExposures`, `GET_PARENT_OBJ`, `MSBFirst`, `StaticColor`, `THREAD_OPTIONS`, `TrueColor`, `XCreatePixmap`, `XGCValues.graphics_exposures`, `XImage`, `XPutPixel`, `XQueryColor`, `XQueryColors`, `XSetFunction`, `XSetGraphicsExposures`, `XShmSegmentInfo`, `ZPixmap`, `aligned_free`, `aligned_malloc`, `create_xshm_image`, `destroy_xshm_image`, `error: call to undeclared library function 'memset' with type 'void *(void *, int, unsigned long)'; ISO C99 and later do not support implicit function declarations [-Wimplicit-function-declaration]`, `error: field has incomplete type 'struct threadpool'`, `error: incomplete definition of type 'struct threadpool'`, `error: invalid application of 'sizeof' to an incomplete type 'XrmOptionDescRec[]'`, `error: unexpected type name 'ATTRACTOR': expected expression`, `error: variable has incomplete type 'const struct threadpool_class'`, `hardware_concurrency`, `i_log2`, `pow2.h`, `put_xshm_image`, `thread_memory_alignment`, `thread_util.h`, `threadpool_create`, `threadpool_destroy`, `threadpool_run`, `threadpool_wait`, `visual_class`, `visual_pixmap_depth`, `visual_rgb_masks`, `xshm.h` | pixmaps, xor, needs-xlockmore | 1353 |
| swirl | 2d | L | `XCreateColormap`, `XFree`, `XFreeColormap`, `XImage`, `XInstallColormap`, `XPutPixel`, `XQueryColor`, `XSetWMColormapWindows`, `XSetWindowColormap`, `XShmSegmentInfo`, `XStoreColors`, `ZPixmap`, `create_xshm_image`, `destroy_xshm_image`, `free_colors`, `make_smooth_colormap`, `put_xshm_image`, `rotate_colors`, `xshm.h` | needs-xlockmore | 1447 |
| t3d | 2d | L | `BlackPixelOfScreen`, `Button1Mask`, `Button2Mask`, `Button3Mask`, `GXandInverted`, `GXor`, `KeyPress`, `KeySym`, `XAllocColorCells`, `XCreatePixmap`, `XDrawSegments`, `XEvent.xkey`, `XGetImage`, `XLookupString`, `XPutImage`, `XQueryPointer`, `XStoreColors`, `error: too few arguments to function call, expected 2, have 1`, `keysym` | pixmaps, readback, float-heavy | 991 |
| tessellimage | 2d | L | `ButtonRelease`, `Convex`, `ITRIANGLE`, `X11/keysymdef.h`, `XCreateImage`, `XCreatePixmap`, `XDestroyImage`, `XEvent.xany`, `XGetImage`, `XGetPixel`, `XImage`, `XPutImage`, `XPutPixel`, `XQueryColor`, `XYZ`, `ZPixmap`, `async_load_state`, `delaunay`, `delaunay.h`, `delaunay_xyzcompare`, `dimg`, `double_time`, `doubletime.h`, `error: expected expression`, `img2`, `load_image_async_simple`, `p`, `tt`, `v`, `visual_rgb_masks` | pixmaps, readback | 996 |
| testx11 | 2d | L | `BlackPixelOfScreen`, `CapProjecting`, `CapRound`, `Convex`, `GCCapStyle`, `GCFont`, `GXxor`, `KeyPress`, `KeySym`, `XClearArea`, `XCreatePixmap`, `XCreatePixmapFromBitmapData`, `XDestroyImage`, `XDrawArc`, `XDrawSegments`, `XDrawString`, `XEvent.x`, `XEvent.xany`, `XEvent.xkey`, `XEvent.y`, `XGCValues.cap_style`, `XGCValues.font`, `XGetImage`, `XImage`, `XLoadFont`, `XLookupString`, `XPutImage`, `XPutPixel`, `XSegment`, `XSetWindowBackground`, `ZPixmap`, `colorbars.h`, `draw_colorbars`, `error: expected ')'`, `error: expected function body after function declarator`, `error: expected parameter declarator`, `get_position`, `get_rotation`, `glx/rotator.h`, `image`, `keysym`, `lines`, `make_color_loop`, `make_rotator`, `rotator`, `seg`, `visual_depth` | pixmaps, readback, text, clipmask | 968 |
| truchet | 2d | L | `CapRound`, `JoinRound`, `LineSolid`, `XCreatePixmap`, `XDrawArc`, `XSetLineAttributes` | pixmaps | 541 |
| twang | 2d | L | `XCreatePixmap`, `XDestroyImage`, `XGetImage`, `XGetPixel`, `XImage`, `XPutPixel`, `XShmSegmentInfo`, `ZPixmap`, `async_load_state`, `create_xshm_image`, `destroy_xshm_image`, `load_image_async_simple`, `put_xshm_image`, `xshm.h` | pixmaps, readback | 791 |
| vfeedback | 2d | L | `ANALOGTV_DEFAULTS`, `ANALOGTV_OPTIONS`, `ANALOGTV_SIGNAL_LEN`, `BlackPixelOfScreen`, `Button1`, `Button2`, `Button3`, `Button4`, `Button5`, `Button6`, `Button7`, `ButtonRelease`, `EASE_IN_OUT_SINE`, `KeyPress`, `KeySym`, `MotionNotify`, `RANDSIGN`, `XCreateImage`, `XCreatePixmap`, `XDestroyImage`, `XEvent.state`, `XEvent.x`, `XEvent.xany`, `XEvent.xkey`, `XEvent.xmotion`, `XEvent.y`, `XGetImage`, `XGetPixel`, `XImage`, `XK_Down`, `XK_Left`, `XK_Right`, `XK_Up`, `XLookupString`, `XPutPixel`, `ZPixmap`, `analogtv`, `analogtv.h`, `analogtv_allocate`, `analogtv_draw`, `analogtv_input_allocate`, `analogtv_load_ximage`, `analogtv_reception`, `analogtv_reception_update`, `analogtv_reconfigure`, `analogtv_release`, `analogtv_set_defaults`, `analogtv_setup_sync`, `double_time`, `doubletime.h`, `ease`, `easing.h`, `img`, `in`, `keysym`, `out` | pixmaps, readback | 592 |
| wander | 2d | L | `XCreatePixmap`, `make_color_loop` | pixmaps | 284 |
| whirlygig | 2d | L | `XCreatePixmap`, `XDrawString` | pixmaps, text, float-heavy | 741 |
| wormhole | 2d | L | `CapRound`, `JoinRound`, `LineSolid`, `XCreatePixmap`, `XSetLineAttributes` | pixmaps | 734 |
| xanalogtv | 2d | L | `ANALOGTV_DEFAULTS`, `ANALOGTV_OPTIONS`, `ANALOGTV_PIC_LEN`, `ANALOGTV_SCALE`, `ANALOGTV_SIGNAL_LEN`, `ANALOGTV_V`, `ANALOGTV_VIS_LEN`, `ANALOGTV_VIS_START`, `KeyPress`, `KeySym`, `XCreatePixmap`, `XDestroyImage`, `XEvent.xkey`, `XGetImage`, `XImage`, `XK_Down`, `XK_Left`, `XK_Next`, `XK_Prior`, `XK_Right`, `XK_Up`, `XLookupString`, `XrmDatabase`, `XrmPutResource`, `XrmValue`, `ZPixmap`, `analogtv`, `analogtv.h`, `analogtv_allocate`, `analogtv_draw`, `analogtv_draw_solid_rel_lcp`, `analogtv_draw_string_centered`, `analogtv_font`, `analogtv_input`, `analogtv_input_allocate`, `analogtv_lcp_to_ntsc`, `analogtv_load_ximage`, `analogtv_make_font`, `analogtv_reception`, `analogtv_reception_update`, `analogtv_reconfigure`, `analogtv_release`, `analogtv_set_defaults`, `analogtv_setup_sync`, `analogtv_setup_teletext`, `db`, `file_to_pixmap`, `gethostname`, `image`, `images/gen/testcard_bbcf_png.h`, `images/gen/testcard_pm5544_png.h`, `images/gen/testcard_rca_png.h`, `inp`, `input`, `keysym`, `load_image_async`, `rec`, `testcard_bbcf_png`, `testcard_pm5544_png`, `testcard_rca_png`, `value`, `ximage` | pixmaps, readback | 689 |
| xflame | 2d | L | `XCreateImage`, `XDestroyImage`, `XGetPixel`, `XImage`, `XPutPixel`, `XQueryColor`, `XShmSegmentInfo`, `ZPixmap`, `bob_png`, `create_xshm_image`, `destroy_xshm_image`, `file_to_ximage`, `image`, `image_data_to_ximage`, `images/gen/bob_png.h`, `out`, `put_xshm_image`, `xshm.h` | readback | 826 |
| xjack | 2d | L | `FcChar8`, `XClearArea`, `XEvent.xany`, `XGlyphInfo`, `XftColor`, `XftColorAllocName`, `XftDraw`, `XftDrawCreate`, `XftDrawDestroy`, `XftDrawStringUtf8`, `XftFont`, `XftFontClose`, `XftTextExtentsUtf8`, `error: expected expression`, `load_xft_font_retry`, `overall`, `screen_number` | pixmaps | 508 |
| xlyap | 2d | L | `BlackPixelOfScreen`, `Cursor`, `KeyPress`, `KeySym`, `WhitePixelOfScreen`, `X11/cursorfont.h`, `XComposeStatus`, `XCreatePixmap`, `XEvent.xkey`, `XKeyEvent`, `XLookupString`, `XPending`, `XStoreColors`, `error: expected expression` | pixmaps | 1939 |
| xmatrix | 2d | L | `KeyPress`, `KeySym`, `XCreateImage`, `XCreatePixmap`, `XDestroyImage`, `XEvent.xany`, `XEvent.xkey`, `XGetImage`, `XGetPixel`, `XImage`, `XLookupString`, `XPutImage`, `XPutPixel`, `XYPixmap`, `XtAppAddTimeOut`, `XtAppContext`, `XtIntervalId`, `XtPointer`, `XtRemoveTimeOut`, `ZPixmap`, `app`, `i1`, `i2`, `im`, `images/gen/matrix1_png.h`, `images/gen/matrix1b_png.h`, `images/gen/matrix2_png.h`, `images/gen/matrix2b_png.h`, `keysym`, `matrix1_png`, `matrix1b_png`, `matrix2_png`, `matrix2b_png`, `text_data`, `textclient.h`, `textclient_close`, `textclient_getc`, `textclient_open`, `textclient_reshape` | pixmaps, readback | 1915 |
| zoom | 2d | L | `XCreatePixmap`, `XDestroyImage`, `XGetImage`, `XGetPixel`, `XImage`, `XSetWindowBackground`, `ZPixmap`, `async_load_state`, `error: too few arguments to function call, expected 2, have 1`, `load_image_async_simple` | pixmaps, readback | 290 |
| blaster | 2d | M | `XArc`, `XFillArcs`, `error: expected expression` | - | 1208 |
| cynosure | 2d | M | `XCreateBitmapFromData`, `XSetWindowBackground`, `rgb_to_hsv` | - | 457 |
| grav | 2d | M | `XDrawArc` | needs-xlockmore | 360 |
| hexadrop | 2d | M | `Convex`, `XSetWindowBackground` | - | 446 |
| hyperball | 2d | M | `UnmapNotify` | - | 2464 |
| interaggregate | 2d | M | `XParseColor` | - | 989 |
| lissie | 2d | M | `XDrawArc` | needs-xlockmore | 323 |
| munch | 2d | M | `GXxor`, `XSetFunction`, `i_log2`, `pow2.h` | xor | 462 |
| rotor | 2d | M | `CapButt`, `JoinMiter`, `LineSolid`, `XSetLineAttributes` | needs-xlockmore | 394 |
| scooter | 2d | M | `CapNotLast`, `JoinRound`, `LineSolid`, `XSetLineAttributes` | needs-xlockmore | 975 |
| substrate | 2d | M | `XParseColor` | - | 780 |
| triangle | 2d | M | `Convex`, `free_colors`, `make_smooth_colormap` | needs-xlockmore | 355 |
| vermiculate | 2d | M | `XSetWindowBackground`, `ya_random` | - | 1229 |
| worm | 2d | M | `GXor`, `XClearArea`, `XSetFunction`, `error: call to undeclared library function 'memset' with type 'void *(void *, int, unsigned long)'; ISO C99 and later do not support implicit function declarations [-Wimplicit-function-declaration]` | xor, needs-xlockmore | 434 |
| xrayswarm | 2d | M | `XSetGraphicsExposures`, `initTime` | - | 1235 |
| cloudlife | 2d | S | - | - | 440 |
| coral | 2d | S | - | - | 328 |
| critical | 2d | S | - | - | 462 |
| discrete | 2d | S | - | needs-xlockmore | 442 |
| drift | 2d | S | - | needs-xlockmore | 674 |
| fadeplot | 2d | S | - | needs-xlockmore | 243 |
| flame | 2d | S | - | - | 457 |
| galaxy | 2d | S | - | needs-xlockmore | 462 |
| helix | 2d | S | - | - | 358 |
| hopalong | 2d | S | - | needs-xlockmore | 563 |
| hypercube | 2d | S | - | - | 576 |
| laser | 2d | S | - | needs-xlockmore | 356 |
| lightning | 2d | S | - | needs-xlockmore | 602 |
| mountain | 2d | S | - | needs-xlockmore | 283 |
| pedal | 2d | S | - | - | 339 |
| petri | 2d | S | - | - | 780 |
| pyro | 2d | S | - | - | 373 |
| rorschach | 2d | S | - | - | 227 |
| sierpinski | 2d | S | - | needs-xlockmore | 215 |
| sphere | 2d | S | - | needs-xlockmore | 304 |
| spiral | 2d | S | - | needs-xlockmore | 331 |
| squiral | 2d | S | - | - | 335 |
| thornbird | 2d | S | - | needs-xlockmore | 270 |
| vines | 2d | S | - | needs-xlockmore | 190 |
| whirlwindwarp | 2d | S | - | - | 509 |
| xspirograph | 2d | S | - | - | 338 |
| antinspect | gl | XL | - | needs-xlockmore | 696 |
| antmaze | gl | XL | - | needs-xlockmore | 1612 |
| antspotlight | gl | XL | - | needs-xlockmore | 797 |
| atlantis | gl | XL | `XDestroyImage` | needs-xlockmore | 607 |
| atunnel | gl | XL | `XDestroyImage` | needs-xlockmore | 315 |
| b_lockglue | gl | XL | `XParseColor` | needs-xlockmore | 240 |
| beats | gl | XL | - | needs-xlockmore | 439 |
| blinkbox | gl | XL | - | needs-xlockmore | 615 |
| blocktube | gl | XL | `XDestroyImage` | needs-xlockmore | 454 |
| boing | gl | XL | `XParseColor` | float-heavy, needs-xlockmore | 666 |
| bouncingcow | gl | XL | - | needs-xlockmore | 649 |
| boxed | gl | XL | - | needs-xlockmore | 1361 |
| cage | gl | XL | `XDestroyImage` | needs-xlockmore | 498 |
| carousel | gl | XL | - | needs-xlockmore | 982 |
| chompytower | gl | XL | `XParseColor` | needs-xlockmore | 1131 |
| circuit | gl | XL | - | needs-xlockmore | 2106 |
| cityflow | gl | XL | - | needs-xlockmore | 561 |
| companion | gl | XL | - | needs-xlockmore | 605 |
| covid19 | gl | XL | `XParseColor` | needs-xlockmore | 657 |
| crackberg | gl | XL | `XLookupString`, `XNextEvent`, `XPeekEvent`, `XPending` | float-heavy, needs-xlockmore | 1484 |
| crumbler | gl | XL | - | needs-xlockmore | 905 |
| cube21 | gl | XL | - | needs-xlockmore | 943 |
| cubenetic | gl | XL | - | needs-xlockmore | 616 |
| cubestack | gl | XL | `XLookupString` | needs-xlockmore | 453 |
| cubestorm | gl | XL | `XLookupString` | needs-xlockmore | 485 |
| cubetwist | gl | XL | `XLookupString` | needs-xlockmore | 579 |
| cubicgrid | gl | XL | - | needs-xlockmore | 322 |
| cubocteversion | gl | XL | `XDestroyImage` | needs-xlockmore | 5657 |
| dangerball | gl | XL | - | needs-xlockmore | 377 |
| deepstars | gl | XL | - | needs-xlockmore | 385 |
| discoball | gl | XL | - | needs-xlockmore | 709 |
| dnalogo | gl | XL | `XLookupString`, `XParseColor` | float-heavy, needs-xlockmore | 3657 |
| dumpsterfire | gl | XL | `XParseColor` | needs-xlockmore | 845 |
| dymaxionmap | gl | XL | `XCreateImage`, `XDestroyImage`, `XGetPixel`, `XLookupString`, `XPutPixel` | readback, needs-xlockmore | 1729 |
| endgame | gl | XL | - | needs-xlockmore | 1451 |
| energystream | gl | XL | - | needs-xlockmore | 536 |
| engine | gl | XL | - | needs-xlockmore | 1015 |
| esper | gl | XL | `XLookupString`, `XParseColor` | needs-xlockmore | 2463 |
| etruscanvenus | gl | XL | - | needs-xlockmore | 2761 |
| extrusion | gl | XL | - | needs-xlockmore | 552 |
| flipflop | gl | XL | - | needs-xlockmore | 875 |
| flipscreen3d | gl | XL | - | needs-xlockmore | 524 |
| fliptext | gl | XL | `XParseColor` | needs-xlockmore | 1005 |
| floppy | gl | XL | `XParseColor` | needs-xlockmore | 584 |
| flurry | gl | XL | - | needs-xlockmore | 550 |
| flyingtoasters | gl | XL | `XDestroyImage` | needs-xlockmore | 924 |
| gears | gl | XL | - | needs-xlockmore | 953 |
| geodesic | gl | XL | - | float-heavy, needs-xlockmore | 825 |
| geodesicgears | gl | XL | `XLookupString` | float-heavy, needs-xlockmore | 1802 |
| gflux | gl | XL | - | needs-xlockmore | 799 |
| gibson | gl | XL | `XParseColor` | needs-xlockmore | 1317 |
| glblur | gl | XL | - | needs-xlockmore | 630 |
| glcells | gl | XL | - | needs-xlockmore | 1382 |
| gleidescope | gl | XL | `XOFFSET` | float-heavy, needs-xlockmore | 1620 |
| glforestfire | gl | XL | `XDestroyImage` | needs-xlockmore | 1089 |
| glhanoi | gl | XL | - | float-heavy, needs-xlockmore | 2080 |
| glknots | gl | XL | - | needs-xlockmore | 455 |
| glmatrix | gl | XL | `XDestroyImage`, `XGetPixel`, `XPutPixel` | readback, needs-xlockmore | 1077 |
| glplanet | gl | XL | `XDestroyImage` | float-heavy, needs-xlockmore | 1111 |
| glschool | gl | XL | - | needs-xlockmore | 229 |
| glslideshow | gl | XL | - | needs-xlockmore | 1917 |
| glsnake | gl | XL | - | needs-xlockmore | 2699 |
| gltext | gl | XL | - | needs-xlockmore | 619 |
| graphstat | gl | XL | - | needs-xlockmore | 750 |
| gravitywell | gl | XL | `XParseColor` | needs-xlockmore | 770 |
| handsy | gl | XL | `XLookupString`, `XParseColor` | needs-xlockmore | 1158 |
| headroom | gl | XL | `XParseColor` | needs-xlockmore | 628 |
| hexstrut | gl | XL | `XLookupString` | needs-xlockmore | 511 |
| hextrail | gl | XL | `XLookupString` | needs-xlockmore | 782 |
| highvoltage | gl | XL | `XLookupString`, `XParseColor` | needs-xlockmore | 949 |
| hilbert | gl | XL | `XLookupString` | needs-xlockmore | 1165 |
| hopffibration | gl | XL | - | needs-xlockmore | 3580 |
| hydrostat | gl | XL | - | needs-xlockmore | 796 |
| hypertorus | gl | XL | `XLookupString` | float-heavy, needs-xlockmore | 2150 |
| hypnowheel | gl | XL | - | needs-xlockmore | 335 |
| jigglypuff | gl | XL | `XDestroyImage` | needs-xlockmore | 1125 |
| jigsaw | gl | XL | - | needs-xlockmore | 1512 |
| juggler3d | gl | XL | `XSetLineAttributes` | float-heavy, needs-xlockmore | 3023 |
| kaleidocycle | gl | XL | `XLookupString` | needs-xlockmore | 584 |
| kallisti | gl | XL | - | needs-xlockmore | 357 |
| klein | gl | XL | `XLookupString` | float-heavy, needs-xlockmore | 3574 |
| klondike | gl | XL | `XDestroyImage` | needs-xlockmore | 803 |
| lament | gl | XL | `XDestroyImage`, `XLookupString` | needs-xlockmore | 1801 |
| lavalite | gl | XL | `XParseColor` | needs-xlockmore | 1552 |
| lockward | gl | XL | `XLookupString` | needs-xlockmore | 964 |
| mapscroller | gl | XL | `XDestroyImage`, `XGetPixel`, `XLookupString` | readback, needs-xlockmore | 1672 |
| maze3d | gl | XL | - | needs-xlockmore | 1958 |
| menger | gl | XL | `XLookupString` | needs-xlockmore | 574 |
| mirrorblob | gl | XL | - | needs-xlockmore | 1822 |
| moebius | gl | XL | `XDestroyImage` | needs-xlockmore | 794 |
| moebiusgears | gl | XL | `XLookupString` | needs-xlockmore | 446 |
| molecule | gl | XL | `XLookupString`, `XParseColor` | needs-xlockmore | 1716 |
| morph3d | gl | XL | - | needs-xlockmore | 841 |
| nakagin | gl | XL | `XParseColor` | needs-xlockmore | 1636 |
| noof | gl | XL | - | needs-xlockmore | 530 |
| papercube | gl | XL | - | needs-xlockmore | 1111 |
| peepers | gl | XL | `XChangeProperty`, `XDestroyImage`, `XInternAtom`, `XQueryPointer` | float-heavy, needs-xlockmore | 1470 |
| photopile | gl | XL | - | needs-xlockmore | 869 |
| pinion | gl | XL | `XLookupString`, `XQueryPointer` | needs-xlockmore | 1497 |
| pipes | gl | XL | - | needs-xlockmore | 1208 |
| platonicfolding | gl | XL | `XDestroyImage` | needs-xlockmore | 3465 |
| polyhedra-gl | gl | XL | `XLookupString` | needs-xlockmore | 687 |
| polytopes | gl | XL | `XLookupString` | needs-xlockmore | 3194 |
| projectiveplane | gl | XL | `XLookupString` | float-heavy, needs-xlockmore | 2643 |
| providence | gl | XL | - | float-heavy, needs-xlockmore | 811 |
| pulsar | gl | XL | - | needs-xlockmore | 509 |
| quasicrystal | gl | XL | `XLookupString` | needs-xlockmore | 494 |
| queens | gl | XL | - | needs-xlockmore | 608 |
| raverhoop | gl | XL | `XLookupString` | needs-xlockmore | 771 |
| razzledazzle | gl | XL | - | needs-xlockmore | 728 |
| romanboy | gl | XL | - | needs-xlockmore | 2565 |
| rubik | gl | XL | - | needs-xlockmore | 2156 |
| rubikblocks | gl | XL | - | needs-xlockmore | 651 |
| sballs | gl | XL | `XDestroyImage` | needs-xlockmore | 830 |
| sierpinski3d | gl | XL | `XLookupString` | needs-xlockmore | 579 |
| skulloop | gl | XL | `XParseColor` | needs-xlockmore | 651 |
| skytentacles | gl | XL | `XCreateImage`, `XDestroyImage`, `XLookupString`, `XParseColor`, `XPutPixel` | float-heavy, needs-xlockmore | 1124 |
| sonar | gl | XL | - | needs-xlockmore | 1266 |
| sphereeversion | gl | XL | `XDestroyImage` | needs-xlockmore | 1420 |
| spheremonics | gl | XL | - | needs-xlockmore | 926 |
| splitflap | gl | XL | `XParseColor` | needs-xlockmore | 1427 |
| splodesic | gl | XL | `XLookupString` | float-heavy, needs-xlockmore | 645 |
| sproingiewrap | gl | XL | - | needs-xlockmore | 227 |
| squirtorus | gl | XL | `XParseColor` | needs-xlockmore | 1006 |
| stairs | gl | XL | `XDestroyImage`, `XLookupString` | needs-xlockmore | 601 |
| starwars | gl | XL | - | needs-xlockmore | 1080 |
| stonerview | gl | XL | - | needs-xlockmore | 156 |
| superquadrics | gl | XL | - | needs-xlockmore | 811 |
| surfaces | gl | XL | - | float-heavy, needs-xlockmore | 654 |
| tangram | gl | XL | - | needs-xlockmore | 1074 |
| timetunnel | gl | XL | `XDestroyImage` | needs-xlockmore | 1259 |
| topblock | gl | XL | `XLookupString` | needs-xlockmore | 891 |
| tronbit | gl | XL | `XLookupString` | needs-xlockmore | 532 |
| unicrud | gl | XL | `XLookupString` | needs-xlockmore | 1039 |
| unknownpleasures | gl | XL | `XCreateImage`, `XDestroyImage`, `XGetPixel`, `XLookupString`, `XParseColor`, `XPutPixel` | readback, needs-xlockmore | 736 |
| vigilance | gl | XL | `XLookupString`, `XParseColor` | needs-xlockmore | 1171 |
| voronoi | gl | XL | - | needs-xlockmore | 543 |
| winduprobot | gl | XL | `XDestroyImage`, `XLookupString`, `XParseColor` | float-heavy, needs-xlockmore | 2505 |
| worldpieces | gl | XL | `XDestroyImage` | float-heavy, needs-xlockmore | 2194 |
| xshadertoy | gl | XL | `XFetchName`, `XQueryPointer`, `XStoreName` | needs-xlockmore | 1192 |

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
