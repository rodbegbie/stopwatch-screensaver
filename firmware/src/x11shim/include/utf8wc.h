/* The functions of xscreensaver's utils/utf8wc.h that a ported hack calls
 * (Barcode's word list, Phosphor's terminal). Not derived from xscreensaver
 * code. */
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

/* Writes the UTF-8 bytes of uc and then a NUL, and returns how many bytes it
 * wrote before the NUL. Writes nothing and returns 0 when the character needs
 * more than `length` bytes: pass the buffer's size minus one. */
int utf8_encode(unsigned long uc, char *out, long length);

/* Reads the first character: returns the bytes it took and stores the
 * character. Returns 0 (storing nothing) when the string ends in the middle of
 * one. A byte that starts no sequence is taken as itself, one byte long. */
long utf8_decode(const unsigned char *in, long length, unsigned long *unicode_ret);

#ifdef __cplusplus
}
#endif

#endif
