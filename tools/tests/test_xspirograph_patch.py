import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import xs_vendor
import xspirograph_patch as xp

ROOT = Path(__file__).resolve().parents[2]
UPSTREAM = xs_vendor.upstream(ROOT) / "hacks" / "xspirograph.c"

SAMPLE = """\
static Bool
go (struct state *st, int radius1, int radius2, int d)
{
  return cos(1.0);
}

#define min(a,b) ((a)<(b)?(a):(b))
"""


def test_go_is_replaced_and_the_rest_kept():
    out = xp.reduce_the_angles(SAMPLE)
    assert "% 360" in out
    assert "return cos(1.0);" not in out
    assert "#define min(a,b)" in out


def test_both_angles_are_reduced_before_the_multiply():
    out = xp.reduce_the_angles(SAMPLE)
    assert "spin = st->theta % 360;" in out
    assert "roll = (((radius1 * st->theta) - delta) / radius2) % 360;" in out


def test_it_refuses_a_source_without_go():
    with pytest.raises(ValueError, match="upstream"):
        xp.reduce_the_angles("int x;\n")


def test_it_refuses_go_twice():
    with pytest.raises(ValueError, match="upstream"):
        xp.reduce_the_angles(SAMPLE + SAMPLE)


@pytest.mark.skipif(not UPSTREAM.exists(), reason=f"needs {xs_vendor.UPSTREAM}")
def test_upstream_has_go_once_and_keeps_its_neighbours():
    text = UPSTREAM.read_text()
    out = xp.reduce_the_angles(text)
    assert out.count("roll = ") == 1
    assert "static void\npick_new" in out
    assert "#define min(a,b)" in out
