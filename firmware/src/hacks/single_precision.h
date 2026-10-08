/* Included by the *_single.c wrappers just before the unmodified hack source,
 * after every header the hack needs, so shared structs keep their `double`
 * layout. The ESP32-S3's FPU is single-precision only: `double` and the double
 * libm calls run in software. */
#ifndef HACKS_SINGLE_PRECISION_H
#define HACKS_SINGLE_PRECISION_H

#define double float
#define sqrt(x) sqrtf(x)
#define sin(x) sinf(x)
#define cos(x) cosf(x)
#define atan(x) atanf(x)
#define atan2(y, x) atan2f(y, x)
#define exp(x) expf(x)
#define fabs(x) fabsf(x)
#define fmod(x, y) fmodf(x, y)
#define pow(x, y) powf(x, y)
#define log(x) logf(x)

_Static_assert(sizeof(double) == sizeof(float),
               "single_precision.h must be in effect");

#endif
