/* Builds maze.c with its 1000 by 1000 limits cut to 80 by 80 (about 20 MB of
 * arrays down to about 130 KB). The patched copy is written by patch_maze.py
 * at build time; maze/maze.c itself stays byte-identical to upstream. */
#include "maze_patched.c"
