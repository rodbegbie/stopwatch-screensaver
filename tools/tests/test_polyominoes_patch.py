import shutil
import subprocess
import sys
import textwrap
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import polyominoes_patch as pp

ROOT = Path(__file__).resolve().parents[2]
POLYOMINOES = ROOT / "firmware" / "src" / "hacks" / "polyominoes" / "polyominoes.c"
UPSTREAM = sorted((ROOT / "vendor").glob("xscreensaver-*/hacks/polyominoes.c"))


def test_the_function_no_longer_calls_itself():
    out = pp.make_the_flood_fill_iterative(pp.OLD)
    assert "count_adjacent_blanks(sp, count" not in out
    assert "malloc" in out and "free(stack)" in out


def test_the_stack_is_freed_on_every_path_that_allocates_it():
    out = pp.make_the_flood_fill_iterative(pp.OLD)
    assert out.count("malloc") == 1
    # The only early return comes before the allocation.
    assert out.index("return;") < out.index("malloc")


def test_a_changed_upstream_function_fails_loudly():
    with pytest.raises(ValueError, match="has upstream changed"):
        pp.make_the_flood_fill_iterative("static void other(void) {}\n")


def test_a_second_copy_fails_loudly():
    with pytest.raises(ValueError, match="found 2"):
        pp.make_the_flood_fill_iterative(pp.OLD + pp.OLD)


def test_the_copied_source_still_has_the_function_the_patch_replaces():
    pp.make_the_flood_fill_iterative(POLYOMINOES.read_text())


@pytest.mark.skipif(not shutil.which("cc"), reason="needs a C compiler")
def test_the_iterative_fill_marks_and_counts_what_the_recursion_did(tmp_path):
    """Run both versions on every 12 by 9 grid a seeded generator makes."""
    harness = (
        textwrap.dedent(
            """\
        #include <stdio.h>
        #include <stdlib.h>
        #include <string.h>
        typedef struct { int width, height; int *array; } polyominoesstruct;
        #define ARRAY(x,y) (sp->array[(x)*sp->height+(y)])
        static void recursive(polyominoesstruct *sp, int *count, int x, int y, int blank_mark)
        {
          if (ARRAY(x,y) == -1) {
            (*count)++;
            ARRAY(x,y) = blank_mark;
            if (x>=1) recursive(sp, count,x-1,y,blank_mark);
            if (x<sp->width-1) recursive(sp, count,x+1,y,blank_mark);
            if (y>=1) recursive(sp, count,x,y-1,blank_mark);
            if (y<sp->height-1) recursive(sp, count,x,y+1,blank_mark);
          }
        }
        %s
        int main(void) {
          srandom(7);
          int bad = 0, regions = 0;
          for (int trial = 0; trial < 3000; trial++) {
            polyominoesstruct a = {1 + random() %% 12, 1 + random() %% 9, 0};
            int n = a.width * a.height;
            a.array = malloc(n * sizeof(int));
            for (int i = 0; i < n; i++) a.array[i] = (random() %% 100 < 60) ? -1 : 3;
            polyominoesstruct b = a;
            b.array = malloc(n * sizeof(int));
            memcpy(b.array, a.array, n * sizeof(int));
            int x = random() %% a.width, y = random() %% a.height, mark = -2 - (int)(random() %% 3);
            int ca = 0, cb = 0;
            recursive(&a, &ca, x, y, mark);
            count_adjacent_blanks(&b, &cb, x, y, mark);
            if (ca != cb || memcmp(a.array, b.array, n * sizeof(int))) bad++;
            if (ca > 1) regions++;
            free(a.array); free(b.array);
          }
          printf("%%d %%d\\n", bad, regions);
          return 0;
        }
        """
        )
        % pp.NEW
    )
    src = tmp_path / "h.c"
    src.write_text(harness)
    exe = tmp_path / "h"
    subprocess.run(["cc", "-O1", "-o", str(exe), str(src)], check=True)
    bad, regions = subprocess.run(
        [str(exe)], check=True, capture_output=True, text=True
    ).stdout.split()
    assert bad == "0"
    assert int(regions) > 1000
