"""Build-time patches for Celtic: a safe spline lookup in single precision, and
a graph sized to the screen. The first function's notes come first.

Makes Celtic's spline lookup safe in single precision.

spline_value_at picks a segment with `si = floor(t*n)` and then the position in
it with `tt = t*n - si`, and aborts unless 0 <= tt < 1. Computed in double
precision that holds. In float the compiler fuses the second multiply and the
subtraction (the S3's `madd.s`, and arm64 on the host), so `tt` uses the exact
product while `si` came from the product rounded to a float: t = 0.331093 with
3422 segments rounds to exactly 1133 and gives tt = -1.4e-5, and the abort
stopped the board.

The copied celtic.c stays byte-identical to upstream. The firmware build runs
this on its text before the float literal pass (firmware/patch_float_literals.py).
The product is worked out once, and the index and fraction are clamped, since
t just under 1 can round up to the segment count.

The replacement is checked, so a change upstream fails the build loudly.
"""

OLD = """\
  si = floor(t*s->segments->nb_elements);
  tt = t*s->segments->nb_elements - si;
  assert(tt>=0 && tt<1);
"""

NEW = """\
  double pos = t*s->segments->nb_elements;
  si = floor(pos);
  if (si < 0) si = 0;
  if (si >= s->segments->nb_elements) si = s->segments->nb_elements - 1;
  tt = pos - si;
  if (tt < 0) tt = 0;
  if (tt >= 1) tt = 0.999999;
"""


def compute_spline_position_once(source: str) -> str:
    found = source.count(OLD)
    if found != 1:
        raise ValueError(
            f"expected 1 spline_value_at lookup in celtic.c, found {found}; "
            "has upstream changed?"
        )
    return source.replace(OLD, NEW)


# Upstream picks each graph's margin, the border between the screen and the
# graph, from -600..100 (grid, Kennicott) and -900..0 (triangle). The graph
# covers the screen plus twice the margin, so most of a negative margin is
# nodes, edges and spline segments that are never seen, on a heap that is
# about 150 bytes an edge: up to 5,700 edges, 900 KB, against the board's
# 312 KB. The grid and Kennicott graphs tile a square of side 466 - 2 * margin
# centred on the screen, so a margin of minus one edge reaches a cell past the
# screen at any rotation. The triangle's incircle is half its circumradius
# (width / 2), so covering the screen's circle takes width / 2 + 2 * edge.
# Polar graphs are small and are left alone.
#
# Each entry is (upstream text, new text). The random() calls stay, so only
# the margin differs from the unpatched picture sequence.
MARGIN_REPLACEMENTS = [
    (
        "      st->params.edge_size=10*(random()%5)+50;\n",
        "      st->params.edge_size=10*(random()%5)+50;\n"
        "      st->params.margin=-(long)st->params.edge_size;\n",
    ),
    (
        "      st->params.cluster_size=st->params.edge_size/(3.0+random()%10)-1;\n",
        "      st->params.cluster_size=st->params.edge_size/(3.0+random()%10)-1;\n"
        "      st->params.margin=-(long)st->params.edge_size;\n",
    ),
    (
        "      st->params.margin=(random()%10)*100-900;\n",
        "      (void)random();\n"
        "      st->params.margin=-(long)(st->xgwa.width/2+2*st->params.edge_size);\n",
    ),
]


# Every animation tick advances each spline's parameter t by `step`, 100 ticks
# a draw call, and the shadow look-ahead walks t by `step` until the point is a
# shadow's width away. At 0.0001 a pattern takes 100 draw calls and a draw call
# is 25,000 to 58,000 Bezier evaluations. A larger step cuts the evaluations in
# a draw call by the same factor and finishes the pattern in that many times
# fewer calls, with each line segment that much longer (about a pixel). The
# wrapper defines CELTIC_STEP_SCALE, and -DCELTIC_STEP_SCALE=<n> overrides it.
STEP_OLD = "  double step=0.0001;"
STEP_NEW = "  double step=0.0001*CELTIC_STEP_SCALE;"


def scale_the_animation_step(source: str) -> str:
    found = source.count(STEP_OLD)
    if found != 1:
        raise ValueError(
            f"expected 1 `{STEP_OLD.strip()}` in celtic.c, found {found}; "
            "has upstream changed?"
        )
    return source.replace(STEP_OLD, STEP_NEW)


def fit_the_graph_to_the_screen(source: str) -> str:
    for old, new in MARGIN_REPLACEMENTS:
        found = source.count(old)
        if found != 1:
            raise ValueError(
                f"expected 1 of `{old.strip()}` in celtic.c, found {found}; "
                "has upstream changed?"
            )
        source = source.replace(old, new)
    return source
