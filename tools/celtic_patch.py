"""Makes Celtic's spline lookup safe in single precision.

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
