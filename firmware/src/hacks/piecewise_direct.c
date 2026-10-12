/* Builds the unmodified piecewise.c with HAVE_JWXYZ defined after the framework
 * headers. Upstream defines it where the window system double-buffers for the
 * hack, and the hack then draws straight on the window. The shim can draw into
 * pixmaps, but two back buffers would cost 868 KB and a full-frame copy each
 * frame. */
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "screenhack.h"

#define HAVE_JWXYZ

#include "piecewise/piecewise.c"
