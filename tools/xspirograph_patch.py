"""Keeps XSpirograph's trig arguments small enough for single precision.

`go()` takes sin and cos of a whole number of degrees times pi over 180. The
second argument is `(radius1 * theta - delta) / radius2` degrees, which reaches
millions, so in a float build the angle has lost its fraction before sin sees
it and the picture turns to noise. Degrees repeat every 360, so this reduces
them in integers first. The copied xspirograph.c stays byte-identical;
firmware/patch_float_literals.py runs this on its text, and the double maths
that `go` did four times a line (1000 lines a frame) becomes float.
"""

import re

GO = re.compile(
    r"static Bool\ngo \(struct state \*st, int radius1, int radius2, int d\)\n\{.*?\n\}\n",
    re.DOTALL,
)

REPLACEMENT = """\
static Bool
go (struct state *st, int radius1, int radius2, int d)
{
  int width, height;
  int xmid, ymid;
  float tmpx, tmpy;
  int delta;
  int spin, roll;

  width  = st->xgwa.width;
  height = st->xgwa.height;
  delta = 1;
  xmid = width / 2;
  ymid = height / 2;

  if (st->theta == 1) {
    st->x1 = xmid + radius1 - radius2 + d;
    st->y1 = ymid;
  }

  spin = st->theta % 360;
  roll = (((radius1 * st->theta) - delta) / radius2) % 360;
  tmpx = xmid + ((radius1 - radius2) * cos(spin * M_PI / 180))
              + (d * cos(roll * M_PI / 180));
  tmpy = ymid + ((radius1 - radius2) * sin(spin * M_PI / 180))
              + (d * sin(roll * M_PI / 180));

  st->x2 = tmpx;
  st->y2 = tmpy;

  if(st->theta == 1)
  {
    st->firstx = tmpx;
    st->firsty = tmpy;
  }

  if (st->theta != 1)
    XDrawLine (st->dpy, st->window, st->draw_gc,
               st->x1, st->y1, st->x2, st->y2);

  st->x1 = st->x2;
  st->y1 = st->y2;

  if(tmpx == st->firstx && tmpy == st->firsty && st->theta != 1) {
    st->firstx = st->firsty = 0;
    st->theta = 1;
    return True;
  }

  if(!st->always_finish_p && st->theta > ( 360 * 100 ) ) {
    st->firstx = st->firsty = 0;
    st->theta = 1;
    return True;
  }

  st->theta++;

  return False;
}
"""


def reduce_the_angles(source: str) -> str:
    found = len(GO.findall(source))
    if found != 1:
        raise ValueError(
            f"expected 1 `go` function in xspirograph.c, found {found}; has upstream changed?"
        )
    return GO.sub(lambda _: REPLACEMENT, source)
