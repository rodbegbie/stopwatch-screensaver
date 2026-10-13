#ifndef XSHIM_POW2_H
#define XSHIM_POW2_H

#include <stddef.h>

/* The floor of log base 2, or -1 for 0. */
int i_log2(size_t x);
/* For x above 0 only: i_log2 without the check. */
#define i_log2_fast(x) i_log2(x)
/* The smallest power of 2 that is at least x (1 for 0). */
size_t to_pow2(size_t x);

#endif
