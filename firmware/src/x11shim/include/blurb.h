/* Stand-in for xscreensaver's utils/blurb.h, which ansi-tty.c includes for
 * `progname` in its debug messages. Not derived from xscreensaver code.
 *
 * ansi-tty.c writes to stderr only when its `debug_p` is above zero, and
 * nothing sets it. On the board newlib's `stderr` expands to `_impure_ptr`,
 * which this toolchain does not define, so the link fails on it. ansi-tty.c is
 * the one file that includes this header, so it gets a null stream there; the
 * host keeps the real one. */
#ifndef XSHIM_BLURB_H
#define XSHIM_BLURB_H

#include "x11shim/xshim.h"

#ifdef ESP32S3
#include <stdio.h>
#undef stderr
#define stderr ((FILE *)0)
#endif

#endif
