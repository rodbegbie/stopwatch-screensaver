/* Shim replacement for xscreensaver's utils/easing.h. Not derived from
 * xscreensaver code.
 *
 * Only the functions a ported hack has asked for exist: a hack that names
 * another (EASE_IN_QUAD, ...) fails to compile, which is the cue to add it. */
#ifndef XSHIM_EASING_H
#define XSHIM_EASING_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  EASE_NONE,
  EASE_IN_SINE,
  EASE_OUT_SINE,
  EASE_IN_OUT_SINE
} easing_function;

/* Maps x in [0, 1] to [0, 1] along the curve. x is not clamped: the sine
 * curves carry on past the ends, as upstream's do. */
double ease(easing_function, double x);

#ifdef __cplusplus
}
#endif

#endif
