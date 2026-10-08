# Porting assessment

Rough effort to port each xscreensaver hack to the StopWatch X11 shim,
generated from xscreensaver 6.16 by `tools/score_hacks.py`. Regenerate
it with `uv run tools/score_hacks.py`.

## How to read this

- **408 hacks** were scanned: every `hacks/*.c` and `hacks/glx/*.c`.
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
| S | 2D, and every Xlib call is already in the shim | 24 |
| M | 2D, 1-4 missing calls, no pixmaps or pixel read-back | 55 |
| L | 2D, 5+ missing calls, or uses pixmaps or pixel read-back | 82 |
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

Only Pyro has been run so far (default settings, 466×466 canvas pushed to
the display every frame).

| Measurement | Value |
| --- | --- |
| Frame rate | about 23.5 fps steady |
| Frame rate during a hack restart | about 17 fps, recovering in a few seconds |
| Canvas cost | about 515 KB of PSRAM, with about 7.4 MB still free |
| Memory after 20 restarts | unchanged (no leak) |

Pyro's own delay setting asks for 100 steps per second, so the frame rate
is limited by pushing 434 KB to the display each frame. Pyro's `init`
builds two 6284-entry sine and cosine tables in double precision, which is
the cause of the dip at restart.

## Suggested order for shim stage 2

Missing calls across 2D hacks, ranked so calls that block
hacks needing few additions come first.

| Call | Score | 2D hacks needing it |
| --- | --- | --- |
| XCreatePixmap | 11.07 | 63 |
| XSetLineAttributes | 11.04 | 27 |
| XCopyArea | 10.8 | 52 |
| XFreePixmap | 9.18 | 59 |
| XFillRectangles | 8.44 | 15 |
| XSetWindowBackground | 5.68 | 17 |
| XDrawPoints | 5.59 | 9 |
| XDrawArc | 5.29 | 17 |
| XPutPixel | 5.06 | 34 |
| XDestroyImage | 5.03 | 40 |

## All hacks

| Hack | Kind | Effort | Missing X calls | Flags | LOC |
| --- | --- | --- | --- | --- | --- |
| analogtv | 2d | L | XClearArea, XCreateImage, XCreatePixmap, XDestroyImage, XDrawString, XFreePixmap, XGetImage, XGetPixel, XPutPixel, XQueryColors, XSetWindowBackground, XWriteBitmapFile | pixmaps, readback, text | 2454 |
| analogtv-cli | 2d | L | XClearArea, XCreateImage, XCreatePixmap, XCreatePixmapFromBitmapData, XDestroyImage, XDrawString, XFreePixmap, XGetImage, XGetPixel, XInitImage, XPutImage, XPutPixel, XQueryColor, XQueryColors, XSetWindowBackground | pixmaps, readback, text | 1121 |
| anemone | 2d | L | XCopyArea, XCreatePixmap, XFreePixmap, XSetLineAttributes | pixmaps | 458 |
| anemotaxis | 2d | L | XCopyArea, XCreatePixmap, XSetLineAttributes | pixmaps | 760 |
| ant | 2d | L | XChangeGC, XCreatePixmapFromBitmapData, XDrawArc, XFreePixmap, XSetLineAttributes | needs-xlockmore | 1351 |
| apple2 | 2d | L | XCreateImage, XCreatePixmap, XDestroyImage, XDrawString, XFreePixmap, XGetImage, XGetPixel, XLoadQueryFont, XPutPixel, XWriteBitmapFile | pixmaps, readback, text | 886 |
| apple2-main | 2d | L | XCreatePixmap, XDestroyImage, XFreePixmap, XGetImage, XGetPixel, XQueryColors | pixmaps, readback | 1642 |
| binaryhorizon | 2d | L | XCopyArea, XCreatePixmap, XDestroyImage, XFreePixmap, XGetImage, XPutImage, XPutPixel | pixmaps, readback | 624 |
| binaryring | 2d | L | XCopyArea, XCreatePixmap, XDestroyImage, XFreePixmap, XGetImage, XPutImage, XPutPixel | pixmaps, readback | 577 |
| blitspin | 2d | L | XCopyArea, XCopyPlane, XCreatePixmap, XDestroyImage, XDisplayHeight, XDisplayWidth, XFreePixmap, XGetImage, XPutImage, XScreenNumberOfScreen, XSetClipMask | pixmaps, readback, clipmask | 467 |
| boxfit | 2d | L | XCreatePixmap, XDestroyImage, XDrawArc, XDrawRectangle, XFreePixmap, XGetImage, XGetPixel, XSetWindowBackground | pixmaps, readback | 573 |
| bsod | 2d | L | XChangeGC, XClearArea, XCopyArea, XCopyPlane, XCreateImage, XCreatePixmap, XCreatePixmapFromBitmapData, XDestroyImage, XDrawRectangle, XFetchName, XFreePixmap, XGetImage, XGetPixel, XPutImage, XPutPixel, XQueryColor, XSetBackground, XSetClipMask, XSetClipOrigin, XSetLineAttributes, XSetPlaneMask, XSetWindowBackground, XStoreName | pixmaps, readback, clipmask | 7809 |
| bubbles | 2d | L | XCopyArea, XDrawArc, XFreePixmap, XSetClipMask, XSetClipOrigin | pixmaps, clipmask | 1468 |
| bumps | 2d | L | XCreatePixmap, XDestroyImage, XGetImage, XGetPixel, XParseColor, XQueryColors, XSetWindowBackground, XSync | pixmaps, readback | 705 |
| ccurve | 2d | L | XCopyArea, XCreatePixmap, XFreePixmap | pixmaps | 872 |
| compass | 2d | L | XCopyArea, XCreatePixmap, XDrawSegments, XFreePixmap | pixmaps, float-heavy | 999 |
| crystal | 2d | L | XCreateColormap, XFreeColormap, XInstallColormap, XParseColor, XSetFunction, XSetWindowColormap | xor, float-heavy, needs-xlockmore | 1286 |
| decayscreen | 2d | L | XCopyArea, XCreatePixmap | pixmaps | 392 |
| deluxe | 2d | L | XCopyArea, XCreatePixmap, XDrawArc, XFreePixmap | pixmaps, float-heavy | 480 |
| distort | 2d | L | XCopyArea, XCreatePixmap, XDestroyImage, XFreePixmap, XGetImage, XGetPixel, XPutImage, XPutPixel | pixmaps, readback | 894 |
| droste | 2d | L | XCreatePixmap, XDestroyImage, XFreePixmap, XGetImage, XGetPixel, XLookupString, XPutPixel | pixmaps, readback | 686 |
| fiberlamp | 2d | L | XAllocNamedColor, XCopyArea, XCreatePixmap, XFreePixmap, XSetGraphicsExposures, XSetLineAttributes, XTranslateCoordinates | pixmaps, needs-xlockmore | 480 |
| filmleader | 2d | L | XCreateImage, XCreatePixmap, XDestroyImage, XDrawArc, XFreePixmap, XGetImage, XGetPixel, XLookupString, XPutImage, XPutPixel, XSetLineAttributes | pixmaps, readback | 548 |
| flag | 2d | L | XCopyArea, XCreateImage, XCreatePixmap, XDestroyImage, XDrawString, XFreeFont, XFreePixmap, XGetImage, XGetPixel, XLoadQueryFont, XPutPixel, XSetGraphicsExposures, XTextExtents | pixmaps, readback, text, needs-xlockmore | 570 |
| flow | 2d | L | XCopyArea, XCreatePixmap, XDrawSegments, XFreePixmap, XSetGraphicsExposures, XSetLineAttributes | pixmaps, needs-xlockmore | 1216 |
| fluidballs | 2d | L | XCopyArea, XCreatePixmap, XFreePixmap, XQueryPointer, XSelectInput, XTranslateCoordinates | pixmaps | 881 |
| fontglide | 2d | L | XCopyArea, XCreateImage, XCreatePixmap, XDestroyImage, XDrawRectangle, XDrawString, XDrawString16, XFreeFont, XFreePixmap, XGetAtomName, XGetGeometry, XGetImage, XGetPixel, XLoadQueryFont, XLookupString, XPutImage, XPutPixel, XSetClipMask, XSetClipOrigin, XSetFont, XTextExtents, XTextExtents16 | pixmaps, readback, text, clipmask | 2474 |
| fuzzyflakes | 2d | L | XCopyArea, XCreatePixmap, XFreePixmap, XParseColor | pixmaps | 655 |
| glitchpeg | 2d | L | XCreateImage, XDestroyImage, XGetPixel, XPutImage, XPutPixel | readback | 466 |
| goop | 2d | L | XCopyArea, XCopyPlane, XCreatePixmap, XSetFunction, XSetPlaneMask | pixmaps, xor | 651 |
| halftone | 2d | L | XCopyArea, XCreatePixmap, XFreePixmap | pixmaps | 413 |
| halo | 2d | L | XCopyPlane, XCreatePixmap, XFreePixmap, XSetBackground | pixmaps | 459 |
| ifs | 2d | L | XCopyArea, XCreatePixmap, XFillRectangles, XFreePixmap | pixmaps | 560 |
| imsmap | 2d | L | XCreateImage, XDestroyImage, XPutImage, XPutPixel, XSetBackground | - | 426 |
| interference | 2d | L | XCopyArea, XCreatePixmap, XFreePixmap, XPutPixel, XShmGetEventBase | pixmaps | 1002 |
| intermomentary | 2d | L | XCopyArea, XCreatePixmap, XFreePixmap, XQueryColor, XSetFillStyle, XSetTile | pixmaps | 605 |
| juggle | 2d | L | XDrawArc, XDrawImageString, XDrawString, XFreeFontInfo, XLoadQueryFont, XSetLineAttributes, XTextWidth | text, float-heavy, needs-xlockmore | 2798 |
| julia | 2d | L | XCreatePixmap, XCreatePixmapCursor, XCreatePixmapFromBitmapData, XDefineCursor, XDrawArc, XFillRectangles, XFreeCursor, XFreePixmap, XSetFillStyle, XSetStipple, XSetTSOrigin, XUndefineCursor | pixmaps, float-heavy, needs-xlockmore | 451 |
| kumppa | 2d | L | XCopyArea, XSetGraphicsExposures | pixmaps | 545 |
| lcdscrub | 2d | L | XCreateImage, XCreatePixmap, XDestroyImage, XFreePixmap, XGetPixel, XPutImage, XPutPixel, XSetBackground, XSetClipMask | pixmaps, readback, clipmask | 399 |
| maze | 2d | L | XCopyArea, XCopyPlane, XFreePixmap, XGetGeometry, XSetBackground, XSetClipMask, XSetClipOrigin, XSetLineAttributes, XSync | pixmaps, clipmask | 1681 |
| memscroller | 2d | L | XCopyArea, XDrawRectangle | pixmaps | 626 |
| metaballs | 2d | L | XCreateImage, XDestroyImage, XFree, XListPixmapFormats, XParseColor, XPutImage, XPutPixel, XSetWindowBackground | - | 438 |
| moire2 | 2d | L | XCopyArea, XCopyPlane, XCreatePixmap, XDrawArc, XFreePixmap, XSetBackground, XSetFunction | pixmaps, xor | 363 |
| nerverot | 2d | L | XCopyArea, XCreatePixmap | pixmaps, float-heavy | 1367 |
| noseguy | 2d | L | XCopyArea, XCreateImage, XCreatePixmap, XDestroyImage, XDrawRectangle, XFreePixmap, XGetImage, XGetPixel, XPutImage, XPutPixel, XSetClipMask, XSetClipOrigin, XSetLineAttributes | pixmaps, readback, clipmask | 720 |
| pacman | 2d | L | XCopyArea, XCreatePixmap, XDrawArc, XDrawString, XFreePixmap, XGetGeometry, XLoadQueryFont, XSetClipMask, XSetClipOrigin, XSetFillStyle, XSetLineAttributes | pixmaps, text, clipmask, needs-xlockmore | 1479 |
| phosphor | 2d | L | XCopyArea, XCopyPlane, XCreateImage, XCreatePixmap, XDestroyImage, XFreePixmap, XGetImage, XGetPixel, XPutImage, XPutPixel, XQueryColor, XSetClipMask, XSetClipOrigin, XWriteBitmapFile | pixmaps, readback, clipmask | 1260 |
| piecewise | 2d | L | XCopyArea, XCreatePixmap, XDrawArcs | pixmaps | 1036 |
| polyominoes | 2d | L | XCreateImage, XDestroyImage, XDrawRectangle, XDrawSegments, XFillRectangles, XPutImage, XSetLineAttributes | needs-xlockmore | 2370 |
| pong | 2d | L | XCreatePixmap, XCreatePixmapCursor, XDefineCursor, XDestroyImage, XGrabPointer, XHeightMMOfScreen, XHeightOfScreen, XLookupString, XQueryPointer, XSelectInput, XUngrabPointer, XWarpPointer | pixmaps | 1109 |
| popsquares | 2d | L | XCopyArea, XCreatePixmap, XFreePixmap, XQueryColor | pixmaps | 310 |
| recanim | 2d | L | XCopyArea, XCreateImage, XCreatePixmap, XDestroyImage, XFetchName, XFreePixmap, XGetSubImage, XStoreName, XSync | pixmaps | 449 |
| ripples | 2d | L | XDestroyImage, XGetImage, XGetPixel, XPutPixel, XQueryColor | readback | 1127 |
| rocks | 2d | L | XCopyPlane, XCreatePixmap, XFreePixmap, XQueryColor, XSetGraphicsExposures | pixmaps | 562 |
| rotzoomer | 2d | L | XCreatePixmap, XDestroyImage, XFreePixmap, XGetImage, XGetPixel, XPutPixel | pixmaps, readback | 601 |
| screenhack | 2d | L | XBell, XChangeProperty, XCreateColormap, XGetAtomName, XIfEvent, XInternAtom, XLookupString, XNextEvent, XPending, XSelectInput, XSendEvent, XSetErrorHandler, XSetWindowBackground, XSetWindowColormap, XSync, XVisualIDFromVisual | - | 1099 |
| shadebobs | 2d | L | XCreateImage, XDestroyImage, XFree, XGetPixel, XListPixmapFormats, XParseColor, XPutImage, XPutPixel, XSetWindowBackground | readback | 474 |
| slidescreen | 2d | L | XCopyArea, XDrawRectangle, XFree, XParseColor, XQueryColors | pixmaps | 507 |
| slip | 2d | L | XCopyArea, XCreatePixmap, XFreePixmap, XSetGraphicsExposures | pixmaps, needs-xlockmore | 376 |
| speedmine | 2d | L | XCopyArea, XCreatePixmap, XFreePixmap, XQueryColor, XSetClipMask | pixmaps, clipmask | 1659 |
| spotlight | 2d | L | XCopyArea, XCreatePixmap, XDrawRectangle, XFreePixmap, XSetClipMask, XSetClipOrigin | pixmaps, clipmask | 355 |
| strange | 2d | L | XCopyArea, XCopyPlane, XCreatePixmap, XDrawPoints, XFillRectangles, XFreePixmap, XPutPixel, XQueryColor, XQueryColors, XSetBackground, XSetFunction, XSetGraphicsExposures | pixmaps, xor, needs-xlockmore | 1353 |
| swirl | 2d | L | XCreateColormap, XFree, XFreeColormap, XInstallColormap, XPutPixel, XQueryColor, XSetWMColormapWindows, XSetWindowColormap, XStoreColors | needs-xlockmore | 1447 |
| t3d | 2d | L | XAllocColorCells, XCopyArea, XCreatePixmap, XDrawSegments, XFreePixmap, XGetImage, XLookupString, XPutImage, XQueryPointer, XStoreColors | pixmaps, readback, float-heavy | 991 |
| tessellimage | 2d | L | XCopyArea, XCreateImage, XCreatePixmap, XDestroyImage, XFreePixmap, XGetGeometry, XGetImage, XGetPixel, XPutImage, XPutPixel, XQueryColor | pixmaps, readback | 996 |
| testx11 | 2d | L | XClearArea, XCopyArea, XCopyPlane, XCreatePixmap, XCreatePixmapFromBitmapData, XDestroyImage, XDrawArc, XDrawPoints, XDrawRectangle, XDrawSegments, XDrawString, XFillRectangles, XFreePixmap, XGetImage, XLoadFont, XLookupString, XPutImage, XPutPixel, XSetBackground, XSetClipMask, XSetWindowBackground, XSync | pixmaps, readback, text, clipmask | 968 |
| truchet | 2d | L | XCopyArea, XCreatePixmap, XDrawArc, XSetLineAttributes | pixmaps | 541 |
| twang | 2d | L | XCreatePixmap, XDestroyImage, XFreePixmap, XGetImage, XGetPixel, XPutPixel | pixmaps, readback | 791 |
| vfeedback | 2d | L | XCopyArea, XCreateImage, XCreatePixmap, XDestroyImage, XFreePixmap, XGetImage, XGetPixel, XLookupString, XPutPixel, XSelectInput | pixmaps, readback | 592 |
| wander | 2d | L | XCopyArea, XCreatePixmap | pixmaps | 284 |
| whirlygig | 2d | L | XCopyArea, XCreatePixmap, XDrawString | pixmaps, text, float-heavy | 741 |
| wormhole | 2d | L | XCopyArea, XCreatePixmap, XFreePixmap, XSetLineAttributes | pixmaps | 734 |
| xanalogtv | 2d | L | XCreatePixmap, XDestroyImage, XFreePixmap, XGetImage, XLookupString | pixmaps, readback | 689 |
| xflame | 2d | L | XCreateImage, XDestroyImage, XGetPixel, XPutPixel, XQueryColor | readback | 826 |
| ximage-loader | 2d | L | XCreateImage, XCreatePixmap, XDestroyImage, XFreePixmap, XGetImage, XGetPixel, XPutImage, XPutPixel | pixmaps, readback | 679 |
| xjack | 2d | L | XClearArea, XCopyArea | pixmaps | 508 |
| xlyap | 2d | L | XCopyArea, XCreatePixmap, XDrawPoints, XFreePixmap, XGetGeometry, XLookupString, XPending, XStoreColors | pixmaps | 1939 |
| xmatrix | 2d | L | XChangeGC, XCopyArea, XCreateImage, XCreatePixmap, XDestroyImage, XDrawRectangle, XFreePixmap, XGetImage, XGetPixel, XLookupString, XPutImage, XPutPixel | pixmaps, readback | 1915 |
| xscreensaver-getimage | 2d | L | XChangeProperty, XCopyArea, XCreateImage, XCreatePixmap, XDefaultColormapOfScreen, XDeleteProperty, XDestroyImage, XFree, XFreePixmap, XGetClassHint, XGetGeometry, XGetImage, XGetPixel, XGetVisualInfo, XGetWMNormalHints, XGetWindowProperty, XIfEvent, XInstallColormap, XInternAtom, XMapRaised, XPutImage, XPutPixel, XQueryColors, XQueryTree, XRootWindowOfScreen, XScreenNumberOfScreen, XSelectInput, XSetErrorHandler, XSetWMNormalHints, XStoreColors, XSync, XUnmapWindow, XVisualIDFromVisual | pixmaps, readback | 2483 |
| xsublim | 2d | L | XDestroyImage, XDrawString, XGetImage, XGrabServer, XLoadQueryFont, XPutImage, XRootWindowOfScreen, XSetErrorHandler, XSync, XTextWidth, XUngrabServer | readback, text | 808 |
| zoom | 2d | L | XCopyArea, XCreatePixmap, XDestroyImage, XFreePixmap, XGetImage, XGetPixel, XSetWindowBackground | pixmaps, readback | 290 |
| apollonian | 2d | M | XDrawArc, XQueryColor | needs-xlockmore | 820 |
| attraction | 2d | M | XDrawRectangle, XQueryPointer | - | 1115 |
| barcode | 2d | M | XCreateImage, XDestroyImage, XPutImage | - | 2055 |
| blaster | 2d | M | XFillArcs | - | 1208 |
| bouboule | 2d | M | XFillArcs, XSetFunction | xor, needs-xlockmore | 860 |
| braid | 2d | M | XSetLineAttributes | needs-xlockmore | 444 |
| celtic | 2d | M | XDrawArc, XSetLineAttributes | - | 1141 |
| cloudlife | 2d | M | XDrawPoints | - | 440 |
| coral | 2d | M | XFillRectangles | - | 328 |
| critical | 2d | M | XChangeGC | - | 462 |
| cwaves | 2d | M | XSetLineAttributes | - | 219 |
| cynosure | 2d | M | XCreateBitmapFromData, XDrawRectangle, XFreePixmap, XSetWindowBackground | - | 457 |
| deco | 2d | M | XChangeGC, XDrawRectangle, XSetLineAttributes, XStoreColors | - | 345 |
| demon | 2d | M | XChangeGC, XCreatePixmapFromBitmapData, XFillRectangles, XFreePixmap | needs-xlockmore | 953 |
| discrete | 2d | M | XDrawPoints | needs-xlockmore | 442 |
| drift | 2d | M | XDrawPoints | needs-xlockmore | 674 |
| eruption | 2d | M | XPutPixel, XSetWindowBackground | - | 608 |
| euler2d | 2d | M | XDrawArc, XDrawSegments, XSetLineAttributes | float-heavy, needs-xlockmore | 893 |
| fadeplot | 2d | M | XFillRectangles | needs-xlockmore | 243 |
| fireworkx | 2d | M | XCreateImage, XDestroyImage, XPutImage, XSync | - | 882 |
| flame | 2d | M | XFillRectangles | - | 457 |
| forest | 2d | M | XFillArcs, XSetLineAttributes | needs-xlockmore | 241 |
| galaxy | 2d | M | XFillRectangles | needs-xlockmore | 462 |
| grav | 2d | M | XDrawArc | needs-xlockmore | 360 |
| greynetic | 2d | M | XChangeGC, XCreatePixmapFromBitmapData | - | 297 |
| hexadrop | 2d | M | XSetWindowBackground | - | 446 |
| hopalong | 2d | M | XFillRectangles | needs-xlockmore | 563 |
| interaggregate | 2d | M | XParseColor | - | 989 |
| kaleidescope | 2d | M | XDrawSegments, XSetLineAttributes | - | 514 |
| laser | 2d | M | XChangeGC | needs-xlockmore | 356 |
| lisa | 2d | M | XDrawPoints, XMaxRequestSize, XSetLineAttributes | needs-xlockmore | 744 |
| lissie | 2d | M | XDrawArc | needs-xlockmore | 323 |
| lmorph | 2d | M | XSetLineAttributes | float-heavy | 580 |
| loop | 2d | M | XChangeGC, XCreatePixmapFromBitmapData, XFillRectangles, XFreePixmap | needs-xlockmore | 1700 |
| marbling | 2d | M | XLookupString, XPutPixel | - | 635 |
| moire | 2d | M | XPutPixel, XQueryColor | - | 253 |
| munch | 2d | M | XSetFunction | xor | 462 |
| pacman_ai | 2d | M | XQueryPointer | - | 879 |
| penetrate | 2d | M | XDrawArc, XSetLineAttributes, XSync | - | 1037 |
| penrose | 2d | M | XSetLineAttributes | needs-xlockmore | 1342 |
| qix | 2d | M | XQueryColor, XSetWindowBackground | - | 642 |
| rdbomb | 2d | M | XListPixmapFormats, XSetWindowBackground | - | 571 |
| rorschach | 2d | M | XFillRectangles | - | 227 |
| rotor | 2d | M | XSetLineAttributes | needs-xlockmore | 394 |
| scooter | 2d | M | XSetLineAttributes | needs-xlockmore | 975 |
| sierpinski | 2d | M | XDrawPoints | needs-xlockmore | 215 |
| sphere | 2d | M | XDrawPoints | needs-xlockmore | 304 |
| starfish | 2d | M | XSetWindowBackground | - | 564 |
| substrate | 2d | M | XParseColor | - | 780 |
| thornbird | 2d | M | XFillRectangles | needs-xlockmore | 270 |
| vermiculate | 2d | M | XSetWindowBackground | - | 1229 |
| whirlwindwarp | 2d | M | XDrawRectangle | - | 509 |
| worm | 2d | M | XClearArea, XFillRectangles, XSetFunction | xor, needs-xlockmore | 434 |
| xlockmore | 2d | M | XSelectInput | - | 795 |
| xrayswarm | 2d | M | XSetGraphicsExposures | - | 1235 |
| abstractile | 2d | S | - | - | 1625 |
| ansi-tty | 2d | S | - | - | 1952 |
| asm6502 | 2d | S | - | - | 2275 |
| bubbles-default | 2d | S | - | - | 155 |
| delaunay | 2d | S | - | - | 304 |
| epicycle | 2d | S | - | - | 803 |
| ffmpeg-out | 2d | S | - | - | 732 |
| fps | 2d | S | - | - | 306 |
| helix | 2d | S | - | - | 358 |
| hyperball | 2d | S | - | - | 2464 |
| hypercube | 2d | S | - | - | 576 |
| lightning | 2d | S | - | needs-xlockmore | 602 |
| m6502 | 2d | S | - | - | 288 |
| mountain | 2d | S | - | needs-xlockmore | 283 |
| pacman_level | 2d | S | - | - | 774 |
| pedal | 2d | S | - | - | 339 |
| petri | 2d | S | - | - | 780 |
| pyro | 2d | S | - | - | 373 |
| spiral | 2d | S | - | needs-xlockmore | 331 |
| squiral | 2d | S | - | - | 335 |
| triangle | 2d | S | - | needs-xlockmore | 355 |
| vines | 2d | S | - | needs-xlockmore | 190 |
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
