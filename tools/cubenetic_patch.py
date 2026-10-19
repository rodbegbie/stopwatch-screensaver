"""Makes the size of Cubenetic's plasma texture a build setting.

Upstream works out a 256 by 256 water-wave texture again every frame: 65,536
pixels, each with a square root and a lookup per wave source, 847 ms a frame on
the board. firmware/src/hacks/cubenetic_single_gl.c defines
CUBENETIC_TEXTURE_SIZE, and the build (firmware/patch_float_literals.py) runs
this on the hack's text. The copied cubenetic.c stays byte-identical. The
replacement is checked, so a change upstream fails the build.
"""

OLD = "static int texture_size = 256;\n"
NEW = "static int texture_size = CUBENETIC_TEXTURE_SIZE;\n"


def size_the_texture(source: str) -> str:
    found = source.count(OLD)
    if found != 1:
        raise ValueError(
            f"expected 1 of `{OLD.strip()}` in cubenetic.c, found {found}; "
            "has upstream changed?"
        )
    return source.replace(OLD, NEW)
