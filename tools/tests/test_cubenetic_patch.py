import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import cubenetic_patch as cp
import xs_vendor

ROOT = Path(__file__).resolve().parents[2]
UPSTREAM = xs_vendor.upstream(ROOT) / "hacks" / "glx" / "cubenetic.c"

SAMPLE = """\
static int wave_radius;
static int texture_size = 256;
static XrmOptionDescRec opts[] = {
"""


def test_the_texture_follows_the_wrappers_size():
    out = cp.size_the_texture(SAMPLE)
    assert "static int texture_size = CUBENETIC_TEXTURE_SIZE;\n" in out
    assert "static int wave_radius;" in out


def test_it_refuses_a_source_that_has_changed():
    with pytest.raises(ValueError, match="upstream"):
        cp.size_the_texture(SAMPLE.replace("= 256", "= 128"))


@pytest.mark.skipif(not UPSTREAM.exists(), reason=f"needs {xs_vendor.UPSTREAM}")
def test_upstream_has_the_size_once():
    out = cp.size_the_texture(UPSTREAM.read_text())
    assert out.count("CUBENETIC_TEXTURE_SIZE") == 1
