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


PARAMS = """\
    switch (random()%4) {
    case 0:
      st->params.type=tgrid;
      st->params.shape1=(random()%1*2-1.0)*(random()%10+3)/10.0;
      st->params.edge_size=10*(random()%5)+50;
      break;
    case 1:
      st->params.type=kennicott;
      st->params.edge_size=10*(random()%3)+70;
      st->params.cluster_size=st->params.edge_size/(3.0+random()%10)-1;
      break;
    case 2:
      st->params.type=triangle;
      st->params.edge_size=10*(random()%5)+60;
      st->params.margin=(random()%10)*100-900;
      break;
    case 3:
      st->params.type=polar;
      st->params.nb_orbits=2+random()%10;
      break;
    }
"""

MARGIN = "st->params.margin=-(long)st->params.edge_size;"


def test_a_grid_and_a_kennicott_graph_reach_one_cell_past_the_screen():
    out = cp.fit_the_graph_to_the_screen(PARAMS)
    assert out.count(MARGIN) == 2
    grid = out.split("case 1:")[0]
    assert MARGIN in grid
    kennicott = out.split("case 1:")[1].split("case 2:")[0]
    assert MARGIN in kennicott


def test_a_triangle_is_big_enough_for_its_incircle_to_cover_the_screen():
    out = cp.fit_the_graph_to_the_screen(PARAMS)
    triangle = out.split("case 2:")[1].split("case 3:")[0]
    assert "(random()%10)*100-900" not in triangle
    assert (
        "st->params.margin=-(long)(st->xgwa.width/2+2*st->params.edge_size);"
        in triangle
    )


def test_the_random_stream_is_not_shifted_by_the_new_margins():
    out = cp.fit_the_graph_to_the_screen(PARAMS)
    assert out.count("random()") == PARAMS.count("random()")


def test_the_polar_graph_is_left_alone():
    out = cp.fit_the_graph_to_the_screen(PARAMS)
    assert out.split("case 3:")[1] == PARAMS.split("case 3:")[1]


def test_a_changed_parameter_block_fails_loudly():
    with pytest.raises(ValueError, match="has upstream changed"):
        cp.fit_the_graph_to_the_screen(PARAMS.replace("+50;", "+55;"))
    with pytest.raises(ValueError, match="has upstream changed"):
        cp.fit_the_graph_to_the_screen(PARAMS.replace("*100-900", "*100-800"))


def test_the_copied_celtic_source_fits_the_screen_cleanly():
    out = cp.fit_the_graph_to_the_screen(CELTIC.read_text())
    assert out.count(MARGIN) == 2
    assert "(random()%10)*100-900" not in out


ANIMATE = """\
static void pattern_animate(struct state *st)
{
  int ticks;
  double step=0.0001; /* TODO: set the step (or the delay) as a
                        * function of the spline length, so that
                        * drawing speed is constant
                        */
  Spline s;
}
"""


def test_the_animation_step_is_scaled_by_a_macro_the_build_can_set():
    out = cp.scale_the_animation_step(ANIMATE)
    assert "double step=0.0001*CELTIC_STEP_SCALE;" in out
    assert "double step=0.0001;" not in out


def test_the_rest_of_the_animation_function_is_untouched():
    out = cp.scale_the_animation_step(ANIMATE)
    assert out.startswith(
        "static void pattern_animate(struct state *st)\n{\n  int ticks;"
    )
    assert "  Spline s;\n}" in out
    assert "drawing speed is constant" in out


def test_a_changed_step_fails_loudly():
    with pytest.raises(ValueError, match="has upstream changed"):
        cp.scale_the_animation_step(ANIMATE.replace("0.0001", "0.0002"))
    with pytest.raises(ValueError, match="has upstream changed"):
        cp.scale_the_animation_step(ANIMATE + ANIMATE)


def test_the_copied_celtic_source_scales_its_step_cleanly():
    out = cp.scale_the_animation_step(CELTIC.read_text())
    assert out.count("double step=0.0001*CELTIC_STEP_SCALE;") == 1
