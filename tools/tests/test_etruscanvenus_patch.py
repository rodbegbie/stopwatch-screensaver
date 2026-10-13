import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import etruscanvenus_patch as ep
import xs_vendor

ROOT = Path(__file__).resolve().parents[2]
UPSTREAM = xs_vendor.upstream(ROOT) / "hacks" / "glx" / "etruscanvenus.c"

SAMPLE = """\
/* Number of subdivisions of the surface */
#define NUMU 192
#define NUMV 128
static float a[NUMU];
"""


def test_the_grid_follows_the_wrappers_size():
    out = ep.size_the_grid(SAMPLE)
    assert "#define NUMU ETRUSCANVENUS_NUMU\n" in out
    assert "#define NUMV ETRUSCANVENUS_NUMV\n" in out
    assert "static float a[NUMU];" in out


@pytest.mark.parametrize("missing", ["#define NUMU 192\n", "#define NUMV 128\n"])
def test_it_refuses_a_source_that_has_changed(missing):
    with pytest.raises(ValueError, match="upstream"):
        ep.size_the_grid(SAMPLE.replace(missing, ""))


@pytest.mark.skipif(not UPSTREAM.exists(), reason=f"needs {xs_vendor.UPSTREAM}")
def test_upstream_has_both_defines_once():
    out = ep.size_the_grid(UPSTREAM.read_text())
    assert out.count("ETRUSCANVENUS_NUMU") == 1
    assert out.count("ETRUSCANVENUS_NUMV") == 1
