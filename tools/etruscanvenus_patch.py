"""Makes the size of Etruscan Venus's surface grid a build setting.

Upstream draws the Klein bottle on a 192 by 128 grid and works out every point
and normal again each frame: about 25,000 points with ten sines and cosines
each, and 49,000 lit vertices. The board cannot do that at a useful rate, so
firmware/src/hacks/etruscanvenus_single_gl.c defines ETRUSCANVENUS_NUMU and
ETRUSCANVENUS_NUMV, and the build (firmware/patch_float_literals.py) runs this
on the hack's text. The copied etruscanvenus.c stays byte-identical. Each
replacement is checked, so a change upstream fails the build.
"""

REPLACEMENTS = [
    ("#define NUMU 192\n", "#define NUMU ETRUSCANVENUS_NUMU\n"),
    ("#define NUMV 128\n", "#define NUMV ETRUSCANVENUS_NUMV\n"),
]


def size_the_grid(source: str) -> str:
    for old, new in REPLACEMENTS:
        found = source.count(old)
        if found != 1:
            raise ValueError(
                f"expected 1 of `{old.strip()}` in etruscanvenus.c, found {found}; "
                "has upstream changed?"
            )
        source = source.replace(old, new)
    return source
