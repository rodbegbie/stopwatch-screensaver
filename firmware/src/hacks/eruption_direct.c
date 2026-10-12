/* Builds the unmodified eruption.c with HAVE_INTTYPES_H: without it the hack
 * typedefs uint32_t as `unsigned long` itself, which clashes with the C
 * library's. */
#define HAVE_INTTYPES_H 1
#include "eruption/eruption.c"
