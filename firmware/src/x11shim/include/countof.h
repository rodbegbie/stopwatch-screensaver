/* The number of elements of an array, which upstream's hacks/screenhackI.h
 * defines for every hack. Not derived from xscreensaver code. Screenhack.h,
 * screenhackI.h and utils.h include it, because the shim's screenhack.h does not
 * include screenhackI.h as upstream's does, and an xlockmore hack reaches only
 * the second. */
#ifndef XSHIM_COUNTOF_H
#define XSHIM_COUNTOF_H

#ifndef countof
#define countof(x) (sizeof(x) / sizeof((x)[0]))
#endif

#endif
