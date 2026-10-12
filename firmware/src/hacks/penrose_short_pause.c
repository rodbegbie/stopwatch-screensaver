/* Builds penrose.c with its two restart pauses cut from 3,141 and 31,415
 * frames to 900 (about 10 s). The patched copy is written by patch_penrose.py
 * at build time; penrose/penrose.c itself stays byte-identical to upstream. */
#include "penrose_patched.c"
