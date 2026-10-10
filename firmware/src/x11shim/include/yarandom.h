/* Stand-in for xscreensaver's utils/yarandom.h, built on the C library's
 * random(). Not derived from xscreensaver code. */
#ifndef XSHIM_YARANDOM_H
#define XSHIM_YARANDOM_H

#include <stdlib.h>

#define LRAND() ((long)random())
#define NRAND(n) ((int)(LRAND() % (n)))
#define MAXRAND (2147483648.0)

/* The GL helpers (rotator.c) use frand; other hacks may define their own. */
#ifdef USE_GL
#define frand(f) ((double)LRAND() / MAXRAND * (double)(f))
#endif

/* A non-zero seed makes the sequence repeatable; zero keeps the current one. */
void ya_rand_init(unsigned int seed);

#endif
