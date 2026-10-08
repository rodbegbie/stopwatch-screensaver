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
| S | 2D, and the unmodified source compiles against the shim | 7 |
| M | 2D, 1-4 shim gaps, no pixmaps or pixel read-back | 34 |
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

Seven hacks have been run so far (default settings, 466×466 canvas pushed to
the display every frame, canvas held in PSRAM). The firmware times each frame
in three parts, averaged over 5 seconds: **step** is the hack's own draw call,
**push** is sending the canvas to the display, and **wait** is what is left of
the delay the hack asked for after step and push (the runner clamps the delay
to 1 second).

| Hack | fps | step | push | wait | Extra PSRAM |
| --- | --- | --- | --- | --- | --- |
| Pyro | 30.0 | 0.8-1.1 ms | 31.2 ms | 0 ms | about 80 KB |
| HyperCube | 28.3 | 2.7-2.9 ms | 31.5 ms | 0 ms | none measurable |
| Petri | 28.8-30.8 | 0.3-2.5 ms | 31.1 ms | 0 ms | about 1.3 MB |
| XSpirograph | 11.0-11.4 | 54-56 ms | 31.4 ms | 0-20 ms | none measurable |
| Helix | 16.6-23.6 | 0.9-2.1 ms | 31.3 ms | 9-26 ms | none measurable |
| Rorschach | 10.6 | 1.5 ms | 31.2 ms | 61 ms | none measurable |
| Pedal | 1.0 | 30-74 ms | 31.4 ms | 990-1250 ms | none measurable |

Free heap and free PSRAM return to exactly the same values every time a
hack is switched back to, so switching does not leak.

## What limits the frame rate

- Pushing the 434 KB canvas costs a steady 31 ms whatever the hack draws, so
  about 31 fps is the real ceiling today. The best hacks reach about 30.
- The firmware treats a hack's requested delay as the frame period and waits
  only for what is left after step and push. Before that change the delay was
  added on top, and Pyro, HyperCube and Petri ran at 22-23.5 fps, Helix at
  11.6-15, XSpirograph at 9.2 and Rorschach at 9.0.
- Helix and Rorschach draw in under 3 ms a frame. Their remaining low frame
  rates come from 20 ms delays plus a 1-second hold between pictures
  (Rorschach asks for 5 seconds, clamped to 1). Pedal likewise holds each
  picture for a second.
- XSpirograph is the only measured hack that is slow at drawing: 54-56 ms a
  frame for 1000 lines. Whether that is the PSRAM pixel writes or its
  double-precision maths has not been separated.
- Hacks that draw many primitives per frame are the ones to profile first.

## Other notes

Rorschach keeps a 9.6 KB array on the stack, which overflowed the Arduino
loop task's default 8 KB stack and rebooted the device on the first frame. The
firmware now sets a 16 KB loop stack (about 8 KB less free heap). Any hack with
large local arrays can hit the same limit.

Pyro's `init` builds two 6284-entry sine and cosine tables in double
precision, so restarting it dips to about 17 fps for a few seconds.

## Suggested order for shim stage 2

Shim gaps across 2D hacks, ranked so gaps that block hacks
needing few additions come first.

| Gap | Score | 2D hacks needing it |
| --- | --- | --- |
| `xlock.h` | 14.92 | 40 |
| `XCreatePixmap` | 4.67 | 57 |
| `XCopyArea` | 4.64 | 50 |
| `XSetLineAttributes` | 4.57 | 27 |
| `XFreePixmap` | 4.11 | 53 |
| `make_smooth_colormap` | 3.95 | 25 |
| `Pixmap` | 3.77 | 51 |
| `XDrawPoints` | 2.88 | 9 |
| `XDrawArc` | 2.53 | 17 |
| `XParseColor` | 2.41 | 8 |

## All hacks

| Hack | Kind | Effort | Shim gaps | Flags | LOC |
| --- | --- | --- | --- | --- | --- |
| abstractile | 2d | L | `BlackPixelOfScreen`, `Convex`, `countof`, `make_color_loop`, `make_color_ramp`, `make_smooth_colormap`, `make_uniform_colormap`, `rgb_to_hsv` | - | 1625 |
| anemone | 2d | L | `CapRound`, `JoinBevel`, `LineSolid`, `Pixmap`, `XCopyArea`, `XCreatePixmap`, `XFreePixmap`, `XSetLineAttributes`, `make_smooth_colormap` | pixmaps | 458 |
| anemotaxis | 2d | L | `CapRound`, `JoinRound`, `LineSolid`, `Pixmap`, `XCopyArea`, `XCreatePixmap`, `XSetLineAttributes` | pixmaps | 760 |
| ant | 2d | L | `XChangeGC`, `XCreatePixmapFromBitmapData`, `XDrawArc`, `XFreePixmap`, `XSetLineAttributes`, `automata.h`, `xlock.h` | needs-xlockmore | 1351 |
| apple2-main | 2d | L | `A2CONTROLLER_DONE`, `A2CONTROLLER_FREE`, `A2_GR_FULL`, `A2_GR_HIRES`, `A2_GR_LORES`, `ANALOGTV_DEFAULTS`, `ANALOGTV_OPTIONS`, `DisplayOfScreen`, `GrayScale`, `KeyPress`, `Pixmap`, `PseudoColor`, `TTY_BLINK`, `TTY_BOLD`, `TTY_INVERSE`, `TTY_SYMBOLS`, `XCreatePixmap`, `XDestroyImage`, `XEvent.xany`, `XEvent.xkey`, `XFreePixmap`, `XGetImage`, `XGetPixel`, `XImage`, `XQueryColors`, `ZPixmap`, `a2_clear_gr`, `a2_clear_hgr`, `a2_cls`, `a2_display_image_loading`, `a2_goto`, `a2_hline`, `a2_hplot`, `a2_invalidate`, `a2_plot`, `a2_printc`, `a2_printc_noscroll`, `a2_prints`, `analogtv_reconfigure`, `ansi-tty.h`, `ansi_tty`, `ansi_tty_free`, `ansi_tty_init`, `ansi_tty_print`, `apple2.h`, `apple2_one_frame`, `apple2_sim_t`, `apple2_start`, `apple2_state_t`, `countof`, `error: expected expression`, `error: incompatible integer to pointer conversion initializing 'Display *' (aka 'struct XshimDisplay *') with an expression of type 'int' [-Wint-conversion]`, `error: incompatible integer to pointer conversion initializing 'char *' with an expression of type 'int' [-Wint-conversion]`, `flag`, `image`, `load_image_async`, `p`, `sim`, `st`, `tc`, `text_data`, `textclient.h`, `textclient_close`, `textclient_getc`, `textclient_open`, `textclient_putc_event`, `textclient_puts`, `textclient_reshape`, `time`, `time_t`, `tty`, `tty_char`, `tty_flag`, `utf8_encode`, `utf8_to_latin1`, `utf8wc.h`, `visual_cells`, `visual_class`, `visual_rgb_masks` | pixmaps, readback | 1642 |
| attraction | 2d | L | `ButtonRelease`, `CapButt`, `CapRound`, `Convex`, `GCCapStyle`, `XDrawRectangle`, `XEvent.x`, `XEvent.xany`, `XEvent.y`, `XGCValues.cap_style`, `XQueryPointer`, `compute_closed_spline`, `error: incompatible integer to pointer conversion assigning to 'int *' from 'int' [-Wint-conversion]`, `error: member reference base type 'int' is not a structure or union`, `error: type name does not allow function specifier to be specified`, `error: type specifier missing, defaults to 'int'; ISO C99 and later do not support implicit int [-Wimplicit-int]`, `free_spline`, `make_color_ramp`, `make_smooth_colormap`, `make_spline`, `spline` | - | 1115 |
| barcode | 2d | L | `ButtonRelease`, `LSBFirst`, `XCreateImage`, `XDestroyImage`, `XEvent.xany`, `XImage`, `XPutImage`, `XYBitmap` | - | 2055 |
| binaryhorizon | 2d | L | `KeyPress`, `Pixmap`, `XCopyArea`, `XCreatePixmap`, `XDestroyImage`, `XEvent.xany`, `XFreePixmap`, `XGetImage`, `XImage`, `XPutImage`, `XPutPixel`, `ZPixmap`, `error: expected expression`, `time`, `time_t`, `visual_depth` | pixmaps, readback | 624 |
| binaryring | 2d | L | `KeyPress`, `Pixmap`, `XCopyArea`, `XCreatePixmap`, `XDestroyImage`, `XEvent.xany`, `XFreePixmap`, `XGetImage`, `XImage`, `XPutImage`, `XPutPixel`, `ZPixmap`, `visual_depth` | pixmaps, readback | 577 |
| blitspin | 2d | L | `GXand`, `GXclear`, `GXor`, `GXset`, `GXxor`, `Pixmap`, `XCopyArea`, `XCopyPlane`, `XCreatePixmap`, `XDestroyImage`, `XDisplayHeight`, `XDisplayWidth`, `XFreePixmap`, `XGetImage`, `XPutImage`, `XScreenNumberOfScreen`, `XSetClipMask`, `async_load_state`, `error: type specifier missing, defaults to 'int'; ISO C99 and later do not support implicit int [-Wimplicit-int]`, `file_to_pixmap`, `images/gen/som_png.h`, `load_image_async_simple`, `mask`, `pixmap`, `pow2.h`, `to_pow2`, `ximage-loader.h` | pixmaps, readback, clipmask | 467 |
| bouboule | 2d | L | `Display`, `GC`, `GXcopy`, `GXor`, `MAX`, `MIN`, `MI_BATCHCOUNT`, `MI_DELTA3D`, `MI_DISPLAY`, `MI_GC`, `MI_HEIGHT`, `MI_INIT`, `MI_LEFT_COLOR`, `MI_NONE_COLOR`, `MI_NPIXELS`, `MI_PIXEL`, `MI_RIGHT_COLOR`, `MI_SCREEN`, `MI_SIZE`, `MI_WIDTH`, `MI_WINDOW`, `MI_WIN_BLACK_PIXEL`, `MI_WIN_HEIGHT`, `MI_WIN_IS_INSTALL`, `MI_WIN_IS_USE3D`, `MI_WIN_WHITE_PIXEL`, `MI_WIN_WIDTH`, `M_PI`, `ModeInfo`, `ModeSpecOpt`, `NRAND`, `NULL`, `Window`, `XArc`, `XClearWindow`, `XFillArcs`, `XFillRectangle`, `XSetForeground`, `XSetFunction`, `arc`, `arcleft`, `display`, `error: call to undeclared library function 'calloc' with type 'void *(unsigned long, unsigned long)'; ISO C99 and later do not support implicit function declarations [-Wimplicit-function-declaration]`, `error: call to undeclared library function 'cos' with type 'double (double)'; ISO C99 and later do not support implicit function declarations [-Wimplicit-function-declaration]`, `error: call to undeclared library function 'free' with type 'void (void *)'; ISO C99 and later do not support implicit function declarations [-Wimplicit-function-declaration]`, `error: call to undeclared library function 'malloc' with type 'void *(unsigned long)'; ISO C99 and later do not support implicit function declarations [-Wimplicit-function-declaration]`, `error: call to undeclared library function 'sin' with type 'double (double)'; ISO C99 and later do not support implicit function declarations [-Wimplicit-function-declaration]`, `error: expected ')'`, `error: expected expression`, `error: expected function body after function declarator`, `error: expected parameter declarator`, `gc`, `oarc`, `oarcleft`, `window`, `xlock.h` | xor, needs-xlockmore | 860 |
| boxfit | 2d | L | `Pixmap`, `XCreatePixmap`, `XDestroyImage`, `XDrawArc`, `XDrawRectangle`, `XFreePixmap`, `XGetImage`, `XGetPixel`, `XImage`, `XSetWindowBackground`, `ZPixmap`, `async_load_state`, `free_colors`, `load_image_async_simple`, `make_smooth_colormap`, `ximage-loader.h` | pixmaps, readback | 573 |
| bsod | 2d | L | `A2CONTROLLER_DONE`, `A2CONTROLLER_FREE`, `A2_GR_FULL`, `A2_GR_HIRES`, `A2_GR_LORES`, `ANALOGTV_DEFAULTS`, `ANALOGTV_OPTIONS`, `CapButt`, `CapRound`, `FcChar8`, `JoinMiter`, `LineSolid`, `None`, `Pixmap`, `XChangeGC`, `XClearArea`, `XColor.color`, `XCopyArea`, `XCopyPlane`, `XCreateImage`, `XCreatePixmap`, `XCreatePixmapFromBitmapData`, `XDestroyImage`, `XDrawRectangle`, `XFetchName`, `XFreePixmap`, `XGetImage`, `XGetPixel`, `XGlyphInfo`, `XImage`, `XPutImage`, `XPutPixel`, `XQueryColor`, `XSetBackground`, `XSetClipMask`, `XSetClipOrigin`, `XSetLineAttributes`, `XSetPlaneMask`, `XSetWindowBackground`, `XStoreName`, `XYPixmap`, `XftColor`, `XftDraw`, `XftDrawCreate`, `XftDrawDestroy`, `XftDrawStringUtf8`, `XftFont`, `XftFontClose`, `XftTextExtentsUtf8`, `XftTextExtentsUtf8_multi`, `ZPixmap`, `a2_cls`, `a2_goto`, `a2_init_memory_active`, `a2_invalidate`, `a2_poke`, `a2_printc`, `a2_printc_noscroll`, `a2_prints`, `amiga_png`, `android_png`, `apple2.h`, `apple2_one_frame`, `apple2_sim_t`, `apple2_start`, `apple2_state_t`, `async_load_state`, `cswap`, `em`, `error: expected expression`, `font`, `gnome1_png`, `gnome2_png`, `hmac_png`, `i1`, `i2`, `image_data_to_pixmap`, `images/gen/amiga_png.h`, `images/gen/android_png.h`, `images/gen/apple_png.h`, `images/gen/atari_png.h`, `images/gen/atm_png.h`, `images/gen/dvd_png.h`, `images/gen/gnome1_png.h`, `images/gen/gnome2_png.h`, `images/gen/hmac_png.h`, `images/gen/mac_png.h`, `images/gen/macbomb_png.h`, `images/gen/osx_10_2_png.h`, `images/gen/osx_10_3_png.h`, `images/gen/ransomware_png.h`, `images/gen/sun_png.h`, `load_image_async_simple`, `load_xft_font_retry`, `mask`, `osx_10_2_png`, `osx_10_3_png`, `ov`, `ov2`, `p2`, `pixmap`, `screen_number`, `sim`, `st`, `utf8_decode_combining`, `utf8wc.h`, `xft.h`, `xft_word_wrap`, `xftwrap.h`, `ximage-loader.h` | pixmaps, readback, clipmask | 7809 |
| bubbles | 2d | L | `BUBBLE_MAGIC`, `Bubble`, `Bubble_Step`, `DELETE_BUBBLE`, `KEEP_BUBBLE`, `MAX`, `MAX_DROPPAGE`, `MIN`, `XCopyArea`, `XDrawArc`, `XFreePixmap`, `XSetClipMask`, `XSetClipOrigin`, `bubbles.h`, `default_bubbles`, `error: expected expression`, `head`, `image_data_to_pixmap`, `init_default_bubbles`, `least`, `newpix`, `nextbub`, `num_default_bubbles`, `pixmap_list`, `rv`, `tmp`, `tmppix`, `touch`, `ximage-loader.h`, `yarandom.h` | pixmaps, clipmask | 1468 |
| bumps | 2d | L | `Pixmap`, `XCreatePixmap`, `XDestroyImage`, `XGetImage`, `XGetPixel`, `XImage`, `XParseColor`, `XQueryColors`, `XSetWindowBackground`, `XShmSegmentInfo`, `XSync`, `ZPixmap`, `async_load_state`, `create_xshm_image`, `destroy_xshm_image`, `load_image_async_simple`, `pScreenImage`, `put_xshm_image`, `xshm.h` | pixmaps, readback | 705 |
| ccurve | 2d | L | `Pixmap`, `XCopyArea`, `XCreatePixmap`, `XFreePixmap`, `make_color_loop` | pixmaps | 872 |
| celtic | 2d | L | `CapRound`, `GCCapStyle`, `JoinRound`, `LineSolid`, `XDrawArc`, `XGCValues.cap_style`, `XSetLineAttributes`, `make_smooth_colormap` | - | 1141 |
| compass | 2d | L | `Convex`, `GCJoinStyle`, `JoinBevel`, `Pixmap`, `XCopyArea`, `XCreatePixmap`, `XDrawSegments`, `XFreePixmap`, `XGCValues.join_style`, `XSegment`, `countof`, `segs` | pixmaps, float-heavy | 999 |
| crystal | 2d | L | `Bool`, `Colormap`, `Convex`, `CoordModeOrigin`, `Display`, `ENTRYPOINT`, `False`, `GC`, `GXcopy`, `GXxor`, `LRAND`, `MAX`, `MIN`, `MI_BG_PIXEL`, `MI_BLACK_PIXEL`, `MI_CLEARWINDOW`, `MI_COUNT`, `MI_DISPLAY`, `MI_FG_PIXEL`, `MI_HEIGHT`, `MI_INIT`, `MI_IS_DRAWN`, `MI_IS_FULLRANDOM`, `MI_IS_INSTALL`, `MI_IS_VERBOSE`, `MI_NCOLORS`, `MI_NPIXELS`, `MI_PIXEL`, `MI_SCREEN`, `MI_SIZE`, `MI_VISUAL`, `MI_WHITE_PIXEL`, `MI_WIDTH`, `MI_WINDOW`, `M_PI`, `ModeInfo`, `NULL`, `None`, `OptionStruct`, `True`, `Window`, `XAllocColor`, `XColor`, `XCreateColormap`, `XCreateGC`, `XDrawLine`, `XFillPolygon`, `XFreeColormap`, `XFreeGC`, `XGCValues`, `XInstallColormap`, `XParseColor`, `XPoint`, `XSetForeground`, `XSetFunction`, `XSetWindowColormap`, `XrmOptionDescRec`, `XrmoptionNoArg`, `XrmoptionSepArg`, `argtype`, `color`, `color.h`, `display`, `error: call to undeclared library function 'calloc' with type 'void *(unsigned long, unsigned long)'; ISO C99 and later do not support implicit function declarations [-Wimplicit-function-declaration]`, `error: call to undeclared library function 'cos' with type 'double (double)'; ISO C99 and later do not support implicit function declarations [-Wimplicit-function-declaration]`, `error: call to undeclared library function 'fabs' with type 'double (double)'; ISO C99 and later do not support implicit function declarations [-Wimplicit-function-declaration]`, `error: call to undeclared library function 'free' with type 'void (void *)'; ISO C99 and later do not support implicit function declarations [-Wimplicit-function-declaration]`, `error: call to undeclared library function 'sin' with type 'double (double)'; ISO C99 and later do not support implicit function declarations [-Wimplicit-function-declaration]`, `error: expected ')'`, `error: expected ';' after top level declarator`, `error: expected expression`, `error: expected function body after function declarator`, `error: expected parameter declarator`, `fprintf`, `free_colors`, `has_writable_cells`, `make_random_colormap`, `make_smooth_colormap`, `make_uniform_colormap`, `new_xy`, `rotate_colors`, `stdout`, `t_Bool`, `t_Int`, `window`, `xlock.h`, `xy`, `xy1`, `xy_1` | xor, float-heavy, needs-xlockmore | 1286 |
| cwaves | 2d | L | `BlackPixelOfScreen`, `CapRound`, `JoinRound`, `LineSolid`, `XSetLineAttributes`, `make_smooth_colormap`, `ximage-loader.h` | - | 219 |
| cynosure | 2d | L | `XCreateBitmapFromData`, `XDrawRectangle`, `XFreePixmap`, `XSetWindowBackground`, `make_smooth_colormap`, `rgb_to_hsv` | - | 457 |
| decayscreen | 2d | L | `Pixmap`, `XCopyArea`, `XCreatePixmap`, `async_load_state`, `load_image_async_simple` | pixmaps | 392 |
| deco | 2d | L | `CapButt`, `DisplayOfScreen`, `JoinBevel`, `LineSolid`, `XChangeGC`, `XDrawRectangle`, `XSetLineAttributes`, `XStoreColors`, `allocate_writable_colors`, `error: incompatible integer to pointer conversion initializing 'Display *' (aka 'struct XshimDisplay *') with an expression of type 'int' [-Wint-conversion]`, `has_writable_cells`, `make_smooth_colormap` | - | 345 |
| deluxe | 2d | L | `CapProjecting`, `GCCapStyle`, `GCJoinStyle`, `GCPlaneMask`, `JoinMiter`, `Pixmap`, `XCopyArea`, `XCreatePixmap`, `XDrawArc`, `XFreePixmap`, `XGCValues.cap_style`, `XGCValues.join_style`, `XGCValues.plane_mask`, `allocate_alpha_colors`, `alpha.h`, `countof` | pixmaps, float-heavy | 480 |
| demon | 2d | L | `XChangeGC`, `XCreatePixmapFromBitmapData`, `XFreePixmap`, `automata.h`, `xlock.h` | needs-xlockmore | 953 |
| distort | 2d | L | `BlackPixelOfScreen`, `Pixmap`, `XCopyArea`, `XCreatePixmap`, `XDestroyImage`, `XFreePixmap`, `XGetImage`, `XGetPixel`, `XImage`, `XPutImage`, `XPutPixel`, `XShmSegmentInfo`, `ZPixmap`, `async_load_state`, `create_xshm_image`, `destroy_xshm_image`, `load_image_async_simple`, `put_xshm_image`, `xshm.h` | pixmaps, readback | 894 |
| droste | 2d | L | `BlackPixelOfScreen`, `GET_PARENT_OBJ`, `KeyPress`, `KeySym`, `Pixmap`, `THREAD_DEFAULTS`, `THREAD_OPTIONS`, `XCreatePixmap`, `XDestroyImage`, `XEvent.xkey`, `XFreePixmap`, `XGetImage`, `XGetPixel`, `XImage`, `XK_Down`, `XK_Left`, `XK_Right`, `XK_Up`, `XLookupString`, `XPutPixel`, `XShmSegmentInfo`, `ZPixmap`, `async_load_state`, `countof`, `create_xshm_image`, `destroy_xshm_image`, `double_time`, `doubletime.h`, `error: expected expression`, `error: field has incomplete type 'struct threadpool'`, `error: variable has incomplete type 'const struct threadpool_class'`, `hardware_concurrency`, `i_log2_fast`, `keysym`, `load_image_async_simple`, `pow2.h`, `put_xshm_image`, `thread_util.h`, `threadpool`, `threadpool_create`, `threadpool_destroy`, `threadpool_run`, `threadpool_wait`, `xshm.h` | pixmaps, readback | 686 |
| epicycle | 2d | L | `CapRound`, `GCCapStyle`, `GCJoinStyle`, `JoinRound`, `XGCValues.cap_style`, `XGCValues.join_style`, `free_colors`, `make_smooth_colormap` | - | 803 |
| eruption | 2d | L | `XEvent.x`, `XEvent.y`, `XImage`, `XPutPixel`, `XSetWindowBackground`, `XShmSegmentInfo`, `ZPixmap`, `create_xshm_image`, `destroy_xshm_image`, `error: typedef redefinition with different types ('unsigned long' vs 'unsigned int')`, `img`, `put_xshm_image`, `xshm.h` | - | 608 |
| fiberlamp | 2d | L | `XAllocNamedColor`, `XCopyArea`, `XCreatePixmap`, `XFreePixmap`, `XSetGraphicsExposures`, `XSetLineAttributes`, `XTranslateCoordinates`, `xlock.h` | pixmaps, needs-xlockmore | 480 |
| filmleader | 2d | L | `ANALOGTV_DEFAULTS`, `ANALOGTV_OPTIONS`, `ANALOGTV_SIGNAL_LEN`, `ButtonRelease`, `CapRound`, `FcChar8`, `JoinRound`, `KeyPress`, `KeySym`, `LineSolid`, `Pixmap`, `XCreateImage`, `XCreatePixmap`, `XDestroyImage`, `XDrawArc`, `XEvent.xany`, `XEvent.xkey`, `XFreePixmap`, `XGetImage`, `XGetPixel`, `XGlyphInfo`, `XImage`, `XLookupString`, `XPutImage`, `XPutPixel`, `XSetLineAttributes`, `XftColor`, `XftColorAllocName`, `XftColorFree`, `XftDraw`, `XftDrawCreate`, `XftDrawDestroy`, `XftDrawStringUtf8`, `XftFont`, `XftTextExtentsUtf8`, `ZPixmap`, `analogtv`, `analogtv.h`, `analogtv_allocate`, `analogtv_draw`, `analogtv_input`, `analogtv_input_allocate`, `analogtv_load_ximage`, `analogtv_reception`, `analogtv_reception_update`, `analogtv_reconfigure`, `analogtv_release`, `analogtv_set_defaults`, `analogtv_setup_sync`, `countof`, `double_time`, `doubletime.h`, `error: expected expression`, `extents`, `img`, `img1`, `img2`, `keysym`, `load_xft_font_retry`, `screen_number`, `xftfont` | pixmaps, readback | 548 |
| fireworkx | 2d | L | `ButtonRelease`, `ImageByteOrder`, `MSBFirst`, `XCreateImage`, `XDestroyImage`, `XEvent.x`, `XEvent.y`, `XImage`, `XPutImage`, `XSync`, `ZPixmap`, `make_smooth_colormap` | - | 882 |
| flag | 2d | L | `Bool`, `Display`, `ENTRYPOINT`, `False`, `LRAND`, `MAXRAND`, `MI_CYCLES`, `MI_DISPLAY`, `MI_GC`, `MI_INIT`, `MI_NPIXELS`, `MI_PIXEL`, `MI_SCREEN`, `MI_SIZE`, `MI_VISUAL`, `MI_WINDOW`, `MI_WIN_BLACK_PIXEL`, `MI_WIN_DEPTH`, `MI_WIN_HEIGHT`, `MI_WIN_WHITE_PIXEL`, `MI_WIN_WIDTH`, `M_PI`, `ModeInfo`, `NRAND`, `NULL`, `Pixmap`, `SINF`, `True`, `Window`, `XClearWindow`, `XCopyArea`, `XCreateImage`, `XCreatePixmap`, `XDestroyImage`, `XDrawPoint`, `XDrawString`, `XFillArc`, `XFillRectangle`, `XFreeFont`, `XFreePixmap`, `XGetImage`, `XGetPixel`, `XImage`, `XLoadQueryFont`, `XPutPixel`, `XSetForeground`, `XSetGraphicsExposures`, `XTextExtents`, `XYBitmap`, `display`, `error`, `error: call to undeclared library function 'abort' with type 'void (void) __attribute__((noreturn))'; ISO C99 and later do not support implicit function declarations [-Wimplicit-function-declaration]`, `error: call to undeclared library function 'calloc' with type 'void *(unsigned long, unsigned long)'; ISO C99 and later do not support implicit function declarations [-Wimplicit-function-declaration]`, `error: expected ')'`, `error: expected ';' after top level declarator`, `error: expected function body after function declarator`, `error: expected parameter declarator`, `flag.h`, `flag_bits`, `flag_height`, `flag_width`, `window`, `xlock.h` | pixmaps, readback, text, needs-xlockmore | 570 |
| flow | 2d | L | `XCopyArea`, `XCreatePixmap`, `XDrawSegments`, `XFreePixmap`, `XSetGraphicsExposures`, `XSetLineAttributes`, `xlock.h` | pixmaps, needs-xlockmore | 1216 |
| fluidballs | 2d | L | `ButtonRelease`, `FcChar8`, `Pixmap`, `RootWindow`, `XCopyArea`, `XCreatePixmap`, `XEvent.x`, `XEvent.xany`, `XEvent.y`, `XFreePixmap`, `XQueryPointer`, `XSelectInput`, `XTranslateCoordinates`, `XftColor`, `XftColorAllocName`, `XftDraw`, `XftDrawCreate`, `XftDrawDestroy`, `XftDrawStringUtf8`, `XftFont`, `XftFontClose`, `error: expected expression`, `gettimeofday`, `load_xft_font_retry`, `screen_number` | pixmaps | 881 |
| fontglide | 2d | L | `BlackPixelOfScreen`, `DisplayOfScreen`, `FcChar8`, `Pixmap`, `X11/Intrinsic.h`, `XCopyArea`, `XCreateImage`, `XCreatePixmap`, `XDestroyImage`, `XDrawRectangle`, `XDrawString`, `XDrawString16`, `XFreeFont`, `XFreePixmap`, `XGetAtomName`, `XGetGeometry`, `XGetImage`, `XGetPixel`, `XGlyphInfo`, `XImage`, `XLoadQueryFont`, `XLookupString`, `XPutImage`, `XPutPixel`, `XRenderColor`, `XSetClipMask`, `XSetClipOrigin`, `XSetFont`, `XTextExtents`, `XTextExtents16`, `XYPixmap`, `XftColor`, `XftColorAllocValue`, `XftColorFree`, `XftDraw`, `XftDrawCreate`, `XftDrawDestroy`, `XftDrawStringUtf8`, `XftFont`, `XftFontClose`, `XftTextExtentsUtf8`, `ZPixmap`, `bg`, `error: Xft is required under X11`, `error: expected expression`, `error: incompatible integer to pointer conversion initializing 'Display *' (aka 'struct XshimDisplay *') with an expression of type 'int' [-Wint-conversion]`, `extents`, `fg`, `in`, `load_xft_font_retry`, `mask`, `out`, `screen_number`, `swap`, `text_data`, `textclient.h`, `textclient_close`, `textclient_getc`, `textclient_open`, `utf8_decode_combining`, `utf8wc.h`, `xftdraw` | pixmaps, readback, text, clipmask | 2474 |
| forest | 2d | L | `CapButt`, `Display`, `DoBlue`, `DoGreen`, `DoRed`, `ENTRYPOINT`, `GC`, `JoinMiter`, `LineSolid`, `MI_DISPLAY`, `MI_INIT`, `MI_SCREEN`, `MI_WINDOW`, `MI_WIN_HEIGHT`, `MI_WIN_WIDTH`, `M_PI_2`, `ModeInfo`, `NRAND`, `NULL`, `XAllocColor`, `XArc`, `XClearWindow`, `XColor`, `XDrawLine`, `XFillArcs`, `XSetForeground`, `XSetLineAttributes`, `display`, `error: call to undeclared library function 'cos' with type 'double (double)'; ISO C99 and later do not support implicit function declarations [-Wimplicit-function-declaration]`, `error: call to undeclared library function 'printf' with type 'int (const char *, ...)'; ISO C99 and later do not support implicit function declarations [-Wimplicit-function-declaration]`, `error: call to undeclared library function 'sin' with type 'double (double)'; ISO C99 and later do not support implicit function declarations [-Wimplicit-function-declaration]`, `error: expected ')'`, `error: expected ';' after top level declarator`, `error: expected function body after function declarator`, `error: expected parameter declarator`, `gc`, `leaf`, `xlock.h` | needs-xlockmore | 241 |
| fuzzyflakes | 2d | L | `CapProjecting`, `GCCapStyle`, `GCJoinStyle`, `JoinMiter`, `Pixmap`, `XCopyArea`, `XCreatePixmap`, `XFreePixmap`, `XGCValues.cap_style`, `XGCValues.join_style`, `XParseColor` | pixmaps | 655 |
| galaxy | 2d | L | `Bool`, `COSF`, `Display`, `ENTRYPOINT`, `GC`, `LRAND`, `MAXRAND`, `MI_BATCHCOUNT`, `MI_CYCLES`, `MI_DISPLAY`, `MI_HEIGHT`, `MI_INIT`, `MI_NCOLORS`, `MI_PIXEL`, `MI_SCREEN`, `MI_WIDTH`, `MI_WINDOW`, `MI_WIN_BLACK_PIXEL`, `MI_WIN_HEIGHT`, `MI_WIN_WIDTH`, `M_PI`, `ModeInfo`, `NRAND`, `NULL`, `OptionStruct`, `SINF`, `Window`, `XClearWindow`, `XFillRectangles`, `XRectangle`, `XSetForeground`, `XrmOptionDescRec`, `XrmoptionNoArg`, `argtype`, `display`, `dummy`, `error: call to undeclared library function 'calloc' with type 'void *(unsigned long, unsigned long)'; ISO C99 and later do not support implicit function declarations [-Wimplicit-function-declaration]`, `error: call to undeclared library function 'exp' with type 'double (double)'; ISO C99 and later do not support implicit function declarations [-Wimplicit-function-declaration]`, `error: call to undeclared library function 'free' with type 'void (void *)'; ISO C99 and later do not support implicit function declarations [-Wimplicit-function-declaration]`, `error: call to undeclared library function 'malloc' with type 'void *(unsigned long)'; ISO C99 and later do not support implicit function declarations [-Wimplicit-function-declaration]`, `error: call to undeclared library function 'sqrt' with type 'double (double)'; ISO C99 and later do not support implicit function declarations [-Wimplicit-function-declaration]`, `error: expected ')'`, `error: expected ';' after top level declarator`, `error: expected expression`, `error: expected function body after function declarator`, `error: expected parameter declarator`, `gc`, `newp`, `oldp`, `t_Bool`, `window`, `xlock.h` | needs-xlockmore | 462 |
| glitchpeg | 2d | L | `BitmapBitOrder`, `ButtonRelease`, `ImageByteOrder`, `X11/Intrinsic.h`, `XCreateImage`, `XDestroyImage`, `XEvent.xany`, `XGetPixel`, `XImage`, `XPutImage`, `XPutPixel`, `XtAppAddInput`, `XtDisplayToApplicationContext`, `XtInputExceptMask`, `XtInputId`, `XtInputReadMask`, `XtPointer`, `XtRemoveInput`, `ZPixmap`, `error: operand of type 'XPoint' where arithmetic or pointer type is required`, `image`, `image_data_to_ximage`, `out`, `time`, `ximage-loader.h` | readback | 466 |
| goop | 2d | L | `AllPlanes`, `DefaultScreenOfDisplay`, `DisplayOfScreen`, `GXclear`, `GXxor`, `Nonconvex`, `Pixmap`, `WhitePixelOfScreen`, `XCopyArea`, `XCopyPlane`, `XCreatePixmap`, `XSetFunction`, `XSetPlaneMask`, `allocate_alpha_colors`, `alpha.h`, `compute_closed_spline`, `error: incompatible integer to pointer conversion assigning to 'int *' from 'int' [-Wint-conversion]`, `error: incompatible integer to pointer conversion initializing 'Display *' (aka 'struct XshimDisplay *') with an expression of type 'int' [-Wint-conversion]`, `error: member reference base type 'int' is not a structure or union`, `error: type name does not allow function specifier to be specified`, `error: type specifier missing, defaults to 'int'; ISO C99 and later do not support implicit int [-Wimplicit-int]`, `free_spline`, `has_writable_cells`, `make_spline`, `spline` | pixmaps, xor | 651 |
| greynetic | 2d | L | `FillOpaqueStippled`, `GCFillStyle`, `GCStipple`, `Pixmap`, `XChangeGC`, `XCreatePixmapFromBitmapData`, `XGCValues.fill_style`, `XGCValues.stipple` | - | 297 |
| halftone | 2d | L | `Pixmap`, `XCopyArea`, `XCreatePixmap`, `XFreePixmap`, `make_smooth_colormap` | pixmaps | 413 |
| halo | 2d | L | `GXxor`, `Pixmap`, `XCopyPlane`, `XCreatePixmap`, `XFreePixmap`, `XSetBackground`, `make_smooth_colormap`, `make_uniform_colormap` | pixmaps | 459 |
| hexadrop | 2d | L | `Convex`, `XSetWindowBackground`, `countof`, `free_colors`, `make_smooth_colormap` | - | 446 |
| ifs | 2d | L | `None`, `XCopyArea`, `XCreatePixmap`, `XFreePixmap`, `countof`, `make_smooth_colormap` | pixmaps | 560 |
| imsmap | 2d | L | `XCreateImage`, `XDestroyImage`, `XImage`, `XPutImage`, `XPutPixel`, `XSetBackground`, `XYBitmap`, `free_colors`, `image`, `make_smooth_colormap` | - | 426 |
| interference | 2d | L | `GET_PARENT_OBJ`, `None`, `Pixmap`, `THREAD_DEFAULTS`, `THREAD_OPTIONS`, `XCopyArea`, `XCreatePixmap`, `XFreePixmap`, `XImage`, `XPutPixel`, `XShmGetEventBase`, `XShmSegmentInfo`, `ZPixmap`, `create_xshm_image`, `destroy_xshm_image`, `error: expected expression`, `error: field has incomplete type 'struct threadpool'`, `error: variable has incomplete type 'const struct threadpool_class'`, `free_colors`, `gettimeofday`, `hardware_concurrency`, `make_color_loop`, `put_xshm_image`, `thread_memory_alignment`, `thread_util.h`, `threadpool`, `threadpool_create`, `threadpool_destroy`, `threadpool_run`, `threadpool_wait`, `visual_pixmap_depth`, `xshm.h` | pixmaps | 1002 |
| intermomentary | 2d | L | `Pixmap`, `XCopyArea`, `XCreatePixmap`, `XFreePixmap`, `XQueryColor`, `XSetFillStyle`, `XSetTile`, `make_color_ramp`, `rgb_to_hsv` | pixmaps | 605 |
| juggle | 2d | L | `XDrawArc`, `XDrawImageString`, `XDrawString`, `XFreeFontInfo`, `XLoadQueryFont`, `XSetLineAttributes`, `XTextWidth`, `xlock.h` | text, float-heavy, needs-xlockmore | 2798 |
| julia | 2d | L | `Bool`, `Cursor`, `Display`, `DoBlue`, `DoGreen`, `DoRed`, `ENTRYPOINT`, `FillOpaqueStippled`, `GC`, `GCBackground`, `GCForeground`, `LRAND`, `MAX`, `MIN`, `MI_BATCHCOUNT`, `MI_CYCLES`, `MI_DISPLAY`, `MI_HEIGHT`, `MI_INIT`, `MI_NPIXELS`, `MI_PIXEL`, `MI_SCREEN`, `MI_WIDTH`, `MI_WIN_BLACK_PIXEL`, `MI_WIN_HEIGHT`, `MI_WIN_IS_INROOT`, `MI_WIN_WHITE_PIXEL`, `MI_WIN_WIDTH`, `M_PI`, `ModeInfo`, `NRAND`, `NULL`, `None`, `Pixmap`, `Window`, `XClearWindow`, `XColor`, `XCreateGC`, `XCreatePixmap`, `XCreatePixmapCursor`, `XCreatePixmapFromBitmapData`, `XDefineCursor`, `XDrawArc`, `XFillArc`, `XFillRectangle`, `XFillRectangles`, `XFreeCursor`, `XFreeGC`, `XFreePixmap`, `XGCValues`, `XRectangle`, `XSetFillStyle`, `XSetForeground`, `XSetStipple`, `XSetTSOrigin`, `XUndefineCursor`, `bg_gc`, `bit`, `black`, `display`, `error: call to undeclared library function 'atan2' with type 'double (double, double)'; ISO C99 and later do not support implicit function declarations [-Wimplicit-function-declaration]`, `error: call to undeclared library function 'cos' with type 'double (double)'; ISO C99 and later do not support implicit function declarations [-Wimplicit-function-declaration]`, `error: call to undeclared library function 'free' with type 'void (void *)'; ISO C99 and later do not support implicit function declarations [-Wimplicit-function-declaration]`, `error: call to undeclared library function 'memset' with type 'void *(void *, int, unsigned long)'; ISO C99 and later do not support implicit function declarations [-Wimplicit-function-declaration]`, `error: call to undeclared library function 'sin' with type 'double (double)'; ISO C99 and later do not support implicit function declarations [-Wimplicit-function-declaration]`, `error: call to undeclared library function 'sqrt' with type 'double (double)'; ISO C99 and later do not support implicit function declarations [-Wimplicit-function-declaration]`, `error: expected ')'`, `error: expected ';' after top level declarator`, `error: expected expression`, `error: expected function body after function declarator`, `error: expected parameter declarator`, `fg_gc`, `gc`, `gcv`, `new_circle`, `old_circle`, `window`, `xlock.h`, `xp` | pixmaps, float-heavy, needs-xlockmore | 451 |
| kaleidescope | 2d | L | `CapRound`, `GCCapStyle`, `JoinRound`, `LineSolid`, `XDrawSegments`, `XGCValues.cap_style`, `XSegment`, `XSetLineAttributes`, `error: expected expression` | - | 514 |
| kumppa | 2d | L | `XCopyArea`, `XSetGraphicsExposures`, `countof` | pixmaps | 545 |
| lcdscrub | 2d | L | `Pixmap`, `XCreateImage`, `XCreatePixmap`, `XDestroyImage`, `XFreePixmap`, `XGetPixel`, `XImage`, `XPutImage`, `XPutPixel`, `XSetBackground`, `XSetClipMask`, `XYPixmap`, `countof`, `gettimeofday`, `p` | pixmaps, readback, clipmask | 399 |
| lmorph | 2d | L | `CapButt`, `CapRound`, `JoinBevel`, `JoinRound`, `LineSolid`, `XSetLineAttributes` | float-heavy | 580 |
| loop | 2d | L | `XChangeGC`, `XCreatePixmapFromBitmapData`, `XFreePixmap`, `automata.h`, `xlock.h` | needs-xlockmore | 1700 |
| m6502 | 2d | L | `ANALOGTV_BLACK_LEVEL`, `ANALOGTV_BOT`, `ANALOGTV_DEFAULTS`, `ANALOGTV_OPTIONS`, `ANALOGTV_TOP`, `ANALOGTV_VISLINES`, `ANALOGTV_VIS_END`, `ANALOGTV_VIS_LEN`, `ANALOGTV_VIS_START`, `ANALOGTV_WHITE_LEVEL`, `Bit8`, `analogtv`, `analogtv.h`, `analogtv_allocate`, `analogtv_draw`, `analogtv_draw_solid`, `analogtv_input`, `analogtv_input_allocate`, `analogtv_lcp_to_ntsc`, `analogtv_reception`, `analogtv_reception_update`, `analogtv_reconfigure`, `analogtv_release`, `analogtv_set_defaults`, `analogtv_setup_sync`, `asm6502.h`, `countof`, `double_time`, `doubletime.h`, `m6502.h`, `m6502_build`, `m6502_destroy6502`, `m6502_next_eval`, `m6502_start_eval_file`, `m6502_start_eval_string`, `machine_6502` | - | 288 |
| marbling | 2d | L | `DefaultScreenOfDisplay`, `GET_PARENT_OBJ`, `KeyPress`, `KeySym`, `THREAD_DEFAULTS`, `THREAD_OPTIONS`, `XEvent.xany`, `XEvent.xkey`, `XImage`, `XK_Down`, `XK_Left`, `XK_Right`, `XK_Up`, `XLookupString`, `XPutPixel`, `XShmSegmentInfo`, `ZPixmap`, `create_xshm_image`, `destroy_xshm_image`, `error: expected expression`, `error: field has incomplete type 'struct threadpool'`, `error: variable has incomplete type 'const struct threadpool_class'`, `free_colors`, `hardware_concurrency`, `keysym`, `make_smooth_colormap`, `put_xshm_image`, `thread_memory_alignment`, `thread_util.h`, `threadpool_create`, `threadpool_destroy`, `threadpool_run`, `threadpool_wait`, `visual_pixmap_depth`, `xshm.h` | - | 635 |
| maze | 2d | L | `Expose`, `Pixmap`, `XCopyArea`, `XCopyPlane`, `XFreePixmap`, `XGetGeometry`, `XSetBackground`, `XSetClipMask`, `XSetClipOrigin`, `XSetLineAttributes`, `XSync`, `image_data_to_pixmap`, `images/gen/logo-180_png.h`, `images/gen/logo-360_png.h`, `images/gen/logo-50_png.h`, `logo_180_png`, `logo_360_png`, `logo_50_png`, `logo_mask`, `ximage-loader.h` | pixmaps, clipmask | 1681 |
| memscroller | 2d | L | `FcChar8`, `XCopyArea`, `XDrawRectangle`, `XGlyphInfo`, `XImage`, `XShmSegmentInfo`, `XftColor`, `XftColorAllocName`, `XftDraw`, `XftDrawCreate`, `XftDrawDestroy`, `XftDrawStringUtf8`, `XftFont`, `XftFontClose`, `XftTextExtentsUtf8`, `ZPixmap`, `countof`, `create_xshm_image`, `destroy_xshm_image`, `error: expected expression`, `load_xft_font_retry`, `overall`, `put_xshm_image`, `screen_number`, `xft.h`, `xshm.h` | pixmaps | 626 |
| metaballs | 2d | L | `BitmapPad`, `XCreateImage`, `XDestroyImage`, `XFree`, `XImage`, `XListPixmapFormats`, `XParseColor`, `XPutImage`, `XPutPixel`, `XSetWindowBackground`, `ZPixmap` | - | 438 |
| moire | 2d | L | `BlackPixelOfScreen`, `DefaultScreenOfDisplay`, `WhitePixelOfScreen`, `XImage`, `XPutPixel`, `XQueryColor`, `XShmSegmentInfo`, `ZPixmap`, `create_xshm_image`, `destroy_xshm_image`, `make_color_ramp`, `put_xshm_image`, `rgb_to_hsv`, `visual_depth`, `xshm.h` | - | 253 |
| moire2 | 2d | L | `GXor`, `GXxor`, `Pixmap`, `XCopyArea`, `XCopyPlane`, `XCreatePixmap`, `XDrawArc`, `XFreePixmap`, `XSetBackground`, `XSetFunction`, `make_smooth_colormap` | pixmaps, xor | 363 |
| nerverot | 2d | L | `XCopyArea`, `XCreatePixmap`, `make_color_ramp`, `rgb_to_hsv` | pixmaps, float-heavy | 1367 |
| noseguy | 2d | L | `FcChar8`, `None`, `Pixmap`, `XCopyArea`, `XCreateImage`, `XCreatePixmap`, `XDestroyImage`, `XDrawRectangle`, `XFreePixmap`, `XGetImage`, `XGetPixel`, `XGlyphInfo`, `XImage`, `XPutImage`, `XPutPixel`, `XSetClipMask`, `XSetClipOrigin`, `XSetLineAttributes`, `XYPixmap`, `XftColor`, `XftColorAllocName`, `XftDraw`, `XftDrawCreate`, `XftDrawDestroy`, `XftDrawStringUtf8`, `XftFont`, `XftFontClose`, `XftTextExtentsUtf8`, `ZPixmap`, `error: expected ';' after expression`, `error: expected expression`, `extents`, `i1`, `i2`, `images/gen/nose-f1_png.h`, `images/gen/nose-f2_png.h`, `images/gen/nose-f3_png.h`, `images/gen/nose-f4_png.h`, `images/gen/nose-l1_png.h`, `images/gen/nose-l2_png.h`, `images/gen/nose-r1_png.h`, `images/gen/nose-r2_png.h`, `load_xft_font_retry`, `mask`, `nose_f1_png`, `nose_f2_png`, `nose_f3_png`, `nose_f4_png`, `nose_l1_png`, `nose_l2_png`, `nose_r1_png`, `nose_r2_png`, `p2`, `pixmap`, `screen_number`, `text_data`, `textclient.h`, `textclient_close`, `textclient_getc`, `textclient_open`, `textclient_reshape`, `ximage-loader.h` | pixmaps, readback, clipmask | 720 |
| pacman | 2d | L | `XCopyArea`, `XCreatePixmap`, `XDrawArc`, `XDrawString`, `XFreePixmap`, `XGetGeometry`, `XLoadQueryFont`, `XSetClipMask`, `XSetClipOrigin`, `XSetFillStyle`, `XSetLineAttributes`, `xlock.h` | pixmaps, text, clipmask, needs-xlockmore | 1479 |
| penetrate | 2d | L | `FcChar8`, `XDrawArc`, `XGlyphInfo`, `XSetLineAttributes`, `XSync`, `XftColor`, `XftColorAllocName`, `XftDraw`, `XftDrawCreate`, `XftDrawDestroy`, `XftDrawStringUtf8`, `XftFont`, `XftFontClose`, `XftTextExtentsUtf8`, `error: expected expression`, `load_xft_font_retry`, `overall`, `screen_number`, `usleep` | - | 1037 |
| phosphor | 2d | L | `BlackPixelOfScreen`, `CapRound`, `DefaultScreenOfDisplay`, `Expose`, `FALSE`, `FcChar8`, `GCCapStyle`, `KeyPress`, `None`, `Pixmap`, `TTY_BOLD`, `TTY_INVERSE`, `TTY_ITALIC`, `TTY_SYMBOLS`, `Time`, `X11/Intrinsic.h`, `XCopyArea`, `XCopyPlane`, `XCreateImage`, `XCreatePixmap`, `XDestroyImage`, `XEvent.xany`, `XEvent.xkey`, `XFreePixmap`, `XGCValues.cap_style`, `XGetImage`, `XGetPixel`, `XGlyphInfo`, `XImage`, `XPutImage`, `XPutPixel`, `XQueryColor`, `XSetClipMask`, `XSetClipOrigin`, `XWriteBitmapFile`, `XYBitmap`, `XYPixmap`, `XftColor`, `XftDraw`, `XftDrawCreate`, `XftDrawStringUtf8`, `XftFont`, `XftTextExtentsUtf8`, `XtAppAddTimeOut`, `XtAppContext`, `XtIntervalId`, `XtPointer`, `XtRemoveTimeOut`, `ZPixmap`, `ansi-tty.h`, `ansi_graphics_unicode`, `ansi_tty`, `ansi_tty_free`, `ansi_tty_init`, `ansi_tty_print`, `ansi_tty_resize`, `app`, `countof`, `error: expected expression`, `font`, `font_bits`, `im`, `im2`, `images/gen/6x10font_png.h`, `load_xft_font_retry`, `m`, `make_color_ramp`, `mm`, `overall`, `p`, `p2`, `pm_color`, `rgb_to_hsv`, `screen_number`, `tcell`, `text_data`, `textclient.h`, `textclient_close`, `textclient_getc`, `textclient_open`, `textclient_putc_event`, `textclient_puts`, `textclient_reshape`, `tty`, `tty_char`, `utf8_encode`, `utf8_to_latin1`, `utf8wc.h`, `xft_fg`, `xftdraw`, `xim_color`, `xim_mono`, `ximage-loader.h` | pixmaps, readback, clipmask | 1260 |
| piecewise | 2d | L | `Pixmap`, `XArc`, `XCopyArea`, `XCreatePixmap`, `XDrawArcs`, `make_color_loop` | pixmaps | 1036 |
| polyominoes | 2d | L | `XCreateImage`, `XDestroyImage`, `XDrawRectangle`, `XDrawSegments`, `XPutImage`, `XSetLineAttributes`, `xlock.h` | needs-xlockmore | 2370 |
| pong | 2d | L | `ANALOGTV_BLACK_LEVEL`, `ANALOGTV_BOT`, `ANALOGTV_DEFAULTS`, `ANALOGTV_OPTIONS`, `ANALOGTV_TOP`, `ANALOGTV_VISLINES`, `ANALOGTV_VIS_END`, `ANALOGTV_VIS_LEN`, `ANALOGTV_VIS_START`, `ButtonPressMask`, `ButtonRelease`, `ButtonReleaseMask`, `CurrentTime`, `Cursor`, `FocusChangeMask`, `FocusIn`, `FocusOut`, `GrabModeAsync`, `KeyPress`, `KeyPressMask`, `KeyRelease`, `KeyReleaseMask`, `KeySym`, `MotionNotify`, `None`, `Pixmap`, `PointerMotionMask`, `X11/keysym.h`, `XCreatePixmap`, `XCreatePixmapCursor`, `XDefineCursor`, `XDestroyImage`, `XEvent.x`, `XEvent.xkey`, `XEvent.xmotion`, `XGrabPointer`, `XHeightMMOfScreen`, `XHeightOfScreen`, `XK_Down`, `XK_Up`, `XLookupString`, `XQueryPointer`, `XSelectInput`, `XUngrabPointer`, `XWarpPointer`, `analogtv`, `analogtv.h`, `analogtv_allocate`, `analogtv_draw`, `analogtv_draw_solid`, `analogtv_draw_string`, `analogtv_font`, `analogtv_font_set_char`, `analogtv_input`, `analogtv_input_allocate`, `analogtv_lcp_to_ntsc`, `analogtv_make_font`, `analogtv_reception`, `analogtv_reception_update`, `analogtv_reconfigure`, `analogtv_release`, `analogtv_set_defaults`, `analogtv_setup_sync`, `cursor_pix`, `double_time`, `doubletime.h`, `key` | pixmaps | 1109 |
| popsquares | 2d | L | `Pixmap`, `XCopyArea`, `XCreatePixmap`, `XFreePixmap`, `XQueryColor`, `make_color_ramp`, `rgb_to_hsv` | pixmaps | 310 |
| qix | 2d | L | `CellsOfScreen`, `DefaultScreenOfDisplay`, `GCPlaneMask`, `GXxor`, `XGCValues.plane_mask`, `XQueryColor`, `XSetWindowBackground`, `allocate_alpha_colors`, `alpha.h`, `has_writable_cells`, `rgb_to_hsv` | - | 642 |
| rdbomb | 2d | L | `DefaultScreenOfDisplay`, `XImage`, `XListPixmapFormats`, `XPixmapFormatValues`, `XSetWindowBackground`, `XShmSegmentInfo`, `ZPixmap`, `create_xshm_image`, `destroy_xshm_image`, `error: expected ')'`, `error: expected function body after function declarator`, `error: expected parameter declarator`, `error: incompatible integer to pointer conversion passing 'int' to parameter of type 'void *' [-Wint-conversion]`, `error: subscripted value is not an array, pointer, or vector`, `has_writable_cells`, `make_smooth_colormap`, `pfv`, `put_xshm_image`, `visual_depth`, `xshm.h` | - | 571 |
| ripples | 2d | L | `XDestroyImage`, `XGetImage`, `XGetPixel`, `XImage`, `XPutPixel`, `XQueryColor`, `XShmSegmentInfo`, `ZPixmap`, `async_load_state`, `create_xshm_image`, `destroy_xshm_image`, `error: expected expression`, `load_image_async_simple`, `make_smooth_colormap`, `put_xshm_image`, `time`, `time_t`, `visual_rgb_masks`, `xshm.h` | readback | 1127 |
| rocks | 2d | L | `Nonconvex`, `Pixmap`, `XCopyPlane`, `XCreatePixmap`, `XFreePixmap`, `XQueryColor`, `XSetGraphicsExposures`, `p` | pixmaps | 562 |
| rotzoomer | 2d | L | `Pixmap`, `XCreatePixmap`, `XDestroyImage`, `XFreePixmap`, `XGetImage`, `XGetPixel`, `XImage`, `XPutPixel`, `XShmSegmentInfo`, `ZPixmap`, `async_load_state`, `create_xshm_image`, `destroy_xshm_image`, `error: expected expression`, `load_image_async_simple`, `put_xshm_image`, `time`, `time_t`, `xshm.h` | pixmaps, readback | 601 |
| shadebobs | 2d | L | `BlackPixelOfScreen`, `XCreateImage`, `XDestroyImage`, `XFree`, `XGetPixel`, `XImage`, `XListPixmapFormats`, `XParseColor`, `XPutImage`, `XPutPixel`, `XSetWindowBackground`, `ZPixmap` | readback | 474 |
| slidescreen | 2d | L | `Convex`, `XCopyArea`, `XDrawRectangle`, `XFree`, `XParseColor`, `XQueryColors`, `async_load_state`, `error: expected expression`, `load_image_async_simple`, `time`, `time_t`, `visual_cells` | pixmaps | 507 |
| slip | 2d | L | `XCopyArea`, `XCreatePixmap`, `XFreePixmap`, `XSetGraphicsExposures`, `xlock.h` | pixmaps, needs-xlockmore | 376 |
| speedmine | 2d | L | `Nonconvex`, `None`, `Pixmap`, `XCopyArea`, `XCreatePixmap`, `XFreePixmap`, `XQueryColor`, `XSetClipMask`, `free_colors`, `gettimeofday`, `make_color_ramp`, `rgb_to_hsv` | pixmaps, clipmask | 1659 |
| spotlight | 2d | L | `Pixmap`, `XCopyArea`, `XCreatePixmap`, `XDrawRectangle`, `XFreePixmap`, `XSetClipMask`, `XSetClipOrigin`, `async_load_state`, `clip_pm`, `error: expected expression`, `error: incompatible integer to pointer conversion assigning to 'GC' (aka 'struct XshimGC *') from 'int' [-Wint-conversion]`, `error: incompatible pointer to integer conversion passing 'GC' (aka 'struct XshimGC *') to parameter of type 'Drawable' (aka 'unsigned long') [-Wint-conversion]`, `gettimeofday`, `load_image_async_simple`, `time`, `time_t` | pixmaps, clipmask | 355 |
| starfish | 2d | L | `EvenOddRule`, `GCFillRule`, `XGCValues.fill_rule`, `XSetWindowBackground`, `compute_closed_spline`, `error: expected expression`, `error: incompatible integer to pointer conversion assigning to 'int *' from 'int' [-Wint-conversion]`, `error: member reference base type 'int' is not a structure or union`, `error: type name does not allow function specifier to be specified`, `error: type specifier missing, defaults to 'int'; ISO C99 and later do not support implicit int [-Wimplicit-int]`, `free_colors`, `make_smooth_colormap`, `make_spline`, `make_uniform_colormap`, `spline`, `time`, `time_t` | - | 564 |
| strange | 2d | L | `XCopyArea`, `XCopyPlane`, `XCreatePixmap`, `XDrawPoints`, `XFreePixmap`, `XPutPixel`, `XQueryColor`, `XQueryColors`, `XSetBackground`, `XSetFunction`, `XSetGraphicsExposures`, `pow2.h`, `thread_util.h`, `xlock.h`, `xshm.h` | pixmaps, xor, needs-xlockmore | 1353 |
| swirl | 2d | L | `AllocAll`, `Bool`, `Colormap`, `Display`, `DoBlue`, `DoGreen`, `DoRed`, `ENTRYPOINT`, `False`, `LRAND`, `MAXRAND`, `MI_BATCHCOUNT`, `MI_BG_COLOR`, `MI_COLORMAP`, `MI_DISPLAY`, `MI_FG_COLOR`, `MI_GC`, `MI_INIT`, `MI_NPIXELS`, `MI_SATURATION`, `MI_SCREEN`, `MI_VISUAL`, `MI_WINDOW`, `MI_WIN_BLACK_PIXEL`, `MI_WIN_DEPTH`, `MI_WIN_HEIGHT`, `MI_WIN_IS_INWINDOW`, `MI_WIN_WHITE_PIXEL`, `MI_WIN_WIDTH`, `M_PI`, `ModeInfo`, `True`, `Visual`, `Window`, `XAllocColor`, `XClearWindow`, `XColor`, `XCreateColormap`, `XFree`, `XFreeColormap`, `XImage`, `XInstallColormap`, `XPutPixel`, `XQueryColor`, `XSetWMColormapWindows`, `XSetWindowColormap`, `XShmSegmentInfo`, `XStoreColors`, `ZPixmap`, `create_xshm_image`, `dest`, `destroy_xshm_image`, `display`, `done`, `dpy`, `error: call to undeclared library function 'abs' with type 'int (int)'; ISO C99 and later do not support implicit function declarations [-Wimplicit-function-declaration]`, `error: call to undeclared library function 'atan2' with type 'double (double, double)'; ISO C99 and later do not support implicit function declarations [-Wimplicit-function-declaration]`, `error: call to undeclared library function 'calloc' with type 'void *(unsigned long, unsigned long)'; ISO C99 and later do not support implicit function declarations [-Wimplicit-function-declaration]`, `error: call to undeclared library function 'cos' with type 'double (double)'; ISO C99 and later do not support implicit function declarations [-Wimplicit-function-declaration]`, `error: call to undeclared library function 'exp' with type 'double (double)'; ISO C99 and later do not support implicit function declarations [-Wimplicit-function-declaration]`, `error: call to undeclared library function 'fabs' with type 'double (double)'; ISO C99 and later do not support implicit function declarations [-Wimplicit-function-declaration]`, `error: call to undeclared library function 'free' with type 'void (void *)'; ISO C99 and later do not support implicit function declarations [-Wimplicit-function-declaration]`, `error: call to undeclared library function 'sin' with type 'double (double)'; ISO C99 and later do not support implicit function declarations [-Wimplicit-function-declaration]`, `error: call to undeclared library function 'sqrt' with type 'double (double)'; ISO C99 and later do not support implicit function declarations [-Wimplicit-function-declaration]`, `error: expected ')'`, `error: expected ';' after top level declarator`, `error: expected expression`, `error: expected function body after function declarator`, `error: expected parameter declarator`, `hook`, `orbit`, `picasso`, `preserveColors`, `put_xshm_image`, `ray`, `same`, `setColormap`, `setupColormap`, `src`, `truecolor`, `value`, `wheel`, `window`, `xlock.h` | needs-xlockmore | 1447 |
| t3d | 2d | L | `BlackPixelOfScreen`, `Button1Mask`, `Button2Mask`, `Button3Mask`, `GXandInverted`, `GXor`, `KeyPress`, `KeySym`, `Pixmap`, `XAllocColorCells`, `XCopyArea`, `XCreatePixmap`, `XDrawSegments`, `XEvent.xkey`, `XFreePixmap`, `XGetImage`, `XLookupString`, `XPutImage`, `XQueryPointer`, `XStoreColors`, `gettimeofday`, `keysym` | pixmaps, readback, float-heavy | 991 |
| tessellimage | 2d | L | `ButtonRelease`, `Convex`, `ITRIANGLE`, `Pixmap`, `X11/keysymdef.h`, `XCopyArea`, `XCreateImage`, `XCreatePixmap`, `XDestroyImage`, `XEvent.xany`, `XFreePixmap`, `XGetGeometry`, `XGetImage`, `XGetPixel`, `XImage`, `XPutImage`, `XPutPixel`, `XQueryColor`, `XYZ`, `ZPixmap`, `async_load_state`, `countof`, `delaunay`, `delaunay.h`, `delaunay_xyzcompare`, `dimg`, `double_time`, `doubletime.h`, `error: expected expression`, `img2`, `load_image_async_simple`, `p`, `tt`, `v`, `visual_rgb_masks` | pixmaps, readback | 996 |
| testx11 | 2d | L | `BlackPixelOfScreen`, `CapProjecting`, `CapRound`, `Convex`, `GCCapStyle`, `GCFont`, `GXxor`, `KeyPress`, `KeySym`, `NRAND`, `Pixmap`, `XClearArea`, `XCopyArea`, `XCopyPlane`, `XCreatePixmap`, `XCreatePixmapFromBitmapData`, `XDestroyImage`, `XDrawArc`, `XDrawPoints`, `XDrawRectangle`, `XDrawSegments`, `XDrawString`, `XEvent.x`, `XEvent.xany`, `XEvent.xkey`, `XEvent.y`, `XFreePixmap`, `XGCValues.cap_style`, `XGCValues.font`, `XGetImage`, `XImage`, `XLoadFont`, `XLookupString`, `XPutImage`, `XPutPixel`, `XSegment`, `XSetBackground`, `XSetClipMask`, `XSetWindowBackground`, `XSync`, `ZPixmap`, `colorbars.h`, `countof`, `draw_colorbars`, `error: expected ')'`, `error: expected function body after function declarator`, `error: expected parameter declarator`, `get_position`, `get_rotation`, `glx/rotator.h`, `image`, `images/gen/logo-180_png.h`, `keysym`, `lines`, `logo`, `logo_mask`, `make_color_loop`, `make_rotator`, `pixmap`, `rotator`, `seg`, `ximage-loader.h` | pixmaps, readback, text, clipmask | 968 |
| triangle | 2d | L | `Convex`, `CoordModeOrigin`, `Display`, `ENTRYPOINT`, `GC`, `LRAND`, `MAX`, `MAXRAND`, `MIN`, `MI_DISPLAY`, `MI_GC`, `MI_INIT`, `MI_NCOLORS`, `MI_PAUSE`, `MI_SCREEN`, `MI_WINDOW`, `MI_WIN_BLACK_PIXEL`, `MI_WIN_HEIGHT`, `MI_WIN_WHITE_PIXEL`, `MI_WIN_WIDTH`, `M_PI_2`, `ModeInfo`, `NULL`, `Window`, `XClearWindow`, `XDrawLine`, `XFillPolygon`, `XFillRectangle`, `XPoint`, `XSetForeground`, `display`, `error: call to undeclared library function 'atan' with type 'double (double)'; ISO C99 and later do not support implicit function declarations [-Wimplicit-function-declaration]`, `error: expected ')'`, `error: expected ';' after top level declarator`, `error: expected function body after function declarator`, `error: expected parameter declarator`, `gc`, `p`, `window`, `xlock.h` | needs-xlockmore | 355 |
| truchet | 2d | L | `CapRound`, `JoinRound`, `LineSolid`, `Pixmap`, `XCopyArea`, `XCreatePixmap`, `XDrawArc`, `XSetLineAttributes` | pixmaps | 541 |
| twang | 2d | L | `Pixmap`, `XCreatePixmap`, `XDestroyImage`, `XFreePixmap`, `XGetImage`, `XGetPixel`, `XImage`, `XPutPixel`, `XShmSegmentInfo`, `ZPixmap`, `async_load_state`, `create_xshm_image`, `destroy_xshm_image`, `error: expected expression`, `load_image_async_simple`, `put_xshm_image`, `time`, `time_t`, `xshm.h` | pixmaps, readback | 791 |
| vfeedback | 2d | L | `ANALOGTV_DEFAULTS`, `ANALOGTV_OPTIONS`, `ANALOGTV_SIGNAL_LEN`, `BlackPixelOfScreen`, `Button1`, `Button2`, `Button3`, `Button4`, `Button5`, `Button6`, `Button7`, `ButtonRelease`, `EASE_IN_OUT_SINE`, `KeyPress`, `KeySym`, `MotionNotify`, `Pixmap`, `PointerMotionMask`, `RANDSIGN`, `XCopyArea`, `XCreateImage`, `XCreatePixmap`, `XDestroyImage`, `XEvent.state`, `XEvent.x`, `XEvent.xany`, `XEvent.xkey`, `XEvent.xmotion`, `XEvent.y`, `XFreePixmap`, `XGetImage`, `XGetPixel`, `XImage`, `XK_Down`, `XK_Left`, `XK_Right`, `XK_Up`, `XLookupString`, `XPutPixel`, `XSelectInput`, `XWindowAttributes.your_event_mask`, `ZPixmap`, `analogtv`, `analogtv.h`, `analogtv_allocate`, `analogtv_draw`, `analogtv_input_allocate`, `analogtv_load_ximage`, `analogtv_reception`, `analogtv_reception_update`, `analogtv_reconfigure`, `analogtv_release`, `analogtv_set_defaults`, `analogtv_setup_sync`, `double_time`, `doubletime.h`, `ease`, `easing.h`, `img`, `in`, `keysym`, `out` | pixmaps, readback | 592 |
| wander | 2d | L | `NRAND`, `Pixmap`, `XCopyArea`, `XCreatePixmap`, `free_colors`, `make_color_loop` | pixmaps | 284 |
| whirlygig | 2d | L | `Pixmap`, `XCopyArea`, `XCreatePixmap`, `XDrawString`, `make_uniform_colormap` | pixmaps, text, float-heavy | 741 |
| worm | 2d | L | `COSF`, `Display`, `ENTRYPOINT`, `False`, `GC`, `GXcopy`, `GXor`, `LRAND`, `MI_BATCHCOUNT`, `MI_CYCLES`, `MI_DELTA3D`, `MI_DISPLAY`, `MI_GC`, `MI_INIT`, `MI_LEFT_COLOR`, `MI_NONE_COLOR`, `MI_NPIXELS`, `MI_PIXEL`, `MI_RIGHT_COLOR`, `MI_SCREEN`, `MI_SIZE`, `MI_WINDOW`, `MI_WIN_HEIGHT`, `MI_WIN_IS_INSTALL`, `MI_WIN_IS_USE3D`, `MI_WIN_WHITE_PIXEL`, `MI_WIN_WIDTH`, `M_PI`, `ModeInfo`, `NRAND`, `NULL`, `NUMCOLORS`, `SINF`, `Window`, `XClearArea`, `XClearWindow`, `XFillRectangle`, `XFillRectangles`, `XPoint`, `XRectangle`, `XSetForeground`, `XSetFunction`, `display`, `error: call to undeclared library function 'free' with type 'void (void *)'; ISO C99 and later do not support implicit function declarations [-Wimplicit-function-declaration]`, `error: call to undeclared library function 'malloc' with type 'void *(unsigned long)'; ISO C99 and later do not support implicit function declarations [-Wimplicit-function-declaration]`, `error: call to undeclared library function 'memset' with type 'void *(void *, int, unsigned long)'; ISO C99 and later do not support implicit function declarations [-Wimplicit-function-declaration]`, `error: call to undeclared library function 'sqrt' with type 'double (double)'; ISO C99 and later do not support implicit function declarations [-Wimplicit-function-declaration]`, `error: expected ')'`, `error: expected ';' after top level declarator`, `error: expected expression`, `error: expected function body after function declarator`, `error: expected parameter declarator`, `gc`, `window`, `xlock.h` | xor, needs-xlockmore | 434 |
| wormhole | 2d | L | `CapRound`, `JoinRound`, `LineSolid`, `Pixmap`, `XCopyArea`, `XCreatePixmap`, `XFreePixmap`, `XSetLineAttributes` | pixmaps | 734 |
| xanalogtv | 2d | L | `ANALOGTV_DEFAULTS`, `ANALOGTV_OPTIONS`, `ANALOGTV_PIC_LEN`, `ANALOGTV_SCALE`, `ANALOGTV_SIGNAL_LEN`, `ANALOGTV_V`, `ANALOGTV_VIS_LEN`, `ANALOGTV_VIS_START`, `KeyPress`, `KeySym`, `Pixmap`, `X11/Intrinsic.h`, `XCreatePixmap`, `XDestroyImage`, `XEvent.xkey`, `XFreePixmap`, `XGetImage`, `XImage`, `XK_Down`, `XK_Left`, `XK_Next`, `XK_Prior`, `XK_Right`, `XK_Up`, `XLookupString`, `XrmDatabase`, `XrmPutResource`, `XrmValue`, `ZPixmap`, `analogtv`, `analogtv.h`, `analogtv_allocate`, `analogtv_draw`, `analogtv_draw_solid_rel_lcp`, `analogtv_draw_string_centered`, `analogtv_font`, `analogtv_input`, `analogtv_input_allocate`, `analogtv_lcp_to_ntsc`, `analogtv_load_ximage`, `analogtv_make_font`, `analogtv_reception`, `analogtv_reception_update`, `analogtv_reconfigure`, `analogtv_release`, `analogtv_set_defaults`, `analogtv_setup_sync`, `analogtv_setup_teletext`, `db`, `error: expected expression`, `error: incompatible integer to pointer conversion initializing 'struct tm *' with an expression of type 'int' [-Wint-conversion]`, `gethostname`, `gettimeofday`, `image`, `image_data_to_pixmap`, `images/gen/logo-180_png.h`, `images/gen/testcard_bbcf_png.h`, `images/gen/testcard_pm5544_png.h`, `images/gen/testcard_rca_png.h`, `inp`, `input`, `keysym`, `load_image_async`, `localtime`, `mask`, `p`, `rec`, `strftime`, `testcard_bbcf_png`, `testcard_pm5544_png`, `testcard_rca_png`, `time`, `time_t`, `value`, `ximage`, `ximage-loader.h` | pixmaps, readback | 689 |
| xflame | 2d | L | `XCreateImage`, `XDestroyImage`, `XGetPixel`, `XImage`, `XPutPixel`, `XQueryColor`, `XShmSegmentInfo`, `ZPixmap`, `bob_png`, `create_xshm_image`, `destroy_xshm_image`, `file_to_ximage`, `image`, `image_data_to_ximage`, `images/gen/bob_png.h`, `out`, `put_xshm_image`, `ximage-loader.h`, `xshm.h` | readback | 826 |
| xjack | 2d | L | `FcChar8`, `XClearArea`, `XCopyArea`, `XEvent.xany`, `XGlyphInfo`, `XftColor`, `XftColorAllocName`, `XftDraw`, `XftDrawCreate`, `XftDrawDestroy`, `XftDrawStringUtf8`, `XftFont`, `XftFontClose`, `XftTextExtentsUtf8`, `error: expected expression`, `load_xft_font_retry`, `overall`, `screen_number` | pixmaps | 508 |
| xlyap | 2d | L | `BlackPixelOfScreen`, `Cursor`, `KeyPress`, `KeySym`, `WhitePixelOfScreen`, `X11/cursorfont.h`, `XComposeStatus`, `XCopyArea`, `XCreatePixmap`, `XDrawPoints`, `XEvent.xkey`, `XFreePixmap`, `XGetGeometry`, `XKeyEvent`, `XLookupString`, `XPending`, `XStoreColors`, `countof`, `error: expected expression`, `free_colors`, `make_smooth_colormap`, `yarandom.h` | pixmaps | 1939 |
| xmatrix | 2d | L | `KeyPress`, `KeySym`, `Pixmap`, `X11/Intrinsic.h`, `XChangeGC`, `XCopyArea`, `XCreateImage`, `XCreatePixmap`, `XDestroyImage`, `XDrawRectangle`, `XEvent.xany`, `XEvent.xkey`, `XFreePixmap`, `XGetImage`, `XGetPixel`, `XImage`, `XLookupString`, `XPutImage`, `XPutPixel`, `XYPixmap`, `XtAppAddTimeOut`, `XtAppContext`, `XtIntervalId`, `XtPointer`, `XtRemoveTimeOut`, `ZPixmap`, `app`, `countof`, `error: expected ';' after expression`, `i1`, `i2`, `im`, `image_data_to_pixmap`, `images/gen/matrix1_png.h`, `images/gen/matrix1b_png.h`, `images/gen/matrix2_png.h`, `images/gen/matrix2b_png.h`, `keysym`, `matrix1_png`, `matrix1b_png`, `matrix2_png`, `matrix2b_png`, `p2`, `text_data`, `textclient.h`, `textclient_close`, `textclient_getc`, `textclient_open`, `textclient_reshape`, `ximage-loader.h` | pixmaps, readback | 1915 |
| zoom | 2d | L | `Pixmap`, `XCopyArea`, `XCreatePixmap`, `XDestroyImage`, `XFreePixmap`, `XGetImage`, `XGetPixel`, `XImage`, `XSetWindowBackground`, `ZPixmap`, `async_load_state`, `error: expected expression`, `gettimeofday`, `load_image_async_simple`, `time`, `time_t` | pixmaps, readback | 290 |
| apollonian | 2d | M | `XDrawArc`, `XQueryColor`, `xlock.h` | needs-xlockmore | 820 |
| blaster | 2d | M | `XArc`, `XFillArcs`, `error: expected expression` | - | 1208 |
| braid | 2d | M | `XSetLineAttributes`, `xlock.h` | needs-xlockmore | 444 |
| cloudlife | 2d | M | `XDrawPoints`, `make_smooth_colormap` | - | 440 |
| coral | 2d | M | `free_colors`, `make_uniform_colormap` | - | 328 |
| critical | 2d | M | `XChangeGC`, `free_colors`, `make_smooth_colormap`, `make_uniform_colormap` | - | 462 |
| discrete | 2d | M | `XDrawPoints`, `xlock.h` | needs-xlockmore | 442 |
| drift | 2d | M | `XDrawPoints`, `xlock.h` | needs-xlockmore | 674 |
| euler2d | 2d | M | `XDrawArc`, `XDrawSegments`, `XSetLineAttributes`, `xlock.h` | float-heavy, needs-xlockmore | 893 |
| fadeplot | 2d | M | `xlock.h` | needs-xlockmore | 243 |
| flame | 2d | M | `make_smooth_colormap` | - | 457 |
| grav | 2d | M | `XDrawArc`, `xlock.h` | needs-xlockmore | 360 |
| hopalong | 2d | M | `xlock.h` | needs-xlockmore | 563 |
| hyperball | 2d | M | `Expose`, `UnmapNotify` | - | 2464 |
| interaggregate | 2d | M | `XParseColor` | - | 989 |
| laser | 2d | M | `XChangeGC`, `xlock.h` | needs-xlockmore | 356 |
| lightning | 2d | M | `xlock.h` | needs-xlockmore | 602 |
| lisa | 2d | M | `XDrawPoints`, `XMaxRequestSize`, `XSetLineAttributes`, `xlock.h` | needs-xlockmore | 744 |
| lissie | 2d | M | `XDrawArc`, `xlock.h` | needs-xlockmore | 323 |
| mountain | 2d | M | `xlock.h` | needs-xlockmore | 283 |
| munch | 2d | M | `GXxor`, `XSetFunction`, `i_log2`, `pow2.h` | xor | 462 |
| penrose | 2d | M | `XSetLineAttributes`, `xlock.h` | needs-xlockmore | 1342 |
| rotor | 2d | M | `XSetLineAttributes`, `xlock.h` | needs-xlockmore | 394 |
| scooter | 2d | M | `XSetLineAttributes`, `xlock.h` | needs-xlockmore | 975 |
| sierpinski | 2d | M | `XDrawPoints`, `xlock.h` | needs-xlockmore | 215 |
| sphere | 2d | M | `XDrawPoints`, `xlock.h` | needs-xlockmore | 304 |
| spiral | 2d | M | `xlock.h` | needs-xlockmore | 331 |
| squiral | 2d | M | `free_colors`, `make_uniform_colormap`, `yarandom.h` | - | 335 |
| substrate | 2d | M | `XParseColor` | - | 780 |
| thornbird | 2d | M | `xlock.h` | needs-xlockmore | 270 |
| vermiculate | 2d | M | `XSetWindowBackground`, `ya_random` | - | 1229 |
| vines | 2d | M | `xlock.h` | needs-xlockmore | 190 |
| whirlwindwarp | 2d | M | `XDrawRectangle`, `gettimeofday` | - | 509 |
| xrayswarm | 2d | M | `XSetGraphicsExposures`, `initTime` | - | 1235 |
| helix | 2d | S | - | - | 358 |
| hypercube | 2d | S | - | - | 576 |
| pedal | 2d | S | - | - | 339 |
| petri | 2d | S | - | - | 780 |
| pyro | 2d | S | - | - | 373 |
| rorschach | 2d | S | - | - | 227 |
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
