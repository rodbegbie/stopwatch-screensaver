/* Builds pacman_level.c with its random level generator switched off, so
 * Pacman always plays the fixed level. The generator recurses about 200 deep
 * with a 1.3 KB frame each, far more than the loop task's 16 KB stack. The
 * patched copy is written by patch_pacman.py at build time; pacman/pacman_level.c
 * itself stays byte-identical to upstream. */
#include "pacman_level_patched.c"
