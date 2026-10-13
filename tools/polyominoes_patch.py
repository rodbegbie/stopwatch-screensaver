"""Build-time patch for Polyominoes: an iterative flood fill.

count_adjacent_blanks marks and counts the connected blank cells around a
point, calling itself for each of the four neighbours, so its depth is the
size of the region: up to the whole grid, which is 360 cells in several
puzzles (20 by 18, 72 by 5, ...). On the board each level takes 32 bytes of
the 16 KB loop stack, and the solver's own recursion sits underneath it: the
board panicked (a double exception) in count_adjacent_blanks the first time
Polyominoes ran.

The copied polyominoes.c stays byte-identical to upstream. The firmware build
runs this on its text (firmware/patch_polyominoes.py) and the wrapper
src/hacks/polyominoes_iterative.c includes the result. The fill is the same
set of cells whatever the order they are visited in, so the marks and the
count are what the recursion gave. The explicit stack holds cell numbers
(x * height + y), at most one for each cell of the grid, since a cell is
marked when it is pushed.

The replacement is checked, so a change upstream fails the build loudly.
"""

OLD = """\
static void count_adjacent_blanks(polyominoesstruct *sp, int *count, int x, int y, int blank_mark)
{

  if (ARRAY(x,y) == -1) {
    (*count)++;
    ARRAY(x,y) = blank_mark;
    if (x>=1) count_adjacent_blanks(sp, count,x-1,y,blank_mark);
    if (x<sp->width-1) count_adjacent_blanks(sp, count,x+1,y,blank_mark);
    if (y>=1) count_adjacent_blanks(sp, count,x,y-1,blank_mark);
    if (y<sp->height-1) count_adjacent_blanks(sp, count,x,y+1,blank_mark);
  }
}
"""

NEW = """\
static void count_adjacent_blanks(polyominoesstruct *sp, int *count, int x, int y, int blank_mark)
{
  int *stack, top = 0;

  if (ARRAY(x,y) != -1) return;
  stack = (int *) malloc(sizeof(int) * sp->width * sp->height);
  if (!stack) abort();
  (*count)++;
  ARRAY(x,y) = blank_mark;
  stack[top++] = x*sp->height+y;
  while (top > 0) {
    int cell = stack[--top];
    x = cell / sp->height;
    y = cell % sp->height;
    if (x>=1 && ARRAY(x-1,y) == -1) {
      (*count)++;
      ARRAY(x-1,y) = blank_mark;
      stack[top++] = (x-1)*sp->height+y;
    }
    if (x<sp->width-1 && ARRAY(x+1,y) == -1) {
      (*count)++;
      ARRAY(x+1,y) = blank_mark;
      stack[top++] = (x+1)*sp->height+y;
    }
    if (y>=1 && ARRAY(x,y-1) == -1) {
      (*count)++;
      ARRAY(x,y-1) = blank_mark;
      stack[top++] = x*sp->height+y-1;
    }
    if (y<sp->height-1 && ARRAY(x,y+1) == -1) {
      (*count)++;
      ARRAY(x,y+1) = blank_mark;
      stack[top++] = x*sp->height+y+1;
    }
  }
  free(stack);
}
"""


def make_the_flood_fill_iterative(source: str) -> str:
    found = source.count(OLD)
    if found != 1:
        raise ValueError(
            f"expected 1 count_adjacent_blanks in polyominoes.c, found {found}; "
            "has upstream changed?"
        )
    return source.replace(OLD, NEW)
