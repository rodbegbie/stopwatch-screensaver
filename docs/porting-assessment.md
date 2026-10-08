# Porting assessment

Rough effort to port each xscreensaver hack to the StopWatch X11 shim,
generated from xscreensaver 6.16 by `tools/score_hacks.py`. Regenerate
it with `uv run tools/score_hacks.py`.

## How to read this

- **408 hacks** were scanned: every `hacks/*.c` and `hacks/glx/*.c`.
- The scan is static and heuristic. For each 2D hack it counts the Xlib
  calls in the source that the shim does not declare, and it also
  syntax-checks the unmodified source against the shim's headers
  (`cc -fsyntax-only`). Anything the compiler cannot find is a **shim
  gap**: a missing header, type, struct field, constant or function.
  That second check catches helpers hacks reach through `utils/` that a
  count of Xlib calls cannot see. It does not run anything, so treat the
  effort ratings as a prioritisation aid, not an estimate.
- Many files in `hacks/` are shared helpers or support code rather than
  hacks, so the totals overstate the number of distinct screensavers.

## Effort ratings

| Rating | Meaning | Count |
| --- | --- | --- |
| S | 2D, and the unmodified source compiles against the shim | 6 |
| M | 2D, 1-4 shim gaps, no pixmaps or pixel read-back | 35 |
| L | 2D, 5+ shim gaps, or uses pixmaps or pixel read-back | 120 |
| XL | GL: needs a software rasteriser (see below) | 247 |

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

Four hacks have been run so far (default settings, 466×466 canvas pushed to
the display every frame, canvas held in PSRAM).

| Hack | Frame rate | Extra PSRAM while running |
| --- | --- | --- |
| Pyro | about 23 fps | about 80 KB |
| HyperCube | about 22 fps | none measurable |
| Petri | about 22-24 fps | about 1.3 MB |
| XSpirograph | about 9.6 fps | none measurable |

Free heap and free PSRAM return to exactly the same values every time a
hack is switched back to, so switching does not leak.

Pushing the 434 KB canvas to the display takes most of a frame, so about 23
fps is the ceiling for any hack today. XSpirograph is the exception that
proves it: it draws 1000 lines per frame, and writing those pixels into the
PSRAM canvas is slow, which halves its frame rate. Hacks that draw many
primitives per frame are the ones to profile first.

Pyro's `init` builds two 6284-entry sine and cosine tables in double
precision, so restarting it dips to about 17 fps for a few seconds.

## Suggested order for shim stage 2

Shim gaps across 2D hacks, ranked so gaps that block hacks
needing few additions come first.

| Gap | Score | 2D hacks needing it |
| --- | --- | --- |
| xlock.h | 13.36 | 40 |
| screenhack_event_helper | 5.39 | 45 |
| XSetLineAttributes | 4.51 | 27 |
| XCreatePixmap | 4.46 | 63 |
| XCopyArea | 4.42 | 52 |
| XFreePixmap | 3.87 | 59 |
| Pixmap | 3.58 | 57 |
| XFillRectangles | 3.17 | 15 |
| XGCValues.background | 2.92 | 31 |
| make_smooth_colormap | 2.84 | 26 |

## All hacks

| Hack | Kind | Effort | Shim gaps | Flags | LOC |
| --- | --- | --- | --- | --- | --- |
| abstractile | 2d | L | BlackPixelOfScreen, Convex, countof, make_color_loop, make_color_ramp, make_smooth_colormap, make_uniform_colormap, rgb_to_hsv, screenhack_event_helper | - | 1625 |
| analogtv | 2d | L | ANALOGTV_BLACK_LEVEL, ANALOGTV_BLANK_LEVEL, ANALOGTV_BOT, ANALOGTV_BP_START, ANALOGTV_CB_LEVEL, ANALOGTV_CB_START, ANALOGTV_CV_MAX, ANALOGTV_FP_START, ANALOGTV_GHOSTFIR_LEN, ANALOGTV_H, ANALOGTV_MAX_LINEHEIGHT, ANALOGTV_PIC_END, ANALOGTV_PIC_LEN, ANALOGTV_PIC_START, ANALOGTV_SCALE, ANALOGTV_SIGNAL_LEN, ANALOGTV_SYNC_LEVEL, ANALOGTV_SYNC_START, ANALOGTV_TOP, ANALOGTV_V, ANALOGTV_VISLINES, ANALOGTV_VIS_LEN, ANALOGTV_VIS_START, ANALOGTV_WHITE_LEVEL, BlackPixelOfScreen, DefaultScreenOfDisplay, DirectColor, FcChar8, GCBackground, GET_PARENT_OBJ, LSBFirst, MSBFirst, Pixmap, PseudoColor, StaticColor, TrueColor, X11/Xlib.h, X11/Xutil.h, XClearArea, XCreateImage, XCreatePixmap, XDestroyImage, XDrawString, XFreePixmap, XGCValues.background, XGetImage, XGetPixel, XImage, XPutPixel, XQueryColors, XSetWindowBackground, XWriteBitmapFile, XYBitmap, XYPixmap, XftColor, XftDraw, XftDrawCreate, XftDrawDestroy, XftDrawStringUtf8, XftFont, ZPixmap, analogtv, analogtv.h, analogtv_font, analogtv_input, analogtv_reception, create_xshm_image, destroy_xshm_image, dp, font, font-retry.h, frand, grabclient.h, hardware_concurrency, im, images/gen/6x10font_png.h, inp, it, load_xft_font_retry, m, mm, p, put_xshm_image, resources.h, ret, screen_number, text_pm, thread_free, thread_malloc, thread_memory_alignment, threadpool_create, threadpool_destroy, threadpool_run, threadpool_wait, visual.h, visual_class, visual_pixmap_depth, visual_rgb_masks, xft.h, xft_fg, xftdraw, xim, ximage-loader.h, yarandom.h | pixmaps, readback, text | 2454 |
| analogtv-cli | 2d | L | ANALOGTV_SIGNAL_LEN, Bool, Colormap, Display, DoBlue, DoGreen, DoRed, Drawable, False, GC, LSBFirst, Pixmap, RANDSIGN, Screen, Status, True, TrueColor, Visual, Window, X11/Xlib.h, X11/Xos.h, X11/Xutil.h, XClearArea, XColor, XCreateImage, XCreatePixmap, XCreatePixmapFromBitmapData, XDestroyImage, XDrawString, XFreePixmap, XGCValues, XGetImage, XGetPixel, XImage, XInitImage, XPutImage, XPutPixel, XQueryColor, XQueryColors, XSetWindowBackground, XShmSegmentInfo, XWindowAttributes, ZPixmap, analogtv, analogtv.h, analogtv_allocate, analogtv_draw, analogtv_draw_solid_rel_lcp, analogtv_font, analogtv_input, analogtv_input_allocate, analogtv_lcp_to_ntsc, analogtv_load_ximage, analogtv_reception, analogtv_reception_update, analogtv_set_defaults, analogtv_setup_sync, base_image, countof, dpy, ffmpeg-out.h, ffmpeg_out_add_frame, ffmpeg_out_close, ffmpeg_out_init, ffmpeg_out_state, ffst, file_to_ximage, font-retry.h, frand, image, inp, input, out, powerp, progname, rec, resources.h, screen, screenhackI.h, thread_free, thread_malloc, thread_util.h, time, visual, visual.h, visual_pixmap_depth, visual_rgb_masks, window, ximage, ximage-loader.h, ximage2, ximages, xshm.h, ya_rand_init, yarandom.h | pixmaps, readback, text | 1121 |
| anemone | 2d | L | CapRound, JoinBevel, LineSolid, Pixmap, XCopyArea, XCreatePixmap, XFreePixmap, XSetLineAttributes, make_smooth_colormap | pixmaps | 458 |
| anemotaxis | 2d | L | CapRound, JoinRound, LineSolid, Pixmap, XCopyArea, XCreatePixmap, XSetLineAttributes | pixmaps | 760 |
| ansi-tty | 2d | L | TTY_BLINK, TTY_BOLD, TTY_DIM, TTY_INVERSE, TTY_ITALIC, TTY_SYMBOLS, TTY_UNDERLINE, ansi-tty.h, ansi_tty, blurb.h, c, ch, end, gg, grid2, progname, tty, tty_char, tty_color, tty_flag, tty_state, utf8_decode, utf8wc.h | - | 1952 |
| ant | 2d | L | XChangeGC, XCreatePixmapFromBitmapData, XDrawArc, XFreePixmap, XSetLineAttributes, automata.h, xlock.h | needs-xlockmore | 1351 |
| apple2 | 2d | L | A2CONTROLLER_DONE, A2CONTROLLER_FREE, A2_GR_FULL, A2_GR_HIRES, A2_GR_LORES, ANALOGTV_BLACK_LEVEL, ANALOGTV_PIC_START, ANALOGTV_TOP, ANALOGTV_WHITE_LEVEL, BlackPixelOfScreen, DefaultScreenOfDisplay, Display, NULL, Pixmap, Window, XClearWindow, XCreateImage, XCreatePixmap, XDestroyImage, XDrawString, XFreePixmap, XGetImage, XGetPixel, XGetWindowAttributes, XImage, XLoadQueryFont, XPutPixel, XWindowAttributes, XWriteBitmapFile, XYBitmap, XYPixmap, ZPixmap, a2_cls, a2_goto, analogtv_allocate, analogtv_draw, analogtv_input_allocate, analogtv_reception, analogtv_reception_update, analogtv_release, analogtv_set_defaults, analogtv_setup_frame, analogtv_setup_sync, apple2.h, apple2_sim_t, apple2_state_t, gettimeofday, im, images/gen/apple2font_png.h, m, mm, p, random, screenhackI.h, sim, st, xgwa, ximage-loader.h | pixmaps, readback, text | 886 |
| apple2-main | 2d | L | A2CONTROLLER_DONE, A2CONTROLLER_FREE, A2_GR_FULL, A2_GR_HIRES, A2_GR_LORES, ANALOGTV_DEFAULTS, ANALOGTV_OPTIONS, DisplayOfScreen, GrayScale, KeyPress, Pixmap, PseudoColor, TTY_BLINK, TTY_BOLD, TTY_INVERSE, TTY_SYMBOLS, XCreatePixmap, XDestroyImage, XEvent.xany, XEvent.xkey, XFreePixmap, XGetImage, XGetPixel, XImage, XQueryColors, ZPixmap, a2_clear_gr, a2_clear_hgr, a2_cls, a2_display_image_loading, a2_goto, a2_hline, a2_hplot, a2_invalidate, a2_plot, a2_printc, a2_printc_noscroll, a2_prints, analogtv_reconfigure, ansi-tty.h, ansi_tty, ansi_tty_free, ansi_tty_init, ansi_tty_print, apple2.h, apple2_one_frame, apple2_sim_t, apple2_start, apple2_state_t, countof, flag, image, load_image_async, p, sim, st, tc, text_data, textclient.h, textclient_close, textclient_getc, textclient_open, textclient_putc_event, textclient_puts, textclient_reshape, time, time_t, tty, tty_char, tty_flag, utf8_encode, utf8_to_latin1, utf8wc.h, visual_cells, visual_class, visual_rgb_masks | pixmaps, readback | 1642 |
| asm6502 | 2d | L | ABS_LABEL_X, ABS_LABEL_Y, ABS_OR_BRANCH, ABS_VALUE, ABS_X, ABS_Y, Bit16, Bit32, Bit8, DCB_PARAM, FALSE, IMMEDIATE_GREAT, IMMEDIATE_LESS, IMMEDIATE_VALUE, INDIRECT_X, INDIRECT_Y, MAX_CMD_LEN, MAX_LABEL_LEN, MAX_PARAM_VALUE, MEM_64K, NUM_OPCODES, PROG_START, SINGLE, STACK_BOTTOM, STACK_TOP, TRUE, ZERO, ZERO_X, ZERO_Y, address, adm, al, ar, asm6502.h, bl, br, c, cf, currAddr, i, idx, m6502_AddrMode, m6502_Opcodes, m6502_Plotter, machine, machine_6502, mask, nl, nr, offMask, oldDefault, onMask, op, opcode, pc, tmp, val, value, w, x, y, yarandom.h, zp | - | 2275 |
| attraction | 2d | L | ButtonRelease, CapButt, CapRound, Convex, GCCapStyle, XDrawRectangle, XEvent.x, XEvent.xany, XEvent.y, XGCValues.cap_style, XQueryPointer, compute_closed_spline, free_spline, make_color_ramp, make_smooth_colormap, make_spline, spline | - | 1115 |
| barcode | 2d | L | ButtonRelease, GCBackground, LSBFirst, XCreateImage, XDestroyImage, XEvent.xany, XGCValues.background, XImage, XPutImage, XYBitmap | - | 2055 |
| binaryhorizon | 2d | L | KeyPress, Pixmap, XCopyArea, XCreatePixmap, XDestroyImage, XEvent.xany, XFreePixmap, XGetImage, XImage, XPutImage, XPutPixel, ZPixmap, time, time_t, visual_depth | pixmaps, readback | 624 |
| binaryring | 2d | L | KeyPress, Pixmap, XCopyArea, XCreatePixmap, XDestroyImage, XEvent.xany, XFreePixmap, XGetImage, XImage, XPutImage, XPutPixel, ZPixmap, visual_depth | pixmaps, readback | 577 |
| blitspin | 2d | L | GCBackground, GXand, GXclear, GXor, GXset, GXxor, Pixmap, XCopyArea, XCopyPlane, XCreatePixmap, XDestroyImage, XDisplayHeight, XDisplayWidth, XFreePixmap, XGCValues.background, XGetImage, XPutImage, XScreenNumberOfScreen, XSetClipMask, async_load_state, file_to_pixmap, images/gen/som_png.h, load_image_async_simple, mask, pixmap, pow2.h, screenhack_event_helper, to_pow2, ximage-loader.h | pixmaps, readback, clipmask | 467 |
| bouboule | 2d | L | Display, GC, GXcopy, GXor, MAX, MIN, MI_BATCHCOUNT, MI_DELTA3D, MI_DISPLAY, MI_GC, MI_HEIGHT, MI_INIT, MI_LEFT_COLOR, MI_NONE_COLOR, MI_NPIXELS, MI_PIXEL, MI_RIGHT_COLOR, MI_SCREEN, MI_SIZE, MI_WIDTH, MI_WINDOW, MI_WIN_BLACK_PIXEL, MI_WIN_HEIGHT, MI_WIN_IS_INSTALL, MI_WIN_IS_USE3D, MI_WIN_WHITE_PIXEL, MI_WIN_WIDTH, M_PI, ModeInfo, ModeSpecOpt, NRAND, NULL, Window, XArc, XClearWindow, XFillArcs, XFillRectangle, XSetForeground, XSetFunction, arc, arcleft, display, gc, oarc, oarcleft, window, xlock.h | xor, needs-xlockmore | 860 |
| boxfit | 2d | L | GCBackground, Pixmap, XCreatePixmap, XDestroyImage, XDrawArc, XDrawRectangle, XFreePixmap, XGCValues.background, XGetImage, XGetPixel, XImage, XSetWindowBackground, ZPixmap, async_load_state, free_colors, load_image_async_simple, make_smooth_colormap, screenhack_event_helper, ximage-loader.h | pixmaps, readback | 573 |
| bsod | 2d | L | A2CONTROLLER_DONE, A2CONTROLLER_FREE, A2_GR_FULL, A2_GR_HIRES, A2_GR_LORES, ANALOGTV_DEFAULTS, ANALOGTV_OPTIONS, CapButt, CapRound, FcChar8, GCBackground, JoinMiter, LineSolid, None, Pixmap, XChangeGC, XClearArea, XColor.color, XCopyArea, XCopyPlane, XCreateImage, XCreatePixmap, XCreatePixmapFromBitmapData, XDestroyImage, XDrawRectangle, XFetchName, XFreePixmap, XGCValues.background, XGetImage, XGetPixel, XGlyphInfo, XImage, XPutImage, XPutPixel, XQueryColor, XSetBackground, XSetClipMask, XSetClipOrigin, XSetLineAttributes, XSetPlaneMask, XSetWindowBackground, XStoreName, XYPixmap, XftColor, XftDraw, XftDrawCreate, XftDrawDestroy, XftDrawStringUtf8, XftFont, XftFontClose, XftTextExtentsUtf8, XftTextExtentsUtf8_multi, ZPixmap, a2_cls, a2_goto, a2_init_memory_active, a2_invalidate, a2_poke, a2_printc, a2_printc_noscroll, a2_prints, amiga_png, android_png, apple2.h, apple2_one_frame, apple2_sim_t, apple2_start, apple2_state_t, async_load_state, cswap, em, font, gnome1_png, gnome2_png, hmac_png, i1, i2, image_data_to_pixmap, images/gen/amiga_png.h, images/gen/android_png.h, images/gen/apple_png.h, images/gen/atari_png.h, images/gen/atm_png.h, images/gen/dvd_png.h, images/gen/gnome1_png.h, images/gen/gnome2_png.h, images/gen/hmac_png.h, images/gen/mac_png.h, images/gen/macbomb_png.h, images/gen/osx_10_2_png.h, images/gen/osx_10_3_png.h, images/gen/ransomware_png.h, images/gen/sun_png.h, load_image_async_simple, load_xft_font_retry, mask, osx_10_2_png, osx_10_3_png, ov, ov2, p2, pixmap, screen_number, screenhack_event_helper, sim, st, utf8_decode_combining, utf8wc.h, xft.h, xft_word_wrap, xftwrap.h, ximage-loader.h | pixmaps, readback, clipmask | 7809 |
| bubbles | 2d | L | BUBBLE_MAGIC, Bubble, Bubble_Step, DELETE_BUBBLE, KEEP_BUBBLE, MAX, MAX_DROPPAGE, MIN, XCopyArea, XDrawArc, XFreePixmap, XSetClipMask, XSetClipOrigin, bubbles.h, default_bubbles, head, image_data_to_pixmap, init_default_bubbles, least, newpix, nextbub, num_default_bubbles, pixmap_list, rv, tmp, tmppix, touch, ximage-loader.h, yarandom.h | pixmaps, clipmask | 1468 |
| bubbles-default | 2d | L | bubbles.h, images/gen/blood10_png.h, images/gen/blood11_png.h, images/gen/blood1_png.h, images/gen/blood2_png.h, images/gen/blood3_png.h, images/gen/blood4_png.h, images/gen/blood5_png.h, images/gen/blood6_png.h, images/gen/blood7_png.h, images/gen/blood8_png.h, images/gen/blood9_png.h, images/gen/blue10_png.h, images/gen/blue11_png.h, images/gen/blue1_png.h, images/gen/blue2_png.h, images/gen/blue3_png.h, images/gen/blue4_png.h, images/gen/blue5_png.h, images/gen/blue6_png.h, images/gen/blue7_png.h, images/gen/blue8_png.h, images/gen/blue9_png.h, images/gen/glass1_png.h, yarandom.h | - | 155 |
| bumps | 2d | L | Pixmap, XCreatePixmap, XDestroyImage, XGetImage, XGetPixel, XImage, XParseColor, XQueryColors, XSetWindowBackground, XShmSegmentInfo, XSync, ZPixmap, async_load_state, create_xshm_image, destroy_xshm_image, load_image_async_simple, pScreenImage, put_xshm_image, screenhack_event_helper, xshm.h | pixmaps, readback | 705 |
| ccurve | 2d | L | GCBackground, Pixmap, XCopyArea, XCreatePixmap, XFreePixmap, XGCValues.background, make_color_loop, screenhack_event_helper | pixmaps | 872 |
| celtic | 2d | L | CapRound, GCBackground, GCCapStyle, JoinRound, LineSolid, XDrawArc, XGCValues.background, XGCValues.cap_style, XSetLineAttributes, make_smooth_colormap, screenhack_event_helper | - | 1141 |
| compass | 2d | L | Convex, GCJoinStyle, JoinBevel, Pixmap, XCopyArea, XCreatePixmap, XDrawSegments, XFreePixmap, XGCValues.join_style, XSegment, countof, segs | pixmaps, float-heavy | 999 |
| crystal | 2d | L | Bool, Colormap, Convex, CoordModeOrigin, Display, ENTRYPOINT, False, GC, GXcopy, GXxor, LRAND, MAX, MIN, MI_BG_PIXEL, MI_BLACK_PIXEL, MI_CLEARWINDOW, MI_COUNT, MI_DISPLAY, MI_FG_PIXEL, MI_HEIGHT, MI_INIT, MI_IS_DRAWN, MI_IS_FULLRANDOM, MI_IS_INSTALL, MI_IS_VERBOSE, MI_NCOLORS, MI_NPIXELS, MI_PIXEL, MI_SCREEN, MI_SIZE, MI_VISUAL, MI_WHITE_PIXEL, MI_WIDTH, MI_WINDOW, M_PI, ModeInfo, NULL, None, OptionStruct, True, Window, XAllocColor, XColor, XCreateColormap, XCreateGC, XDrawLine, XFillPolygon, XFreeColormap, XFreeGC, XGCValues, XInstallColormap, XParseColor, XPoint, XSetForeground, XSetFunction, XSetWindowColormap, XrmOptionDescRec, XrmoptionNoArg, XrmoptionSepArg, argtype, color, color.h, display, fprintf, free_colors, has_writable_cells, make_random_colormap, make_smooth_colormap, make_uniform_colormap, new_xy, rotate_colors, stdout, t_Bool, t_Int, window, xlock.h, xy, xy1, xy_1 | xor, float-heavy, needs-xlockmore | 1286 |
| cwaves | 2d | L | BlackPixelOfScreen, CapRound, JoinRound, LineSolid, XSetLineAttributes, make_smooth_colormap, screenhack_event_helper, ximage-loader.h | - | 219 |
| cynosure | 2d | L | XCreateBitmapFromData, XDrawRectangle, XFreePixmap, XSetWindowBackground, make_smooth_colormap, rgb_to_hsv, screenhack_event_helper | - | 457 |
| decayscreen | 2d | L | Pixmap, XCopyArea, XCreatePixmap, async_load_state, load_image_async_simple, screenhack_event_helper | pixmaps | 392 |
| deco | 2d | L | CapButt, DisplayOfScreen, JoinBevel, LineSolid, XChangeGC, XDrawRectangle, XSetLineAttributes, XStoreColors, allocate_writable_colors, has_writable_cells, make_smooth_colormap | - | 345 |
| deluxe | 2d | L | CapProjecting, GCCapStyle, GCJoinStyle, GCPlaneMask, JoinMiter, Pixmap, XCopyArea, XCreatePixmap, XDrawArc, XFreePixmap, XGCValues.cap_style, XGCValues.join_style, XGCValues.plane_mask, allocate_alpha_colors, alpha.h, countof | pixmaps, float-heavy | 480 |
| demon | 2d | L | XChangeGC, XCreatePixmapFromBitmapData, XFillRectangles, XFreePixmap, automata.h, xlock.h | needs-xlockmore | 953 |
| distort | 2d | L | BlackPixelOfScreen, Pixmap, XCopyArea, XCreatePixmap, XDestroyImage, XFreePixmap, XGetImage, XGetPixel, XImage, XPutImage, XPutPixel, XShmSegmentInfo, ZPixmap, async_load_state, create_xshm_image, destroy_xshm_image, load_image_async_simple, put_xshm_image, screenhack_event_helper, xshm.h | pixmaps, readback | 894 |
| droste | 2d | L | BlackPixelOfScreen, GET_PARENT_OBJ, KeyPress, KeySym, Pixmap, THREAD_DEFAULTS, THREAD_OPTIONS, XCreatePixmap, XDestroyImage, XEvent.xkey, XFreePixmap, XGetImage, XGetPixel, XImage, XK_Down, XK_Left, XK_Right, XK_Up, XLookupString, XPutPixel, XShmSegmentInfo, ZPixmap, async_load_state, countof, create_xshm_image, destroy_xshm_image, double_time, doubletime.h, hardware_concurrency, i_log2_fast, keysym, load_image_async_simple, pow2.h, put_xshm_image, screenhack_event_helper, thread_util.h, threadpool, threadpool_create, threadpool_destroy, threadpool_run, threadpool_wait, xshm.h | pixmaps, readback | 686 |
| epicycle | 2d | L | CapRound, GCCapStyle, GCJoinStyle, JoinRound, XGCValues.cap_style, XGCValues.join_style, free_colors, make_smooth_colormap, screenhack_event_helper | - | 803 |
| eruption | 2d | L | XEvent.x, XEvent.y, XImage, XPutPixel, XSetWindowBackground, XShmSegmentInfo, ZPixmap, create_xshm_image, destroy_xshm_image, img, put_xshm_image, screenhack_event_helper, xshm.h | - | 608 |
| ffmpeg-out | 2d | L | AVCodec, AVCodecContext, AVDictionary, AVERROR, AVERROR_EOF, AVFMT_GLOBALHEADER, AVFormatContext, AVFrame, AVIO_FLAG_WRITE, AVMEDIA_TYPE_AUDIO, AVPacket, AVStream, AV_CH_LAYOUT_MONO, AV_CH_LAYOUT_STEREO, AV_CODEC_CAP_VARIABLE_FRAME_SIZE, AV_CODEC_FLAG_GLOBAL_HEADER, AV_CODEC_ID_AAC, AV_CODEC_ID_H264, AV_LOG_ERROR, AV_PIX_FMT_BGR24, AV_PIX_FMT_BGR32, AV_PIX_FMT_YUV420P, AV_SAMPLE_FMT_FLTP, AV_SAMPLE_FMT_NONE, Bool, EAGAIN, FF_PROFILE_H264_HIGH, SWS_BICUBIC, X11/Xlib.h, X11/Xos.h, X11/Xutil.h, XImage, av_compare_ts, av_dict_free, av_dict_set, av_err2str, av_find_best_stream, av_frame_alloc, av_frame_free, av_frame_get_buffer, av_frame_make_writable, av_frame_unref, av_get_channel_layout_nb_channels, av_interleaved_write_frame, av_log_set_level, av_packet_alloc, av_packet_free, av_packet_rescale_ts, av_packet_unref, av_popcount64, av_read_frame, av_register_all, av_rescale, av_samples_set_silence, av_write_trailer, avcodec_alloc_context3, avcodec_find_decoder, avcodec_find_encoder, avcodec_free_context, avcodec_get_name, avcodec_open2, avcodec_parameters_from_context, avcodec_parameters_to_context, avcodec_receive_frame, avcodec_receive_packet, avcodec_send_frame, avcodec_send_packet, avformat_alloc_output_context2, avformat_close_input, avformat_find_stream_info, avformat_free_context, avformat_new_stream, avformat_open_input, avformat_write_header, avio_closep, avio_open, ffmpeg-out.h, ffmpeg_out_state, libavcodec/avcodec.h, libavformat/avformat.h, libavutil/avutil.h, libswresample/swresample.h, libswscale/swscale.h, no_opt, opt, pkt, progname, screenhackI.h, swr_alloc, swr_config_frame, swr_convert_frame, swr_free, swr_get_delay, swr_init, swr_is_initialized, sws_getContext, sws_scale | - | 732 |
| fiberlamp | 2d | L | XAllocNamedColor, XCopyArea, XCreatePixmap, XFreePixmap, XSetGraphicsExposures, XSetLineAttributes, XTranslateCoordinates, xlock.h | pixmaps, needs-xlockmore | 480 |
| filmleader | 2d | L | ANALOGTV_DEFAULTS, ANALOGTV_OPTIONS, ANALOGTV_SIGNAL_LEN, ButtonRelease, CapRound, FcChar8, JoinRound, KeyPress, KeySym, LineSolid, Pixmap, XCreateImage, XCreatePixmap, XDestroyImage, XDrawArc, XEvent.xany, XEvent.xkey, XFreePixmap, XGetImage, XGetPixel, XGlyphInfo, XImage, XLookupString, XPutImage, XPutPixel, XSetLineAttributes, XftColor, XftColorAllocName, XftColorFree, XftDraw, XftDrawCreate, XftDrawDestroy, XftDrawStringUtf8, XftFont, XftTextExtentsUtf8, ZPixmap, analogtv, analogtv.h, analogtv_allocate, analogtv_draw, analogtv_input, analogtv_input_allocate, analogtv_load_ximage, analogtv_reception, analogtv_reception_update, analogtv_reconfigure, analogtv_release, analogtv_set_defaults, analogtv_setup_sync, countof, double_time, doubletime.h, extents, img, img1, img2, keysym, load_xft_font_retry, screen_number, screenhack_event_helper, xftfont | pixmaps, readback | 548 |
| fireworkx | 2d | L | ButtonRelease, ImageByteOrder, MSBFirst, XCreateImage, XDestroyImage, XEvent.x, XEvent.y, XImage, XPutImage, XSync, ZPixmap, make_smooth_colormap | - | 882 |
| flag | 2d | L | Bool, Display, ENTRYPOINT, False, LRAND, MAXRAND, MI_CYCLES, MI_DISPLAY, MI_GC, MI_INIT, MI_NPIXELS, MI_PIXEL, MI_SCREEN, MI_SIZE, MI_VISUAL, MI_WINDOW, MI_WIN_BLACK_PIXEL, MI_WIN_DEPTH, MI_WIN_HEIGHT, MI_WIN_WHITE_PIXEL, MI_WIN_WIDTH, M_PI, ModeInfo, NRAND, NULL, Pixmap, SINF, True, Window, XClearWindow, XCopyArea, XCreateImage, XCreatePixmap, XDestroyImage, XDrawPoint, XDrawString, XFillArc, XFillRectangle, XFreeFont, XFreePixmap, XGetImage, XGetPixel, XImage, XLoadQueryFont, XPutPixel, XSetForeground, XSetGraphicsExposures, XTextExtents, XYBitmap, display, error, flag.h, flag_bits, flag_height, flag_width, window, xlock.h | pixmaps, readback, text, needs-xlockmore | 570 |
| flame | 2d | L | GCBackground, XFillRectangles, XGCValues.background, make_smooth_colormap, screenhack_event_helper | - | 457 |
| flow | 2d | L | XCopyArea, XCreatePixmap, XDrawSegments, XFreePixmap, XSetGraphicsExposures, XSetLineAttributes, xlock.h | pixmaps, needs-xlockmore | 1216 |
| fluidballs | 2d | L | ButtonRelease, FcChar8, GCBackground, Pixmap, RootWindow, XCopyArea, XCreatePixmap, XEvent.x, XEvent.xany, XEvent.y, XFreePixmap, XGCValues.background, XQueryPointer, XSelectInput, XTranslateCoordinates, XftColor, XftColorAllocName, XftDraw, XftDrawCreate, XftDrawDestroy, XftDrawStringUtf8, XftFont, XftFontClose, gettimeofday, load_xft_font_retry, screen_number | pixmaps | 881 |
| fontglide | 2d | L | BlackPixelOfScreen, DisplayOfScreen, FcChar8, Pixmap, X11/Intrinsic.h, XCopyArea, XCreateImage, XCreatePixmap, XDestroyImage, XDrawRectangle, XDrawString, XDrawString16, XFreeFont, XFreePixmap, XGetAtomName, XGetGeometry, XGetImage, XGetPixel, XGlyphInfo, XImage, XLoadQueryFont, XLookupString, XPutImage, XPutPixel, XRenderColor, XSetClipMask, XSetClipOrigin, XSetFont, XTextExtents, XTextExtents16, XYPixmap, XftColor, XftColorAllocValue, XftColorFree, XftDraw, XftDrawCreate, XftDrawDestroy, XftDrawStringUtf8, XftFont, XftFontClose, XftTextExtentsUtf8, ZPixmap, bg, extents, fg, in, load_xft_font_retry, mask, out, screen_number, swap, text_data, textclient.h, textclient_close, textclient_getc, textclient_open, utf8_decode_combining, utf8wc.h, xftdraw | pixmaps, readback, text, clipmask | 2474 |
| forest | 2d | L | CapButt, Display, DoBlue, DoGreen, DoRed, ENTRYPOINT, GC, JoinMiter, LineSolid, MI_DISPLAY, MI_INIT, MI_SCREEN, MI_WINDOW, MI_WIN_HEIGHT, MI_WIN_WIDTH, M_PI_2, ModeInfo, NRAND, NULL, XAllocColor, XArc, XClearWindow, XColor, XDrawLine, XFillArcs, XSetForeground, XSetLineAttributes, display, gc, leaf, xlock.h | needs-xlockmore | 241 |
| fps | 2d | L | Bool, Display, FcChar8, GCForeground, Window, XCreateGC, XFillRectangle, XFreeGC, XGCValues, XGetWindowAttributes, XGlyphInfo, XWindowAttributes, XftColorAllocName, XftDrawCreate, XftDrawDestroy, XftDrawStringUtf8, XftFont, XftFontClose, XftTextExtentsUtf8, f, fpsI.h, fps_state, gcv, get_boolean_resource, get_pixel_resource, get_string_resource, gettimeofday, load_xft_font_retry, overall, progname, screen_number, screenhackI.h, st, xft.h, xgwa | - | 306 |
| fuzzyflakes | 2d | L | CapProjecting, GCCapStyle, GCJoinStyle, JoinMiter, Pixmap, XCopyArea, XCreatePixmap, XFreePixmap, XGCValues.background, XGCValues.cap_style, XGCValues.join_style, XParseColor | pixmaps | 655 |
| galaxy | 2d | L | Bool, COSF, Display, ENTRYPOINT, GC, LRAND, MAXRAND, MI_BATCHCOUNT, MI_CYCLES, MI_DISPLAY, MI_HEIGHT, MI_INIT, MI_NCOLORS, MI_PIXEL, MI_SCREEN, MI_WIDTH, MI_WINDOW, MI_WIN_BLACK_PIXEL, MI_WIN_HEIGHT, MI_WIN_WIDTH, M_PI, ModeInfo, NRAND, NULL, OptionStruct, SINF, Window, XClearWindow, XFillRectangles, XRectangle, XSetForeground, XrmOptionDescRec, XrmoptionNoArg, argtype, display, dummy, gc, newp, oldp, t_Bool, window, xlock.h | needs-xlockmore | 462 |
| glitchpeg | 2d | L | BitmapBitOrder, ButtonRelease, ImageByteOrder, X11/Intrinsic.h, XCreateImage, XDestroyImage, XEvent.xany, XGetPixel, XImage, XPutImage, XPutPixel, XtAppAddInput, XtDisplayToApplicationContext, XtInputExceptMask, XtInputId, XtInputReadMask, XtPointer, XtRemoveInput, ZPixmap, image, image_data_to_ximage, out, screenhack_event_helper, time, ximage-loader.h | readback | 466 |
| goop | 2d | L | AllPlanes, DefaultScreenOfDisplay, DisplayOfScreen, GCBackground, GXclear, GXxor, Nonconvex, Pixmap, WhitePixelOfScreen, XCopyArea, XCopyPlane, XCreatePixmap, XGCValues.background, XSetFunction, XSetPlaneMask, allocate_alpha_colors, alpha.h, compute_closed_spline, free_spline, has_writable_cells, make_spline, spline | pixmaps, xor | 651 |
| greynetic | 2d | L | FillOpaqueStippled, GCBackground, GCFillStyle, GCStipple, Pixmap, XChangeGC, XCreatePixmapFromBitmapData, XGCValues.background, XGCValues.fill_style, XGCValues.stipple | - | 297 |
| halftone | 2d | L | Pixmap, XCopyArea, XCreatePixmap, XFreePixmap, make_smooth_colormap | pixmaps | 413 |
| halo | 2d | L | GCBackground, GXxor, Pixmap, XCopyPlane, XCreatePixmap, XFreePixmap, XGCValues.background, XSetBackground, make_smooth_colormap, make_uniform_colormap | pixmaps | 459 |
| hexadrop | 2d | L | Convex, XSetWindowBackground, countof, free_colors, make_smooth_colormap, screenhack_event_helper | - | 446 |
| ifs | 2d | L | None, XCopyArea, XCreatePixmap, XFillRectangles, XFreePixmap, countof, make_smooth_colormap, screenhack_event_helper | pixmaps | 560 |
| imsmap | 2d | L | XCreateImage, XDestroyImage, XImage, XPutImage, XPutPixel, XSetBackground, XYBitmap, free_colors, image, make_smooth_colormap, screenhack_event_helper | - | 426 |
| interference | 2d | L | GET_PARENT_OBJ, None, Pixmap, THREAD_DEFAULTS, THREAD_OPTIONS, XCopyArea, XCreatePixmap, XFreePixmap, XImage, XPutPixel, XShmGetEventBase, XShmSegmentInfo, ZPixmap, create_xshm_image, destroy_xshm_image, free_colors, gettimeofday, hardware_concurrency, make_color_loop, put_xshm_image, thread_memory_alignment, thread_util.h, threadpool, threadpool_create, threadpool_destroy, threadpool_run, threadpool_wait, visual_pixmap_depth, xshm.h | pixmaps | 1002 |
| intermomentary | 2d | L | Pixmap, XCopyArea, XCreatePixmap, XFreePixmap, XGCValues.background, XQueryColor, XSetFillStyle, XSetTile, make_color_ramp, rgb_to_hsv | pixmaps | 605 |
| juggle | 2d | L | XDrawArc, XDrawImageString, XDrawString, XFreeFontInfo, XLoadQueryFont, XSetLineAttributes, XTextWidth, xlock.h | text, float-heavy, needs-xlockmore | 2798 |
| julia | 2d | L | Bool, Cursor, Display, DoBlue, DoGreen, DoRed, ENTRYPOINT, FillOpaqueStippled, GC, GCBackground, GCForeground, LRAND, MAX, MIN, MI_BATCHCOUNT, MI_CYCLES, MI_DISPLAY, MI_HEIGHT, MI_INIT, MI_NPIXELS, MI_PIXEL, MI_SCREEN, MI_WIDTH, MI_WIN_BLACK_PIXEL, MI_WIN_HEIGHT, MI_WIN_IS_INROOT, MI_WIN_WHITE_PIXEL, MI_WIN_WIDTH, M_PI, ModeInfo, NRAND, NULL, None, Pixmap, Window, XClearWindow, XColor, XCreateGC, XCreatePixmap, XCreatePixmapCursor, XCreatePixmapFromBitmapData, XDefineCursor, XDrawArc, XFillArc, XFillRectangle, XFillRectangles, XFreeCursor, XFreeGC, XFreePixmap, XGCValues, XRectangle, XSetFillStyle, XSetForeground, XSetStipple, XSetTSOrigin, XUndefineCursor, bg_gc, bit, black, display, fg_gc, gc, gcv, new_circle, old_circle, window, xlock.h, xp | pixmaps, float-heavy, needs-xlockmore | 451 |
| kaleidescope | 2d | L | CapRound, GCCapStyle, JoinRound, LineSolid, XDrawSegments, XGCValues.cap_style, XSegment, XSetLineAttributes | - | 514 |
| kumppa | 2d | L | XCopyArea, XSetGraphicsExposures, countof | pixmaps | 545 |
| lcdscrub | 2d | L | Pixmap, XCreateImage, XCreatePixmap, XDestroyImage, XFreePixmap, XGCValues.background, XGetPixel, XImage, XPutImage, XPutPixel, XSetBackground, XSetClipMask, XYPixmap, countof, gettimeofday, p | pixmaps, readback, clipmask | 399 |
| lmorph | 2d | L | CapButt, CapRound, JoinBevel, JoinRound, LineSolid, XSetLineAttributes | float-heavy | 580 |
| loop | 2d | L | XChangeGC, XCreatePixmapFromBitmapData, XFillRectangles, XFreePixmap, automata.h, xlock.h | needs-xlockmore | 1700 |
| m6502 | 2d | L | ANALOGTV_BLACK_LEVEL, ANALOGTV_BOT, ANALOGTV_DEFAULTS, ANALOGTV_OPTIONS, ANALOGTV_TOP, ANALOGTV_VISLINES, ANALOGTV_VIS_END, ANALOGTV_VIS_LEN, ANALOGTV_VIS_START, ANALOGTV_WHITE_LEVEL, Bit8, analogtv, analogtv.h, analogtv_allocate, analogtv_draw, analogtv_draw_solid, analogtv_input, analogtv_input_allocate, analogtv_lcp_to_ntsc, analogtv_reception, analogtv_reception_update, analogtv_reconfigure, analogtv_release, analogtv_set_defaults, analogtv_setup_sync, asm6502.h, countof, double_time, doubletime.h, m6502.h, m6502_build, m6502_destroy6502, m6502_next_eval, m6502_start_eval_file, m6502_start_eval_string, machine_6502, screenhack_event_helper | - | 288 |
| marbling | 2d | L | DefaultScreenOfDisplay, GET_PARENT_OBJ, KeyPress, KeySym, THREAD_DEFAULTS, THREAD_OPTIONS, XEvent.xany, XEvent.xkey, XImage, XK_Down, XK_Left, XK_Right, XK_Up, XLookupString, XPutPixel, XShmSegmentInfo, ZPixmap, create_xshm_image, destroy_xshm_image, free_colors, hardware_concurrency, keysym, make_smooth_colormap, put_xshm_image, screenhack_event_helper, thread_memory_alignment, thread_util.h, threadpool_create, threadpool_destroy, threadpool_run, threadpool_wait, visual_pixmap_depth, xshm.h | - | 635 |
| maze | 2d | L | Expose, Pixmap, XCopyArea, XCopyPlane, XFreePixmap, XGetGeometry, XSetBackground, XSetClipMask, XSetClipOrigin, XSetLineAttributes, XSync, image_data_to_pixmap, images/gen/logo-180_png.h, images/gen/logo-360_png.h, images/gen/logo-50_png.h, logo_180_png, logo_360_png, logo_50_png, logo_mask, screenhack_event_helper, ximage-loader.h | pixmaps, clipmask | 1681 |
| memscroller | 2d | L | FcChar8, GCBackground, XCopyArea, XDrawRectangle, XGCValues.background, XGlyphInfo, XImage, XShmSegmentInfo, XftColor, XftColorAllocName, XftDraw, XftDrawCreate, XftDrawDestroy, XftDrawStringUtf8, XftFont, XftFontClose, XftTextExtentsUtf8, ZPixmap, countof, create_xshm_image, destroy_xshm_image, load_xft_font_retry, overall, put_xshm_image, screen_number, xft.h, xshm.h | pixmaps | 626 |
| metaballs | 2d | L | BitmapPad, XCreateImage, XDestroyImage, XFree, XImage, XListPixmapFormats, XParseColor, XPutImage, XPutPixel, XSetWindowBackground, ZPixmap | - | 438 |
| moire | 2d | L | BlackPixelOfScreen, DefaultScreenOfDisplay, WhitePixelOfScreen, XImage, XPutPixel, XQueryColor, XShmSegmentInfo, ZPixmap, create_xshm_image, destroy_xshm_image, make_color_ramp, put_xshm_image, rgb_to_hsv, visual_depth, xshm.h | - | 253 |
| moire2 | 2d | L | GCBackground, GXor, GXxor, Pixmap, XCopyArea, XCopyPlane, XCreatePixmap, XDrawArc, XFreePixmap, XGCValues.background, XSetBackground, XSetFunction, make_smooth_colormap | pixmaps, xor | 363 |
| munch | 2d | L | GCBackground, GXxor, XGCValues.background, XSetFunction, i_log2, pow2.h, screenhack_event_helper | xor | 462 |
| nerverot | 2d | L | XCopyArea, XCreatePixmap, make_color_ramp, rgb_to_hsv | pixmaps, float-heavy | 1367 |
| noseguy | 2d | L | FcChar8, GCBackground, None, Pixmap, XCopyArea, XCreateImage, XCreatePixmap, XDestroyImage, XDrawRectangle, XFreePixmap, XGCValues.background, XGetImage, XGetPixel, XGlyphInfo, XImage, XPutImage, XPutPixel, XSetClipMask, XSetClipOrigin, XSetLineAttributes, XYPixmap, XftColor, XftColorAllocName, XftDraw, XftDrawCreate, XftDrawDestroy, XftDrawStringUtf8, XftFont, XftFontClose, XftTextExtentsUtf8, ZPixmap, extents, i1, i2, images/gen/nose-f1_png.h, images/gen/nose-f2_png.h, images/gen/nose-f3_png.h, images/gen/nose-f4_png.h, images/gen/nose-l1_png.h, images/gen/nose-l2_png.h, images/gen/nose-r1_png.h, images/gen/nose-r2_png.h, load_xft_font_retry, mask, nose_f1_png, nose_f2_png, nose_f3_png, nose_f4_png, nose_l1_png, nose_l2_png, nose_r1_png, nose_r2_png, p2, pixmap, screen_number, text_data, textclient.h, textclient_close, textclient_getc, textclient_open, textclient_reshape, ximage-loader.h | pixmaps, readback, clipmask | 720 |
| pacman | 2d | L | XCopyArea, XCreatePixmap, XDrawArc, XDrawString, XFreePixmap, XGetGeometry, XLoadQueryFont, XSetClipMask, XSetClipOrigin, XSetFillStyle, XSetLineAttributes, xlock.h | pixmaps, text, clipmask, needs-xlockmore | 1479 |
| pacman_ai | 2d | L | False, GETFACTOR, GHOST_TRACE, JAILHEIGHT, JAILWIDTH, LEVHEIGHT, LEVWIDTH, ModeInfo, NOWHERE, NRAND, NULL, TRACEVECS, True, Window, XQueryPointer, c, chasing, fprintf, ghoststruct, goingin, goingout, hiding, inbox, pacman.h, pacman_ai.h, pacman_check_dot, pacman_check_pos, pacman_get_jail_opening, pacman_level.h, pacman_trackmouse, pacmangamestruct, pacmanstruct, ps_chasing, ps_dieing, ps_eating, ps_hiding, ps_random, r, randdir, stderr, tmp_ghost, xlockmoreI.h | - | 879 |
| pacman_level | 2d | L | False, GETNB, JAILHEIGHT, JAILWIDTH, LEVHEIGHT, LEVWIDTH, MINDOTPERC, NRAND, NULL, NUM_BONUS_DOTS, TESTNB, TILEHEIGHT, TILEWIDTH, True, frmtlev, lev_t, level, pacman.h, pacman_level.h, pacmangamestruct, savedlev, screenhackI.h | - | 774 |
| penetrate | 2d | L | FcChar8, XDrawArc, XGlyphInfo, XSetLineAttributes, XSync, XftColor, XftColorAllocName, XftDraw, XftDrawCreate, XftDrawDestroy, XftDrawStringUtf8, XftFont, XftFontClose, XftTextExtentsUtf8, load_xft_font_retry, overall, screen_number, usleep | - | 1037 |
| phosphor | 2d | L | BlackPixelOfScreen, CapRound, DefaultScreenOfDisplay, Expose, FALSE, FcChar8, GCBackground, GCCapStyle, KeyPress, None, Pixmap, TTY_BOLD, TTY_INVERSE, TTY_ITALIC, TTY_SYMBOLS, Time, X11/Intrinsic.h, XCopyArea, XCopyPlane, XCreateImage, XCreatePixmap, XDestroyImage, XEvent.xany, XEvent.xkey, XFreePixmap, XGCValues.background, XGCValues.cap_style, XGetImage, XGetPixel, XGlyphInfo, XImage, XPutImage, XPutPixel, XQueryColor, XSetClipMask, XSetClipOrigin, XWriteBitmapFile, XYBitmap, XYPixmap, XftColor, XftDraw, XftDrawCreate, XftDrawStringUtf8, XftFont, XftTextExtentsUtf8, XtAppAddTimeOut, XtAppContext, XtIntervalId, XtPointer, XtRemoveTimeOut, ZPixmap, ansi-tty.h, ansi_graphics_unicode, ansi_tty, ansi_tty_free, ansi_tty_init, ansi_tty_print, ansi_tty_resize, app, countof, font, font_bits, im, im2, images/gen/6x10font_png.h, load_xft_font_retry, m, make_color_ramp, mm, overall, p, p2, pm_color, rgb_to_hsv, screen_number, tcell, text_data, textclient.h, textclient_close, textclient_getc, textclient_open, textclient_putc_event, textclient_puts, textclient_reshape, tty, tty_char, utf8_encode, utf8_to_latin1, utf8wc.h, xft_fg, xftdraw, xim_color, xim_mono, ximage-loader.h | pixmaps, readback, clipmask | 1260 |
| piecewise | 2d | L | Pixmap, XArc, XCopyArea, XCreatePixmap, XDrawArcs, make_color_loop | pixmaps | 1036 |
| polyominoes | 2d | L | XCreateImage, XDestroyImage, XDrawRectangle, XDrawSegments, XFillRectangles, XPutImage, XSetLineAttributes, xlock.h | needs-xlockmore | 2370 |
| pong | 2d | L | ANALOGTV_BLACK_LEVEL, ANALOGTV_BOT, ANALOGTV_DEFAULTS, ANALOGTV_OPTIONS, ANALOGTV_TOP, ANALOGTV_VISLINES, ANALOGTV_VIS_END, ANALOGTV_VIS_LEN, ANALOGTV_VIS_START, ButtonPressMask, ButtonRelease, ButtonReleaseMask, CurrentTime, Cursor, FocusChangeMask, FocusIn, FocusOut, GrabModeAsync, KeyPress, KeyPressMask, KeyRelease, KeyReleaseMask, KeySym, MotionNotify, None, Pixmap, PointerMotionMask, X11/keysym.h, XCreatePixmap, XCreatePixmapCursor, XDefineCursor, XDestroyImage, XEvent.x, XEvent.xkey, XEvent.xmotion, XGrabPointer, XHeightMMOfScreen, XHeightOfScreen, XK_Down, XK_Up, XLookupString, XQueryPointer, XSelectInput, XUngrabPointer, XWarpPointer, analogtv, analogtv.h, analogtv_allocate, analogtv_draw, analogtv_draw_solid, analogtv_draw_string, analogtv_font, analogtv_font_set_char, analogtv_input, analogtv_input_allocate, analogtv_lcp_to_ntsc, analogtv_make_font, analogtv_reception, analogtv_reception_update, analogtv_reconfigure, analogtv_release, analogtv_set_defaults, analogtv_setup_sync, cursor_pix, double_time, doubletime.h, key | pixmaps | 1109 |
| popsquares | 2d | L | GCBackground, Pixmap, XCopyArea, XCreatePixmap, XFreePixmap, XGCValues.background, XQueryColor, make_color_ramp, rgb_to_hsv | pixmaps | 310 |
| qix | 2d | L | CellsOfScreen, DefaultScreenOfDisplay, GCPlaneMask, GXxor, XGCValues.plane_mask, XQueryColor, XSetWindowBackground, allocate_alpha_colors, alpha.h, has_writable_cells, rgb_to_hsv | - | 642 |
| rdbomb | 2d | L | DefaultScreenOfDisplay, XImage, XListPixmapFormats, XPixmapFormatValues, XSetWindowBackground, XShmSegmentInfo, ZPixmap, create_xshm_image, destroy_xshm_image, has_writable_cells, make_smooth_colormap, pfv, put_xshm_image, visual_depth, xshm.h | - | 571 |
| recanim | 2d | L | Display, DisplayOfScreen, False, GC, Pixmap, Screen, Window, XCopyArea, XCreateGC, XCreateImage, XCreatePixmap, XDestroyImage, XFetchName, XFreeGC, XFreePixmap, XGCValues, XGetSubImage, XGetWindowAttributes, XImage, XStoreName, XSync, XWindowAttributes, ZPixmap, doubletime.h, dpy, ffmpeg-out.h, ffmpeg_out_add_frame, ffmpeg_out_close, ffmpeg_out_init, ffmpeg_out_state, fprintf, gcv, progname, recanim.h, record_anim_state, screenhackI.h, screenhack_record_anim_free, stderr, unlink | pixmaps | 449 |
| ripples | 2d | L | XDestroyImage, XGetImage, XGetPixel, XImage, XPutPixel, XQueryColor, XShmSegmentInfo, ZPixmap, async_load_state, create_xshm_image, destroy_xshm_image, load_image_async_simple, make_smooth_colormap, put_xshm_image, screenhack_event_helper, time, time_t, visual_rgb_masks, xshm.h | readback | 1127 |
| rocks | 2d | L | GCBackground, Nonconvex, Pixmap, XCopyPlane, XCreatePixmap, XFreePixmap, XGCValues.background, XQueryColor, XSetGraphicsExposures, p | pixmaps | 562 |
| rotzoomer | 2d | L | Pixmap, XCreatePixmap, XDestroyImage, XFreePixmap, XGetImage, XGetPixel, XImage, XPutPixel, XShmSegmentInfo, ZPixmap, async_load_state, create_xshm_image, destroy_xshm_image, load_image_async_simple, put_xshm_image, screenhack_event_helper, time, time_t, xshm.h | pixmaps, readback | 601 |
| screenhack | 2d | L | Atom, Bool, Boolean, ButtonPress, ButtonPressMask, ButtonReleaseMask, CellsOfScreen, ClientMessage, Colormap, ConfigureNotify, DefaultRootWindow, DefaultScreenOfDisplay, DefaultVisualOfScreen, Display, DisplayOfScreen, False, KeyPress, KeyPressMask, KeyReleaseMask, KeySym, MapNotify, O_RDWR, Pixel, PropModeReplace, PropertyChangeMask, RootWindowOfScreen, Screen, StructureNotifyMask, True, VirtualRootWindowOfScreen, Visual, Widget, Window, X11/CoreP.h, X11/Intrinsic.h, X11/IntrinsicP.h, X11/Shell.h, X11/StringDefs.h, X11/keysym.h, XA_ATOM, XA_CARDINAL, XA_NET_WM_PID, XA_NET_WM_PING, XA_WM_DELETE_WINDOW, XBell, XChangeProperty, XClearWindow, XCreateColormap, XErrorEvent, XEvent, XGetAtomName, XGetWindowAttributes, XIfEvent, XInternAtom, XK_Hyper_R, XK_Shift_L, XLookupString, XNextEvent, XPending, XPointer, XSelectInput, XSendEvent, XSetErrorHandler, XSetWindowBackground, XSetWindowColormap, XSync, XVisualIDFromVisual, XWindowAttributes, XmuPrintDefaultErrorMessage, XrmOptionDescRec, XrmoptionNoArg, XrmoptionSepArg, XtAppContext, XtAppInitialize, XtAppPending, XtAppProcessEvent, XtDestroyApplicationContext, XtDestroyWidget, XtDisplay, XtGetApplicationNameAndClass, XtGrabNone, XtIMXEvent, XtInputMask, XtNbackground, XtNborderColor, XtNcolormap, XtNdepth, XtNheight, XtNinput, XtNmappedWhenManaged, XtNtitle, XtNvisual, XtNwidth, XtNx, XtNy, XtPopup, XtRealizeWidget, XtScreen, XtVaAppCreateShell, XtVaSetValues, XtWindow, app, argp, close, cmap, def_visual_p, desired_visual, dont_clear, dpy, event, fps.h, fps_compute, fps_draw, fps_init, fps_slept, fps_state, fpst, fpst2, get_boolean_resource, get_integer_resource, get_pixel_resource, get_string_resource, get_visual_resource, getpid, has_writable_cells, help_p, keysym, m, new, on_window, open, progname, root_p, screenhackI.h, screensaver_id, topLevelShellWidgetClass, toplevel, toplevel2, usleep, v, version.h, visual, visual_depth, vroot.h, window, window2, xgwa, xmu.h, ya_rand_init | - | 1099 |
| shadebobs | 2d | L | BlackPixelOfScreen, XCreateImage, XDestroyImage, XFree, XGetPixel, XImage, XListPixmapFormats, XParseColor, XPutImage, XPutPixel, XSetWindowBackground, ZPixmap | readback | 474 |
| slidescreen | 2d | L | Convex, XCopyArea, XDrawRectangle, XFree, XParseColor, XQueryColors, async_load_state, load_image_async_simple, screenhack_event_helper, time, time_t, visual_cells | pixmaps | 507 |
| slip | 2d | L | XCopyArea, XCreatePixmap, XFreePixmap, XSetGraphicsExposures, xlock.h | pixmaps, needs-xlockmore | 376 |
| speedmine | 2d | L | Nonconvex, None, Pixmap, XCopyArea, XCreatePixmap, XFreePixmap, XQueryColor, XSetClipMask, free_colors, gettimeofday, make_color_ramp, rgb_to_hsv | pixmaps, clipmask | 1659 |
| spotlight | 2d | L | Pixmap, XCopyArea, XCreatePixmap, XDrawRectangle, XFreePixmap, XSetClipMask, XSetClipOrigin, async_load_state, clip_pm, gettimeofday, load_image_async_simple, screenhack_event_helper, time, time_t | pixmaps, clipmask | 355 |
| starfish | 2d | L | EvenOddRule, GCFillRule, XGCValues.fill_rule, XSetWindowBackground, compute_closed_spline, free_colors, make_smooth_colormap, make_spline, make_uniform_colormap, spline, time, time_t | - | 564 |
| strange | 2d | L | XCopyArea, XCopyPlane, XCreatePixmap, XDrawPoints, XFillRectangles, XFreePixmap, XPutPixel, XQueryColor, XQueryColors, XSetBackground, XSetFunction, XSetGraphicsExposures, pow2.h, thread_util.h, xlock.h, xshm.h | pixmaps, xor, needs-xlockmore | 1353 |
| swirl | 2d | L | AllocAll, Bool, Colormap, Display, DoBlue, DoGreen, DoRed, ENTRYPOINT, False, LRAND, MAXRAND, MI_BATCHCOUNT, MI_BG_COLOR, MI_COLORMAP, MI_DISPLAY, MI_FG_COLOR, MI_GC, MI_INIT, MI_NPIXELS, MI_SATURATION, MI_SCREEN, MI_VISUAL, MI_WINDOW, MI_WIN_BLACK_PIXEL, MI_WIN_DEPTH, MI_WIN_HEIGHT, MI_WIN_IS_INWINDOW, MI_WIN_WHITE_PIXEL, MI_WIN_WIDTH, M_PI, ModeInfo, True, Visual, Window, XAllocColor, XClearWindow, XColor, XCreateColormap, XFree, XFreeColormap, XImage, XInstallColormap, XPutPixel, XQueryColor, XSetWMColormapWindows, XSetWindowColormap, XShmSegmentInfo, XStoreColors, ZPixmap, create_xshm_image, dest, destroy_xshm_image, display, done, dpy, hook, orbit, picasso, preserveColors, put_xshm_image, ray, same, setColormap, setupColormap, src, truecolor, value, wheel, window, xlock.h | needs-xlockmore | 1447 |
| t3d | 2d | L | BlackPixelOfScreen, Button1Mask, Button2Mask, Button3Mask, GXandInverted, GXor, KeyPress, KeySym, Pixmap, XAllocColorCells, XCopyArea, XCreatePixmap, XDrawSegments, XEvent.xkey, XFreePixmap, XGetImage, XLookupString, XPutImage, XQueryPointer, XStoreColors, gettimeofday, keysym | pixmaps, readback, float-heavy | 991 |
| tessellimage | 2d | L | ButtonRelease, Convex, ITRIANGLE, Pixmap, X11/keysymdef.h, XCopyArea, XCreateImage, XCreatePixmap, XDestroyImage, XEvent.xany, XFreePixmap, XGetGeometry, XGetImage, XGetPixel, XImage, XPutImage, XPutPixel, XQueryColor, XYZ, ZPixmap, async_load_state, countof, delaunay, delaunay.h, delaunay_xyzcompare, dimg, double_time, doubletime.h, img2, load_image_async_simple, p, screenhack_event_helper, tt, v, visual_rgb_masks | pixmaps, readback | 996 |
| testx11 | 2d | L | BlackPixelOfScreen, CapProjecting, CapRound, Convex, GCBackground, GCCapStyle, GCFont, GXxor, KeyPress, KeySym, NRAND, Pixmap, XClearArea, XCopyArea, XCopyPlane, XCreatePixmap, XCreatePixmapFromBitmapData, XDestroyImage, XDrawArc, XDrawPoints, XDrawRectangle, XDrawSegments, XDrawString, XEvent.x, XEvent.xany, XEvent.xkey, XEvent.y, XFillRectangles, XFreePixmap, XGCValues.background, XGCValues.cap_style, XGCValues.font, XGetImage, XImage, XLoadFont, XLookupString, XPutImage, XPutPixel, XSegment, XSetBackground, XSetClipMask, XSetWindowBackground, XSync, ZPixmap, colorbars.h, countof, draw_colorbars, get_position, get_rotation, glx/rotator.h, image, images/gen/logo-180_png.h, keysym, lines, logo, logo_mask, make_color_loop, make_rotator, pixmap, rotator, seg, ximage-loader.h | pixmaps, readback, text, clipmask | 968 |
| triangle | 2d | L | Convex, CoordModeOrigin, Display, ENTRYPOINT, GC, LRAND, MAX, MAXRAND, MIN, MI_DISPLAY, MI_GC, MI_INIT, MI_NCOLORS, MI_PAUSE, MI_SCREEN, MI_WINDOW, MI_WIN_BLACK_PIXEL, MI_WIN_HEIGHT, MI_WIN_WHITE_PIXEL, MI_WIN_WIDTH, M_PI_2, ModeInfo, NULL, Window, XClearWindow, XDrawLine, XFillPolygon, XFillRectangle, XPoint, XSetForeground, display, gc, p, window, xlock.h | needs-xlockmore | 355 |
| truchet | 2d | L | CapRound, JoinRound, LineSolid, Pixmap, XCopyArea, XCreatePixmap, XDrawArc, XGCValues.background, XSetLineAttributes | pixmaps | 541 |
| twang | 2d | L | Pixmap, XCreatePixmap, XDestroyImage, XFreePixmap, XGetImage, XGetPixel, XImage, XPutPixel, XShmSegmentInfo, ZPixmap, async_load_state, create_xshm_image, destroy_xshm_image, load_image_async_simple, put_xshm_image, screenhack_event_helper, time, time_t, xshm.h | pixmaps, readback | 791 |
| vfeedback | 2d | L | ANALOGTV_DEFAULTS, ANALOGTV_OPTIONS, ANALOGTV_SIGNAL_LEN, BlackPixelOfScreen, Button1, Button2, Button3, Button4, Button5, Button6, Button7, ButtonRelease, EASE_IN_OUT_SINE, KeyPress, KeySym, MotionNotify, Pixmap, PointerMotionMask, RANDSIGN, XCopyArea, XCreateImage, XCreatePixmap, XDestroyImage, XEvent.state, XEvent.x, XEvent.xany, XEvent.xkey, XEvent.xmotion, XEvent.y, XFreePixmap, XGetImage, XGetPixel, XImage, XK_Down, XK_Left, XK_Right, XK_Up, XLookupString, XPutPixel, XSelectInput, XWindowAttributes.your_event_mask, ZPixmap, analogtv, analogtv.h, analogtv_allocate, analogtv_draw, analogtv_input_allocate, analogtv_load_ximage, analogtv_reception, analogtv_reception_update, analogtv_reconfigure, analogtv_release, analogtv_set_defaults, analogtv_setup_sync, double_time, doubletime.h, ease, easing.h, img, in, keysym, out, screenhack_event_helper | pixmaps, readback | 592 |
| wander | 2d | L | NRAND, Pixmap, XCopyArea, XCreatePixmap, free_colors, make_color_loop, screenhack_event_helper | pixmaps | 284 |
| whirlygig | 2d | L | Pixmap, XCopyArea, XCreatePixmap, XDrawString, make_uniform_colormap | pixmaps, text, float-heavy | 741 |
| worm | 2d | L | COSF, Display, ENTRYPOINT, False, GC, GXcopy, GXor, LRAND, MI_BATCHCOUNT, MI_CYCLES, MI_DELTA3D, MI_DISPLAY, MI_GC, MI_INIT, MI_LEFT_COLOR, MI_NONE_COLOR, MI_NPIXELS, MI_PIXEL, MI_RIGHT_COLOR, MI_SCREEN, MI_SIZE, MI_WINDOW, MI_WIN_HEIGHT, MI_WIN_IS_INSTALL, MI_WIN_IS_USE3D, MI_WIN_WHITE_PIXEL, MI_WIN_WIDTH, M_PI, ModeInfo, NRAND, NULL, NUMCOLORS, SINF, Window, XClearArea, XClearWindow, XFillRectangle, XFillRectangles, XPoint, XRectangle, XSetForeground, XSetFunction, display, gc, window, xlock.h | xor, needs-xlockmore | 434 |
| wormhole | 2d | L | CapRound, JoinRound, LineSolid, Pixmap, XCopyArea, XCreatePixmap, XFreePixmap, XSetLineAttributes | pixmaps | 734 |
| xanalogtv | 2d | L | ANALOGTV_DEFAULTS, ANALOGTV_OPTIONS, ANALOGTV_PIC_LEN, ANALOGTV_SCALE, ANALOGTV_SIGNAL_LEN, ANALOGTV_V, ANALOGTV_VIS_LEN, ANALOGTV_VIS_START, KeyPress, KeySym, Pixmap, X11/Intrinsic.h, XCreatePixmap, XDestroyImage, XEvent.xkey, XFreePixmap, XGetImage, XImage, XK_Down, XK_Left, XK_Next, XK_Prior, XK_Right, XK_Up, XLookupString, XrmDatabase, XrmPutResource, XrmValue, ZPixmap, analogtv, analogtv.h, analogtv_allocate, analogtv_draw, analogtv_draw_solid_rel_lcp, analogtv_draw_string_centered, analogtv_font, analogtv_input, analogtv_input_allocate, analogtv_lcp_to_ntsc, analogtv_load_ximage, analogtv_make_font, analogtv_reception, analogtv_reception_update, analogtv_reconfigure, analogtv_release, analogtv_set_defaults, analogtv_setup_sync, analogtv_setup_teletext, db, gethostname, gettimeofday, image, image_data_to_pixmap, images/gen/logo-180_png.h, images/gen/testcard_bbcf_png.h, images/gen/testcard_pm5544_png.h, images/gen/testcard_rca_png.h, inp, input, keysym, load_image_async, localtime, mask, p, rec, screenhack_event_helper, strftime, testcard_bbcf_png, testcard_pm5544_png, testcard_rca_png, time, time_t, value, ximage, ximage-loader.h | pixmaps, readback | 689 |
| xflame | 2d | L | XCreateImage, XDestroyImage, XGetPixel, XImage, XPutPixel, XQueryColor, XShmSegmentInfo, ZPixmap, bob_png, create_xshm_image, destroy_xshm_image, file_to_ximage, image, image_data_to_ximage, images/gen/bob_png.h, out, put_xshm_image, ximage-loader.h, xshm.h | readback | 826 |
| ximage-loader | 2d | L | BitmapBitOrder, Bool, Display, GC, GCBackground, GCForeground, ImageByteOrder, Pixmap, Visual, Window, XCreateGC, XCreateImage, XCreatePixmap, XDestroyImage, XFreeGC, XFreePixmap, XGCValues, XGetImage, XGetPixel, XGetWindowAttributes, XImage, XPutImage, XPutPixel, XWindowAttributes, XYPixmap, ZPixmap, gc, gcv, in, mask, out, p2, pixmap, progname, screenhackI.h, xgwa, ximage, ximage-loader.h | pixmaps, readback | 679 |
| xjack | 2d | L | FcChar8, GCBackground, XClearArea, XCopyArea, XEvent.xany, XGCValues.background, XGlyphInfo, XftColor, XftColorAllocName, XftDraw, XftDrawCreate, XftDrawDestroy, XftDrawStringUtf8, XftFont, XftFontClose, XftTextExtentsUtf8, load_xft_font_retry, overall, screen_number | pixmaps | 508 |
| xlockmore | 2d | L | GCBackground, ModeInfo, ModeSpecOpt, PointerMotionMask, RootWindowOfScreen, X11/Intrinsic.h, XGCValues.background, XLOCKMORE_NUM_SCREENS, XSelectInput, color_scheme_bright, color_scheme_default, color_scheme_smooth, color_scheme_uniform, countof, erase_window, eraser_free, fps_compute, fps_draw, fps_state, free_colors, make_smooth_colormap, make_uniform_colormap, mi, screenhack_event_helper, t_Bool, t_Float, t_Int, t_String, xlockmoreI.h, xlockmore_opts, ya_rand_init | - | 795 |
| xlyap | 2d | L | BlackPixelOfScreen, Cursor, GCBackground, KeyPress, KeySym, WhitePixelOfScreen, X11/cursorfont.h, XComposeStatus, XCopyArea, XCreatePixmap, XDrawPoints, XEvent.xkey, XFreePixmap, XGCValues.background, XGetGeometry, XKeyEvent, XLookupString, XPending, XStoreColors, countof, free_colors, make_smooth_colormap, screenhack_event_helper, yarandom.h | pixmaps | 1939 |
| xmatrix | 2d | L | GCBackground, KeyPress, KeySym, Pixmap, X11/Intrinsic.h, XChangeGC, XCopyArea, XCreateImage, XCreatePixmap, XDestroyImage, XDrawRectangle, XEvent.xany, XEvent.xkey, XFreePixmap, XGCValues.background, XGetImage, XGetPixel, XImage, XLookupString, XPutImage, XPutPixel, XYPixmap, XtAppAddTimeOut, XtAppContext, XtIntervalId, XtPointer, XtRemoveTimeOut, ZPixmap, app, countof, i1, i2, im, image_data_to_pixmap, images/gen/matrix1_png.h, images/gen/matrix1b_png.h, images/gen/matrix2_png.h, images/gen/matrix2b_png.h, keysym, matrix1_png, matrix1b_png, matrix2_png, matrix2b_png, p2, screenhack_event_helper, text_data, textclient.h, textclient_close, textclient_getc, textclient_open, textclient_reshape, ximage-loader.h | pixmaps, readback | 1915 |
| xscreensaver-getimage | 2d | L | ../driver/prefs.h, AnyPropertyType, Atom, BadDrawable, BadWindow, ConnectionNumber, DefaultColormapOfScreen, DisplayOfScreen, IsViewable, MapNotify, None, PPosition, PSize, Pixmap, PropModeReplace, StructureNotifyMask, Success, TrueColor, USPosition, USSize, VirtualRootWindowOfScreen, Widget, X11/Intrinsic.h, X11/Xatom.h, X11/Xutil.h, XA_STRING, XA_XSCREENSAVER_IMAGE_GEOMETRY, XA_XSCREENSAVER_IMAGE_TITLE, XChangeProperty, XClassHint, XCopyArea, XCreateImage, XCreatePixmap, XDefaultColormapOfScreen, XDeleteProperty, XDestroyImage, XErrorEvent, XErrorHandler, XEvent.xany, XEvent.xvisibility, XFree, XFreePixmap, XGetClassHint, XGetGeometry, XGetImage, XGetPixel, XGetVisualInfo, XGetWMNormalHints, XGetWindowProperty, XIfEvent, XImage, XInstallColormap, XInternAtom, XMapRaised, XPointer, XPutImage, XPutPixel, XQueryColors, XQueryTree, XRootWindowOfScreen, XScreenNumberOfScreen, XSelectInput, XSetErrorHandler, XSetWMNormalHints, XSizeHints, XStoreColors, XSync, XUnmapWindow, XVisualIDFromVisual, XWindowAttributes.map_state, XWindowAttributes.your_event_mask, XmuPrintDefaultErrorMessage, XrmDatabase, XrmPutStringResource, XtAppContext, XtAppInitialize, XtDisplay, XtScreen, ZPixmap, a, blurb, blurb.h, close, colorbars.h, describe_visual, draw_colorbars, dup2, execvp, fork, grabclient.h, h, hints, logo, mask, old_handler, parse_init_file, pipe, resources.h, scaled, screensaver_id, screenshot, screenshot.h, screenshot_load, toplevel, type, unlink, usleep, version.h, visual.h, visual_depth, vroot.h, window_root_offset, ximage, ximage2, xmu.h, ya_rand_init, yarandom.h | pixmaps, readback | 2483 |
| xsublim | 2d | L | BadMatch, Bool, DefaultScreenOfDisplay, Display, GC, GCBackground, GCFont, GCForeground, GCSubwindowMode, IncludeInferiors, RootWindowOfScreen, Screen, Widget, Window, X11/CoreP.h, X11/Intrinsic.h, X11/IntrinsicP.h, X11/Shell.h, X11/StringDefs.h, X11/Xatom.h, X11/Xlib.h, X11/Xos.h, X11/Xproto.h, X11/Xutil.h, X11/keysym.h, XCreateGC, XDestroyImage, XDrawString, XErrorEvent, XErrorHandler, XFontStruct, XGCValues, XGetImage, XGetWindowAttributes, XGrabServer, XImage, XLoadQueryFont, XPutImage, XRootWindowOfScreen, XSetErrorHandler, XSync, XTextWidth, XUngrabServer, XWindowAttributes, XrmOptionDescRec, XrmoptionNoArg, XrmoptionSepArg, XtAppContext, XtAppInitialize, XtDestroyWidget, XtDisplay, XtGetApplicationNameAndClass, XtScreen, ZPixmap, app, app_App, argp, attr_Win, blurb.h, dpy, font-retry.h, font_Font, gc_GcBack, gc_GcFore, gc_ValBack, gc_ValFore, get_boolean_resource, get_integer_resource, get_pixel_resource, get_string_resource, getpid, image_Image, progname, resources.h, root, s, usleep, usleep.h, vroot, vroot.h, win_Root, xft.h, ya_rand_init, yarandom.h | readback, text | 808 |
| zoom | 2d | L | Pixmap, XCopyArea, XCreatePixmap, XDestroyImage, XFreePixmap, XGetImage, XGetPixel, XImage, XSetWindowBackground, ZPixmap, async_load_state, gettimeofday, load_image_async_simple, screenhack_event_helper, time, time_t | pixmaps, readback | 290 |
| apollonian | 2d | M | XDrawArc, XQueryColor, xlock.h | needs-xlockmore | 820 |
| blaster | 2d | M | XArc, XFillArcs | - | 1208 |
| braid | 2d | M | XSetLineAttributes, xlock.h | needs-xlockmore | 444 |
| cloudlife | 2d | M | XDrawPoints, make_smooth_colormap, screenhack_event_helper | - | 440 |
| coral | 2d | M | XFillRectangles, free_colors, make_uniform_colormap, screenhack_event_helper | - | 328 |
| critical | 2d | M | XChangeGC, free_colors, make_smooth_colormap, make_uniform_colormap | - | 462 |
| delaunay | 2d | M | ITRIANGLE, XYZ, delaunay.h, p2 | - | 304 |
| discrete | 2d | M | XDrawPoints, xlock.h | needs-xlockmore | 442 |
| drift | 2d | M | XDrawPoints, xlock.h | needs-xlockmore | 674 |
| euler2d | 2d | M | XDrawArc, XDrawSegments, XSetLineAttributes, xlock.h | float-heavy, needs-xlockmore | 893 |
| fadeplot | 2d | M | XFillRectangles, xlock.h | needs-xlockmore | 243 |
| grav | 2d | M | XDrawArc, xlock.h | needs-xlockmore | 360 |
| hopalong | 2d | M | XFillRectangles, xlock.h | needs-xlockmore | 563 |
| hyperball | 2d | M | Expose, UnmapNotify | - | 2464 |
| interaggregate | 2d | M | XGCValues.background, XParseColor, screenhack_event_helper | - | 989 |
| laser | 2d | M | XChangeGC, xlock.h | needs-xlockmore | 356 |
| lightning | 2d | M | xlock.h | needs-xlockmore | 602 |
| lisa | 2d | M | XDrawPoints, XMaxRequestSize, XSetLineAttributes, xlock.h | needs-xlockmore | 744 |
| lissie | 2d | M | XDrawArc, xlock.h | needs-xlockmore | 323 |
| mountain | 2d | M | xlock.h | needs-xlockmore | 283 |
| pedal | 2d | M | GCBackground, XGCValues.background, screenhack_event_helper | - | 339 |
| penrose | 2d | M | XSetLineAttributes, xlock.h | needs-xlockmore | 1342 |
| rorschach | 2d | M | XFillRectangles, screenhack_event_helper | - | 227 |
| rotor | 2d | M | XSetLineAttributes, xlock.h | needs-xlockmore | 394 |
| scooter | 2d | M | XSetLineAttributes, xlock.h | needs-xlockmore | 975 |
| sierpinski | 2d | M | XDrawPoints, xlock.h | needs-xlockmore | 215 |
| sphere | 2d | M | XDrawPoints, xlock.h | needs-xlockmore | 304 |
| spiral | 2d | M | xlock.h | needs-xlockmore | 331 |
| squiral | 2d | M | free_colors, make_uniform_colormap, screenhack_event_helper, yarandom.h | - | 335 |
| substrate | 2d | M | XGCValues.background, XParseColor, screenhack_event_helper | - | 780 |
| thornbird | 2d | M | XFillRectangles, xlock.h | needs-xlockmore | 270 |
| vermiculate | 2d | M | XSetWindowBackground, screenhack_event_helper, ya_random | - | 1229 |
| vines | 2d | M | xlock.h | needs-xlockmore | 190 |
| whirlwindwarp | 2d | M | XDrawRectangle, gettimeofday | - | 509 |
| xrayswarm | 2d | M | XSetGraphicsExposures, initTime | - | 1235 |
| helix | 2d | S | - | - | 358 |
| hypercube | 2d | S | - | - | 576 |
| petri | 2d | S | - | - | 780 |
| pyro | 2d | S | - | - | 373 |
| webcollage-helper | 2d | S | - | - | 597 |
| xspirograph | 2d | S | - | - | 338 |
| antinspect | gl | XL | - | needs-xlockmore | 696 |
| antmaze | gl | XL | - | needs-xlockmore | 1612 |
| antspotlight | gl | XL | - | needs-xlockmore | 797 |
| atlantis | gl | XL | XDestroyImage | needs-xlockmore | 607 |
| atunnel | gl | XL | XDestroyImage | needs-xlockmore | 315 |
| b_draw | gl | XL | - | - | 244 |
| b_lockglue | gl | XL | XParseColor | needs-xlockmore | 240 |
| b_sphere | gl | XL | - | - | 220 |
| beats | gl | XL | - | needs-xlockmore | 439 |
| blinkbox | gl | XL | - | needs-xlockmore | 615 |
| blocktube | gl | XL | XDestroyImage | needs-xlockmore | 454 |
| boing | gl | XL | XParseColor | float-heavy, needs-xlockmore | 666 |
| bouncingcow | gl | XL | - | needs-xlockmore | 649 |
| boxed | gl | XL | - | needs-xlockmore | 1361 |
| bubble3d | gl | XL | - | - | 282 |
| buildlwo | gl | XL | - | - | 99 |
| cage | gl | XL | XDestroyImage | needs-xlockmore | 498 |
| carousel | gl | XL | - | needs-xlockmore | 982 |
| chessmodels | gl | XL | - | - | 1721 |
| chompytower | gl | XL | XParseColor | needs-xlockmore | 1131 |
| circuit | gl | XL | - | needs-xlockmore | 2106 |
| cityflow | gl | XL | - | needs-xlockmore | 561 |
| companion | gl | XL | - | needs-xlockmore | 605 |
| companion_disc | gl | XL | - | - | 9594 |
| companion_heart | gl | XL | - | - | 654 |
| companion_quad | gl | XL | - | - | 390 |
| countries | gl | XL | - | - | 7863 |
| covid19 | gl | XL | XParseColor | needs-xlockmore | 657 |
| cow_face | gl | XL | - | - | 342 |
| cow_hide | gl | XL | - | - | 13056 |
| cow_hoofs | gl | XL | - | - | 1038 |
| cow_horns | gl | XL | - | - | 1026 |
| cow_tail | gl | XL | - | - | 465 |
| cow_udder | gl | XL | - | - | 1521 |
| crackberg | gl | XL | XLookupString, XNextEvent, XPeekEvent, XPending | float-heavy, needs-xlockmore | 1484 |
| crumbler | gl | XL | - | needs-xlockmore | 905 |
| cube21 | gl | XL | - | needs-xlockmore | 943 |
| cubenetic | gl | XL | - | needs-xlockmore | 616 |
| cubestack | gl | XL | XLookupString | needs-xlockmore | 453 |
| cubestorm | gl | XL | XLookupString | needs-xlockmore | 485 |
| cubetwist | gl | XL | XLookupString | needs-xlockmore | 579 |
| cubicgrid | gl | XL | - | needs-xlockmore | 322 |
| cubocteversion | gl | XL | XDestroyImage | needs-xlockmore | 5657 |
| dangerball | gl | XL | - | needs-xlockmore | 377 |
| deepstars | gl | XL | - | needs-xlockmore | 385 |
| discoball | gl | XL | - | needs-xlockmore | 709 |
| dnalogo | gl | XL | XLookupString, XParseColor | float-heavy, needs-xlockmore | 3657 |
| dolphin | gl | XL | - | - | 2063 |
| dropshadow | gl | XL | - | - | 179 |
| dumpster_model | gl | XL | - | - | 1668 |
| dumpsterfire | gl | XL | XParseColor | needs-xlockmore | 845 |
| dymaxionmap | gl | XL | XCreateImage, XDestroyImage, XGetPixel, XLookupString, XPutPixel | readback, needs-xlockmore | 1729 |
| dymaxionmap-coords | gl | XL | - | float-heavy | 686 |
| earth | gl | XL | - | - | 28 |
| endgame | gl | XL | - | needs-xlockmore | 1451 |
| energystream | gl | XL | - | needs-xlockmore | 536 |
| engine | gl | XL | - | needs-xlockmore | 1015 |
| erase-gl | gl | XL | - | - | 39 |
| esper | gl | XL | XLookupString, XParseColor | needs-xlockmore | 2463 |
| etruscanvenus | gl | XL | - | needs-xlockmore | 2761 |
| extrusion | gl | XL | - | needs-xlockmore | 552 |
| extrusion-helix2 | gl | XL | - | - | 48 |
| extrusion-helix3 | gl | XL | - | - | 47 |
| extrusion-helix4 | gl | XL | - | - | 64 |
| extrusion-joinoffset | gl | XL | - | - | 149 |
| extrusion-screw | gl | XL | - | - | 115 |
| extrusion-taper | gl | XL | - | - | 219 |
| extrusion-twistoid | gl | XL | - | - | 216 |
| flipflop | gl | XL | - | needs-xlockmore | 875 |
| flipscreen3d | gl | XL | - | needs-xlockmore | 524 |
| fliptext | gl | XL | XParseColor | needs-xlockmore | 1005 |
| floppy | gl | XL | XParseColor | needs-xlockmore | 584 |
| floppy_model | gl | XL | - | - | 26396 |
| flurry | gl | XL | - | needs-xlockmore | 550 |
| flurry-smoke | gl | XL | - | - | 1442 |
| flurry-spark | gl | XL | - | float-heavy | 288 |
| flurry-star | gl | XL | - | - | 107 |
| flurry-texture | gl | XL | - | - | 226 |
| flyingtoasters | gl | XL | XDestroyImage | needs-xlockmore | 924 |
| fps-gl | gl | XL | - | - | 99 |
| gears | gl | XL | - | needs-xlockmore | 953 |
| geodesic | gl | XL | - | float-heavy, needs-xlockmore | 825 |
| geodesicgears | gl | XL | XLookupString | float-heavy, needs-xlockmore | 1802 |
| gflux | gl | XL | - | needs-xlockmore | 799 |
| gibson | gl | XL | XParseColor | needs-xlockmore | 1317 |
| glblur | gl | XL | - | needs-xlockmore | 630 |
| glcells | gl | XL | - | needs-xlockmore | 1382 |
| gleidescope | gl | XL | XOFFSET | float-heavy, needs-xlockmore | 1620 |
| glforestfire | gl | XL | XDestroyImage | needs-xlockmore | 1089 |
| glhanoi | gl | XL | - | float-heavy, needs-xlockmore | 2080 |
| glknots | gl | XL | - | needs-xlockmore | 455 |
| gllist | gl | XL | - | - | 128 |
| glmatrix | gl | XL | XDestroyImage, XGetPixel, XPutPixel | readback, needs-xlockmore | 1077 |
| glplanet | gl | XL | XDestroyImage | float-heavy, needs-xlockmore | 1111 |
| glschool | gl | XL | - | needs-xlockmore | 229 |
| glschool_alg | gl | XL | - | - | 365 |
| glschool_gl | gl | XL | - | - | 275 |
| glsl-utils | gl | XL | - | - | 561 |
| glslideshow | gl | XL | - | needs-xlockmore | 1917 |
| glsnake | gl | XL | - | needs-xlockmore | 2699 |
| gltext | gl | XL | - | needs-xlockmore | 619 |
| gltrackball | gl | XL | - | - | 357 |
| glut_stroke | gl | XL | - | - | 50 |
| glut_swidth | gl | XL | - | - | 66 |
| grab-ximage | gl | XL | XCreateImage, XCreatePixmap, XDestroyImage, XFree, XFreePixmap, XGetGeometry, XGetPixel, XGetSubImage, XInitImage, XPutPixel, XQueryColors, XSubImage | pixmaps, readback | 1185 |
| graphstat | gl | XL | - | needs-xlockmore | 750 |
| gravitywell | gl | XL | XParseColor | needs-xlockmore | 770 |
| handsy | gl | XL | XLookupString, XParseColor | needs-xlockmore | 1158 |
| handsy_model | gl | XL | - | - | 6685 |
| headroom | gl | XL | XParseColor | needs-xlockmore | 628 |
| headroom_model | gl | XL | - | - | 14013 |
| hexstrut | gl | XL | XLookupString | needs-xlockmore | 511 |
| hextrail | gl | XL | XLookupString | needs-xlockmore | 782 |
| highvoltage | gl | XL | XLookupString, XParseColor | needs-xlockmore | 949 |
| highvoltage_model | gl | XL | - | - | 21652 |
| hilbert | gl | XL | XLookupString | needs-xlockmore | 1165 |
| hopfanimations | gl | XL | - | - | 10225 |
| hopffibration | gl | XL | - | needs-xlockmore | 3580 |
| hydrostat | gl | XL | - | needs-xlockmore | 796 |
| hypertorus | gl | XL | XLookupString | float-heavy, needs-xlockmore | 2150 |
| hypnowheel | gl | XL | - | needs-xlockmore | 335 |
| involute | gl | XL | - | float-heavy | 977 |
| jigglypuff | gl | XL | XDestroyImage | needs-xlockmore | 1125 |
| jigsaw | gl | XL | - | needs-xlockmore | 1512 |
| juggler3d | gl | XL | XSetLineAttributes | float-heavy, needs-xlockmore | 3023 |
| kaleidocycle | gl | XL | XLookupString | needs-xlockmore | 584 |
| kallisti | gl | XL | - | needs-xlockmore | 357 |
| kallisti_model | gl | XL | - | - | 28657 |
| klein | gl | XL | XLookupString | float-heavy, needs-xlockmore | 3574 |
| klondike | gl | XL | XDestroyImage | needs-xlockmore | 803 |
| klondike-game | gl | XL | - | needs-xlockmore | 812 |
| lament | gl | XL | XDestroyImage, XLookupString | needs-xlockmore | 1801 |
| lament_model | gl | XL | - | - | 16259 |
| lavalite | gl | XL | XParseColor | needs-xlockmore | 1552 |
| lockward | gl | XL | XLookupString | needs-xlockmore | 964 |
| mapscroller | gl | XL | XDestroyImage, XGetPixel, XLookupString | readback, needs-xlockmore | 1672 |
| marching | gl | XL | - | - | 629 |
| maze3d | gl | XL | - | needs-xlockmore | 1958 |
| menger | gl | XL | XLookupString | needs-xlockmore | 574 |
| mirrorblob | gl | XL | - | needs-xlockmore | 1822 |
| moebius | gl | XL | XDestroyImage | needs-xlockmore | 794 |
| moebiusgears | gl | XL | XLookupString | needs-xlockmore | 446 |
| molecule | gl | XL | XLookupString, XParseColor | needs-xlockmore | 1716 |
| morph3d | gl | XL | - | needs-xlockmore | 841 |
| nakagin | gl | XL | XParseColor | needs-xlockmore | 1636 |
| noof | gl | XL | - | needs-xlockmore | 530 |
| normals | gl | XL | - | - | 50 |
| papercube | gl | XL | - | needs-xlockmore | 1111 |
| peepers | gl | XL | XChangeProperty, XDestroyImage, XInternAtom, XQueryPointer | float-heavy, needs-xlockmore | 1470 |
| photopile | gl | XL | - | needs-xlockmore | 869 |
| pinion | gl | XL | XLookupString, XQueryPointer | needs-xlockmore | 1497 |
| pipeobjs | gl | XL | - | - | 3265 |
| pipes | gl | XL | - | needs-xlockmore | 1208 |
| platonicfolding | gl | XL | XDestroyImage | needs-xlockmore | 3465 |
| polyhedra | gl | XL | - | - | 2459 |
| polyhedra-gl | gl | XL | XLookupString | needs-xlockmore | 687 |
| polytopes | gl | XL | XLookupString | needs-xlockmore | 3194 |
| projectiveplane | gl | XL | XLookupString | float-heavy, needs-xlockmore | 2643 |
| providence | gl | XL | - | float-heavy, needs-xlockmore | 811 |
| pulsar | gl | XL | - | needs-xlockmore | 509 |
| quasicrystal | gl | XL | XLookupString | needs-xlockmore | 494 |
| quaternion | gl | XL | - | - | 323 |
| queens | gl | XL | - | needs-xlockmore | 608 |
| quickhull | gl | XL | - | - | 1368 |
| raverhoop | gl | XL | XLookupString | needs-xlockmore | 771 |
| razzledazzle | gl | XL | - | needs-xlockmore | 728 |
| robot | gl | XL | - | - | 18540 |
| robot-wireframe | gl | XL | - | - | 155 |
| romanboy | gl | XL | - | needs-xlockmore | 2565 |
| rotator | gl | XL | - | - | 275 |
| rubik | gl | XL | - | needs-xlockmore | 2156 |
| rubikblocks | gl | XL | - | needs-xlockmore | 651 |
| s1_1 | gl | XL | - | - | 1734 |
| s1_2 | gl | XL | - | - | 1734 |
| s1_3 | gl | XL | - | - | 1734 |
| s1_4 | gl | XL | - | - | 1734 |
| s1_5 | gl | XL | - | - | 1734 |
| s1_6 | gl | XL | - | - | 1734 |
| s1_b | gl | XL | - | - | 506 |
| sballs | gl | XL | XDestroyImage | needs-xlockmore | 830 |
| seccam | gl | XL | - | - | 1389 |
| shark | gl | XL | - | - | 1396 |
| ships | gl | XL | - | - | 4977 |
| sierpinski3d | gl | XL | XLookupString | needs-xlockmore | 579 |
| skull_model | gl | XL | - | - | 23099 |
| skulloop | gl | XL | XParseColor | needs-xlockmore | 651 |
| skytentacles | gl | XL | XCreateImage, XDestroyImage, XLookupString, XParseColor, XPutPixel | float-heavy, needs-xlockmore | 1124 |
| sonar | gl | XL | - | needs-xlockmore | 1266 |
| sonar-icmp | gl | XL | - | - | 1850 |
| sonar-sim | gl | XL | - | - | 113 |
| sphere | gl | XL | - | - | 177 |
| sphereeversion | gl | XL | XDestroyImage | needs-xlockmore | 1420 |
| sphereeversion-analytic | gl | XL | - | needs-xlockmore | 2300 |
| sphereeversion-corrugations | gl | XL | - | needs-xlockmore | 2363 |
| spheremonics | gl | XL | - | needs-xlockmore | 926 |
| splitflap | gl | XL | XParseColor | needs-xlockmore | 1427 |
| splitflap_obj | gl | XL | - | - | 201 |
| splodesic | gl | XL | XLookupString | float-heavy, needs-xlockmore | 645 |
| sproingies | gl | XL | - | - | 903 |
| sproingiewrap | gl | XL | - | needs-xlockmore | 227 |
| squirtorus | gl | XL | XParseColor | needs-xlockmore | 1006 |
| stairs | gl | XL | XDestroyImage, XLookupString | needs-xlockmore | 601 |
| starwars | gl | XL | - | needs-xlockmore | 1080 |
| stonerview | gl | XL | - | needs-xlockmore | 156 |
| stonerview-move | gl | XL | - | - | 148 |
| stonerview-osc | gl | XL | - | - | 342 |
| stonerview-view | gl | XL | - | - | 120 |
| superquadrics | gl | XL | - | needs-xlockmore | 811 |
| surfaces | gl | XL | - | float-heavy, needs-xlockmore | 654 |
| swim | gl | XL | - | - | 234 |
| tangram | gl | XL | - | needs-xlockmore | 1074 |
| tangram_shapes | gl | XL | - | - | 226 |
| teapot | gl | XL | - | - | 274 |
| teeth_model | gl | XL | - | - | 15537 |
| texfont | gl | XL | XCreateImage, XCreatePixmap, XDestroyImage, XFreePixmap, XGetImage, XGetPixel, XGetSubImage, XShmGetImage | pixmaps, readback | 1491 |
| timetunnel | gl | XL | XDestroyImage | needs-xlockmore | 1259 |
| timezones | gl | XL | - | - | 3983 |
| toast | gl | XL | - | - | 191 |
| toast2 | gl | XL | - | - | 215 |
| toaster | gl | XL | - | - | 377 |
| toaster_base | gl | XL | - | - | 131 |
| toaster_handle | gl | XL | - | - | 65 |
| toaster_handle2 | gl | XL | - | - | 41 |
| toaster_jet | gl | XL | - | - | 179 |
| toaster_knob | gl | XL | - | - | 77 |
| toaster_slots | gl | XL | - | - | 107 |
| toaster_wing | gl | XL | - | - | 44 |
| topblock | gl | XL | XLookupString | needs-xlockmore | 891 |
| trackball | gl | XL | - | - | 148 |
| triangle | gl | XL | - | - | 16076 |
| tronbit | gl | XL | XLookupString | needs-xlockmore | 532 |
| tronbit_idle1 | gl | XL | - | - | 248 |
| tronbit_idle2 | gl | XL | - | - | 176 |
| tronbit_no | gl | XL | - | - | 1088 |
| tronbit_yes | gl | XL | - | - | 32 |
| tube | gl | XL | - | - | 394 |
| tunnel_draw | gl | XL | - | - | 494 |
| unicrud | gl | XL | XLookupString | needs-xlockmore | 1039 |
| unknownpleasures | gl | XL | XCreateImage, XDestroyImage, XGetPixel, XLookupString, XParseColor, XPutPixel | readback, needs-xlockmore | 736 |
| vigilance | gl | XL | XLookupString, XParseColor | needs-xlockmore | 1171 |
| voronoi | gl | XL | - | needs-xlockmore | 543 |
| whale | gl | XL | - | - | 1889 |
| winduprobot | gl | XL | XDestroyImage, XLookupString, XParseColor | float-heavy, needs-xlockmore | 2505 |
| worldpieces | gl | XL | XDestroyImage | float-heavy, needs-xlockmore | 2194 |
| xlock-gl-utils | gl | XL | XParseColor | - | 218 |
| xscreensaver-gl-visual | gl | XL | XOpenDisplay, XVisualIDFromVisual | - | 89 |
| xshadertoy | gl | XL | XFetchName, XQueryPointer, XStoreName | needs-xlockmore | 1192 |
