/* What xscreensaver's hacks/screenhackI.h gives a build without NLS: strings
 * tagged for translation are used as they are. Not derived from xscreensaver
 * code. Both screenhack.h and screenhackI.h include it, because the shim's
 * screenhack.h does not include screenhackI.h as upstream's does. */
#ifndef XSHIM_NLS_H
#define XSHIM_NLS_H

#define _(S) (S)
#define gettext(S) (S)

#endif
