import difflib
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import maze_patch as mp  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
MAZE_C = ROOT / "firmware" / "src" / "hacks" / "maze" / "maze.c"

SAMPLE = """#include <stdio.h>

#define MAX_MAZE_SIZE_X\t1000
#define MAX_MAZE_SIZE_Y\t1000

#define MOVE_LIST_SIZE  (MAX_MAZE_SIZE_X * MAX_MAZE_SIZE_Y)
"""


def test_both_limits_are_replaced():
    out = mp.patch_maze(SAMPLE, 80)
    assert "#define MAX_MAZE_SIZE_X 80\n" in out
    assert "#define MAX_MAZE_SIZE_Y 80\n" in out
    assert "1000" not in out


def test_nothing_else_changes():
    out = mp.patch_maze(SAMPLE, 80)
    changed = [
        line
        for line in difflib.unified_diff(
            SAMPLE.splitlines(), out.splitlines(), lineterm="", n=0
        )
        if line[0] in "+-" and line[:3] not in ("+++", "---")
    ]
    assert len(changed) == 4  # two lines out, two lines in


def test_missing_define_is_an_error():
    with pytest.raises(ValueError, match="MAX_MAZE_SIZE_Y"):
        mp.patch_maze(SAMPLE.replace("#define MAX_MAZE_SIZE_Y\t1000\n", ""), 80)


def test_unexpected_upstream_value_is_an_error():
    with pytest.raises(ValueError, match="MAX_MAZE_SIZE_X"):
        mp.patch_maze(SAMPLE.replace("X\t1000", "X\t2000"), 80)


def test_a_repeated_define_is_an_error():
    with pytest.raises(ValueError, match="MAX_MAZE_SIZE_X"):
        mp.patch_maze(SAMPLE + "#define MAX_MAZE_SIZE_X 1000\n", 80)


@pytest.mark.skipif(not MAZE_C.exists(), reason="maze.c not copied yet")
def test_real_maze_c_changes_exactly_two_lines():
    original = MAZE_C.read_text()
    out = mp.patch_maze(original, mp.MAZE_LIMIT)
    diff = [
        line
        for line in difflib.unified_diff(
            original.splitlines(), out.splitlines(), lineterm="", n=0
        )
        if line[0] in "+-" and line[:3] not in ("+++", "---")
    ]
    assert len(diff) == 4
    assert str(mp.MAZE_LIMIT) in out
