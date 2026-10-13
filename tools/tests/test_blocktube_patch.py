import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import blocktube_patch as bp
import xs_vendor

ROOT = Path(__file__).resolve().parents[2]
UPSTREAM = xs_vendor.upstream(ROOT) / "hacks" / "glx" / "blocktube.c"


def test_the_block_count_follows_the_wrapper():
    out = bp.count_the_blocks(
        "#define MAX_ENTITIES     1000\nentity e[MAX_ENTITIES];\n"
    )
    assert "#define MAX_ENTITIES BLOCKTUBE_ENTITIES\n" in out
    assert "entity e[MAX_ENTITIES];" in out


def test_it_refuses_a_source_that_has_changed():
    with pytest.raises(ValueError, match="upstream"):
        bp.count_the_blocks("#define MAX_ENTITIES 500\n")


@pytest.mark.skipif(not UPSTREAM.exists(), reason=f"needs {xs_vendor.UPSTREAM}")
def test_upstream_has_the_define_once():
    assert bp.count_the_blocks(UPSTREAM.read_text()).count("BLOCKTUBE_ENTITIES") == 1
