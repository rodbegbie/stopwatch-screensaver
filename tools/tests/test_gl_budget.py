import shutil
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import gl_budget as gb  # noqa: E402
import tinygl_patch  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
needs_cc = pytest.mark.skipif(shutil.which("cc") is None, reason="no C compiler")


def patched_tree():
    return tinygl_patch.patch_tree(gb.read_tinygl(ROOT))


def test_instrument_adds_a_counter_at_each_site():
    tree = gb.load_tinygl(ROOT)
    assert "gl_budget_counts[0]" in tree["src/vertex.c"]
    assert "gl_budget_counts[1]" in tree["src/vertex.c"]
    for index in (2, 3, 4, 5):
        assert f"gl_budget_counts[{index}]" in tree["src/clip.c"]
    assert "extern double gl_budget_counts" in tree["src/zgl.h"]


def test_instrument_fails_when_an_anchor_is_missing():
    tree = patched_tree()
    tree["src/clip.c"] = ""
    with pytest.raises(ValueError, match="has upstream changed"):
        gb.instrument(tree)


def test_instrument_leaves_its_argument_alone():
    tree = patched_tree()
    before = dict(tree)
    gb.instrument(tree)
    assert tree == before


def test_predict_with_nothing_drawn_is_the_fixed_cost():
    nothing = gb.FrameCounts(0, 0, 0, 0, 0, 0)
    assert gb.predict_ms(nothing) == gb.MODEL["clear_ms"] + gb.MODEL["push_ms"]


def test_each_term_adds_its_own_cost():
    model = {
        "clear_ms": 0,
        "push_ms": 0,
        "vertex_us": 4,
        "lit_vertex_us": 2,
        "pixel_ns": 10,
    }
    counts = gb.FrameCounts(
        vertices=1000, lit_vertices=500, triangles=0, lines=0, points=0, pixels=100000
    )
    assert gb.predict_ms(counts, model) == pytest.approx(6.0)


def test_fps_is_the_inverse_of_milliseconds():
    assert gb.fps(125.0) == 8.0
