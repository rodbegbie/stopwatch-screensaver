import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import binaryring_patch as bp  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
UPSTREAM = ROOT / "vendor" / "xscreensaver-6.16" / "hacks" / "binaryring.c"

SAMPLE = """\
#define frand1()    ((((int) random() ) << 1) * 4.65661287524579692411E-10)
static float x = 1.5;
"""


def test_the_random_scale_becomes_a_float():
    out = bp.float_random_scale(SAMPLE)
    assert "* 4.65661287524579692411E-10f)" in out
    assert "static float x = 1.5;" in out


def test_it_refuses_a_source_without_the_literal():
    with pytest.raises(ValueError, match="upstream"):
        bp.float_random_scale("#define frand1() (random() * 1e-9)\n")


def test_it_refuses_the_literal_twice():
    with pytest.raises(ValueError, match="upstream"):
        bp.float_random_scale(SAMPLE + SAMPLE)


@pytest.mark.skipif(not UPSTREAM.exists(), reason="needs vendor/xscreensaver-6.16")
def test_upstream_has_the_literal_once():
    out = bp.float_random_scale(UPSTREAM.read_text())
    assert out.count("E-10f)") == 1
