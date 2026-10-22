"""Build-time patch for FilmLeader: a square canvas of a size we choose.

Upstream draws the leader into a pixmap 712 pixels wide and 16:9, and analogtv
stretches it over the window. On the round 466 by 466 panel that squashes the
rings into upright ovals, and the pixmap (712 by 712, since it is sized for
the longer side) and the image `XGetImage` makes from it each frame are 1 MB
and 2 MB. The copied filmleader.c stays byte-identical: the firmware build runs
this on its text (firmware/patch_analogtv.py), and src/hacks/filmleader_tv.c
includes the result.

The aspect ratio becomes 1, and the width becomes `FILMLEADER_CANVAS`, which
the wrapper defines. The font sizes upstream asks for are in canvas pixels, so
the wrapper scales them with the canvas.

Each replacement is checked, so a change upstream fails the build loudly.
"""

OLD_ASPECT = "    double r = 16/9.0;\n"
NEW_ASPECT = "    double r = 1.0;\n"

OLD_WIDTH = "    st->w = 712;\n"
NEW_WIDTH = "    st->w = FILMLEADER_CANVAS;\n"


def _replace_once(source: str, old: str, new: str, what: str) -> str:
    found = source.count(old)
    if found != 1:
        raise ValueError(
            f"expected 1 {what} in filmleader.c, found {found}; has upstream changed?"
        )
    return source.replace(old, new)


def square_canvas(source: str) -> str:
    source = _replace_once(source, OLD_ASPECT, NEW_ASPECT, "16:9 aspect ratio")
    return _replace_once(source, OLD_WIDTH, NEW_WIDTH, "712 pixel canvas width")


# Upstream measures blurbs[i].s[0] with the length of blurbs[i].s[j], so a later
# line longer than the first ("PLAY DATE" after "REEL No.") reads a byte past the
# literal: AddressSanitizer stops on it. The result is never used (the lines that
# would read it are commented out), so the call measures the string it was given.
OLD_EXTENTS = """\
                                      blurbs[i].s[0], strlen(blurbs[i].s[j]),
"""
NEW_EXTENTS = """\
                                      blurbs[i].s[0], strlen(blurbs[i].s[0]),
"""


def measure_the_string_it_was_given(source: str) -> str:
    return _replace_once(
        source, OLD_EXTENTS, NEW_EXTENTS, "extents call on s[0] by the length of s[j]"
    )
