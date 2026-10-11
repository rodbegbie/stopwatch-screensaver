/* Stand-in for <X11/Intrinsic.h>. xlockmore.c includes it and needs nothing
 * from it; xmatrix also uses Xt timers for its text-mode cursor. The device
 * has no event loop, so a timer is never made (0 is "no timer") and never
 * fires. Not derived from xscreensaver code. */
#ifndef XSHIM_X11_INTRINSIC_H
#define XSHIM_X11_INTRINSIC_H

#include "x11shim/xshim.h"

typedef void *XtPointer;
typedef void *XtAppContext;
typedef unsigned long XtIntervalId;
typedef void (*XtTimerCallbackProc)(XtPointer, XtIntervalId *);

static inline XtAppContext XtDisplayToApplicationContext(Display *dpy) {
  (void)dpy;
  return 0;
}

static inline XtIntervalId XtAppAddTimeOut(XtAppContext app,
                                           unsigned long interval_ms,
                                           XtTimerCallbackProc proc,
                                           XtPointer closure) {
  (void)app, (void)interval_ms, (void)proc, (void)closure;
  return 0;
}

static inline void XtRemoveTimeOut(XtIntervalId id) { (void)id; }

#endif
