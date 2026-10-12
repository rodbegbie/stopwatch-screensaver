import difflib
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import penrose_patch as pp

ROOT = Path(__file__).resolve().parents[2]
PENROSE_C = ROOT / "firmware" / "src" / "hacks" / "penrose" / "penrose.c"

SAMPLE = """#define CELEBRATE 31415\t\t/* This causes a pause, an error occurred. */
#define COMPLETION 3141\t\t/* This causes a pause, tiles filled up screen. */

#define MAX_TILES_PER_VERTEX 7
"""


def test_both_pauses_are_replaced():
    out = pp.shorten_pauses(SAMPLE, 900)
    assert "#define CELEBRATE 900\n" in out
    assert "#define COMPLETION 900\n" in out
    assert "31415" not in out
    assert "3141" not in out


def test_nothing_else_changes():
    out = pp.shorten_pauses(SAMPLE, 900)
    changed = [
        line
        for line in difflib.unified_diff(
            SAMPLE.splitlines(), out.splitlines(), lineterm="", n=0
        )
        if line[0] in "+-" and line[:3] not in ("+++", "---")
    ]
    assert len(changed) == 4
    assert "MAX_TILES_PER_VERTEX 7" in out


def test_missing_define_is_an_error():
    with pytest.raises(ValueError, match="COMPLETION"):
        pp.shorten_pauses(SAMPLE.replace("COMPLETION", "FINISHED"), 900)


def test_unexpected_upstream_value_is_an_error():
    with pytest.raises(ValueError, match="CELEBRATE"):
        pp.shorten_pauses(SAMPLE.replace("31415", "999"), 900)


def test_a_repeated_define_is_an_error():
    with pytest.raises(ValueError, match="CELEBRATE"):
        pp.shorten_pauses(SAMPLE + "#define CELEBRATE 31415\n", 900)


@pytest.mark.skipif(not PENROSE_C.exists(), reason="penrose.c not copied yet")
def test_real_penrose_c_changes_exactly_two_lines():
    original = PENROSE_C.read_text()
    out = pp.shorten_pauses(original, pp.PAUSE_FRAMES)
    diff = [
        line
        for line in difflib.unified_diff(
            original.splitlines(), out.splitlines(), lineterm="", n=0
        )
        if line[0] in "+-" and line[:3] not in ("+++", "---")
    ]
    assert len(diff) == 4
    assert str(pp.PAUSE_FRAMES) in out
