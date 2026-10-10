/* Shim replacement for xscreensaver's utils/doubletime.h. Not derived from
 * xscreensaver code. */
#ifndef XSHIM_DOUBLETIME_H
#define XSHIM_DOUBLETIME_H

#ifdef __cplusplus
extern "C" {
#endif

/* Seconds since the epoch, to the microsecond. The device has no clock set,
 * so there it counts from boot; hacks only ever subtract two readings. */
double double_time(void);

/* Tests only: replaces the clock so a hack that animates from wall time draws
 * the same frames every run. NULL restores the real one. */
void xshim_set_clock(double (*clock)(void));

#ifdef __cplusplus
}
#endif

#endif
