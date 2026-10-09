/* Builds pacman_ai.c with the ghosts' route search (recur_back_track) as a loop
 * over a heap stack instead of a recursion that reaches 453 levels, about 22 KB
 * against the loop task's 16 KB. The patched copy is written by patch_pacman.py
 * at build time; pacman/pacman_ai.c itself stays byte-identical to upstream. */
#include "pacman_ai_patched.c"
