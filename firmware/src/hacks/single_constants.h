#ifndef HACKS_SINGLE_CONSTANTS_H
#define HACKS_SINGLE_CONSTANTS_H

/* For a single-precision wrapper: include after single_precision.h. MAXRAND
 * (yarandom.h) and M_PI, M_PI_2 and M_PI_4 (math.h) are macros whose body is a
 * decimal literal, so the build-time float literal pass cannot reach them and
 * `LRAND() / MAXRAND` or `x * M_PI` widens to double, runs in software on the
 * S3 and truncates back. Opt-in: a wrapper's frame hashes are pinned with the
 * originals. */
#undef MAXRAND
#define MAXRAND (2147483648.0f)
#undef M_PI
#define M_PI 3.14159265358979323846f
#undef M_PI_2
#define M_PI_2 1.57079632679489661923f
#undef M_PI_4
#define M_PI_4 0.78539816339744830962f

#endif
