#ifndef HACKS_SINGLE_FRAND_H
#define HACKS_SINGLE_FRAND_H

/* For a single-precision wrapper that calls frand: include after
 * single_precision.h. screenhack.h's frand has `1.0` inside its macro body,
 * which the build-time float literal pass cannot reach, so it divides in
 * double (software on the S3). Opt-in: the older wrappers keep the original,
 * as their frame hashes are pinned with it. */
#undef frand
#define frand(f) (((float)random() / ((float)RAND_MAX + 1.0f)) * (float)(f))

#endif
