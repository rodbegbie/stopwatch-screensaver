#ifndef HACKS_PHOSPHOR_POEM_SET_H
#define HACKS_PHOSPHOR_POEM_SET_H

/* The poems Phosphor types, as the build chose them: scrolling by default,
 * -DPHOSPHOR_PAGED for pages that clear (tools/make_phosphor_poems.py makes
 * both). Shared with the test that counts the bytes of a poem. */
#ifdef PHOSPHOR_PAGED
#include "hacks/phosphor_poems.h"
#else
#include "hacks/phosphor_poems_scroll.h"
#endif

#endif
