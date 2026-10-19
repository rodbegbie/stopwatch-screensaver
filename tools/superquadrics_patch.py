"""Keeps Superquadrics's random index below its range.

Upstream's `myrand` multiplies in `float`, so the largest value `LRAND()` can
return (2147483647) rounds up to 2^31 and `myrand(4)` is 4, which then indexes
`pats[4][t]` in a 4 by 4 array. The build (firmware/patch_float_literals.py)
runs this on the hack's text; the copied superquadrics.c stays byte-identical.
The replacement is checked, so a change upstream fails the build.
"""

OLD = "\treturn ((int) (((float) range) * LRAND() / (MAXRAND)));\n"
NEW = (
    "\tint r = (int) (((float) range) * LRAND() / (MAXRAND));\n"
    "\treturn r < range ? r : range - 1;\n"
)


def keep_myrand_in_range(source: str) -> str:
    found = source.count(OLD)
    if found != 1:
        raise ValueError(
            f"expected 1 of `{OLD.strip()}` in superquadrics.c, found {found}; "
            "has upstream changed?"
        )
    return source.replace(OLD, NEW)
