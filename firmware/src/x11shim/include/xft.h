/* Shim compatibility header: analogtv.c includes <xft.h> for its optional
 * Xft font branch. The stand-in is x11shim/xft.h; screenhack.h does not pull it
 * in, so only a file that asks for it sees it. */
#ifndef XSHIM_XFT_H
#define XSHIM_XFT_H
#include "x11shim/xft.h"
#endif
