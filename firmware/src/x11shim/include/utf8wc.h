/* The one function of xscreensaver's utils/utf8wc.h that a ported hack calls
 * (Barcode's word list). Not derived from xscreensaver code. */
#ifndef XSHIM_UTF8WC_H
#define XSHIM_UTF8WC_H

#ifdef __cplusplus
extern "C" {
#endif

/* A malloc'd copy of an all-ASCII string, which the caller frees, or NULL for
 * anything with a byte above 127: the board has no glyphs for it, and a caller
 * that gets NULL falls back to the string it had (Barcode does). ascii_p is
 * ignored: Latin-1 letters are not offered either. */
char *utf8_to_latin1(const char *string, int ascii_p);

#ifdef __cplusplus
}
#endif

#endif
