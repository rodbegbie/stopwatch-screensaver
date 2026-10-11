import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import celtic_patch as cp  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
CELTIC = ROOT / "firmware" / "src" / "hacks" / "celtic" / "celtic.c"

SAMPLE = """\
static void spline_value_at(Spline s, double *x, double *y, double t, int *segment)
{
  int si;
  double tt;
  SplineSegment ss;
  si = floor(t*s->segments->nb_elements);
  tt = t*s->segments->nb_elements - si;
  assert(tt>=0 && tt<1);
  ss=s->segments->elements[si];

  *x = ss->x1*(1-tt);
}
"""


def test_the_position_is_worked_out_once_so_a_fused_multiply_cannot_disagree():
    out = cp.compute_spline_position_once(SAMPLE)
    assert out.count("t*s->segments->nb_elements") == 1
    assert "si = floor(pos);" in out
    assert "tt = pos - si;" in out


def test_the_index_and_the_fraction_are_clamped_instead_of_aborting():
    out = cp.compute_spline_position_once(SAMPLE)
    assert "assert(tt>=0 && tt<1);" not in out
    assert "if (si < 0) si = 0;" in out
    assert (
        "if (si >= s->segments->nb_elements) si = s->segments->nb_elements - 1;" in out
    )
    assert "if (tt < 0) tt = 0;" in out
    assert "if (tt >= 1) tt = 0.999999;" in out


def test_the_rest_of_the_function_is_untouched():
    out = cp.compute_spline_position_once(SAMPLE)
    assert out.startswith("static void spline_value_at(Spline s, double *x")
    assert "  ss=s->segments->elements[si];\n" in out
    assert "*x = ss->x1*(1-tt);" in out


def test_a_change_upstream_fails_loudly():
    with pytest.raises(ValueError, match="has upstream changed"):
        cp.compute_spline_position_once(SAMPLE.replace("floor(", "ceil("))


def test_a_source_without_the_lines_fails_loudly():
    with pytest.raises(ValueError, match="has upstream changed"):
        cp.compute_spline_position_once("int main(void) { return 0; }\n")


def test_the_copied_celtic_source_patches_cleanly():
    out = cp.compute_spline_position_once(CELTIC.read_text())
    assert "assert(tt>=0 && tt<1);" not in out
    assert out.count("si = floor(pos);") == 1
