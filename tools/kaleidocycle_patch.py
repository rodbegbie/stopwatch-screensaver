"""Keeps Kaleidocycle's first frame from dividing zero by zero.

The hack starts with `bp->count = 0` and the piece loop runs once at that
count, so its rotation `(i/2) * 4 * 180 / bp->count` is 0 / 0: a NaN angle that
reaches TinyGL's viewport conversion and is cast to an int (undefined; found by
the trail review with -fsanitize=float-cast-overflow). The build
(firmware/patch_float_literals.py) runs this on the hack's text; the copied
kaleidocycle.c stays byte-identical. The replacement is checked, so a change
upstream fails the build.
"""

OLD = "glRotatef ((i/2) * 4 * 180 / bp->count, 0, 0, 1);"
NEW = "glRotatef ((i/2) * 4 * 180 / (bp->count > 0 ? bp->count : 1), 0, 0, 1);"


def guard_the_zero_count(source: str) -> str:
    found = source.count(OLD)
    if found != 1:
        raise ValueError(
            f"expected 1 of `{OLD}` in kaleidocycle.c, found {found}; "
            "has upstream changed?"
        )
    return source.replace(OLD, NEW)
