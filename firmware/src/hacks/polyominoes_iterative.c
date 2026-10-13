/* Builds polyominoes.c with its flood fill made iterative at build time
 * (tools/polyominoes_patch.py): the recursive one overflowed the 16 KB loop
 * stack on the board. The copy in polyominoes/ stays byte-identical. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xlockmore.h"

#include "polyominoes_patched.c"
