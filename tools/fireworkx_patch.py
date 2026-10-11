"""Makes Fireworkx's pixel-unit constants follow the size it is rendered at.

Fireworkx is tuned for a window about 1024 pixels wide: its sparks fly up to a
few hundred pixels, its flash light falls off as 1/(distance + 4) in pixels,
and every shell throws 500 sparks that the glow blur adds together. Rendered
at 152 pixels (a third of the board's screen, see firmware/src/hacks/
lowrez_put.h) a burst fills the whole picture, the sparks pile up past the
point where the blur's doubled output saturates, and the screen is white.

The copied fireworkx.c stays byte-identical to upstream. The firmware build
runs this on its text before the float literal pass (firmware/
patch_float_literals.py), and fireworkx_single.c defines the two macros:

    FIREWORKX_SIZE      the render width over 1024, a float
    FIREWORKX_PIXCOUNT  the sparks a shell throws, an int

Each replacement is checked, so a change upstream fails the build loudly
instead of quietly leaving a constant at its old size.
"""

# (text in upstream, text to use, how many times it appears)
REPLACEMENTS = [
    ("#define PIXCOUNT 500", "#define PIXCOUNT FIREWORKX_PIXCOUNT", 1),
    ("#define POWDER 5.0", "#define POWDER (5.0 * FIREWORKX_SIZE)", 1),
    ("#define FLASH_ZOOM 0.8", "#define FLASH_ZOOM (0.8 * FIREWORKX_SIZE)", 1),
    (
        "#define G_ACCELERATION 0.001",
        "#define G_ACCELERATION (0.001 * FIREWORKX_SIZE)",
        1,
    ),
    # The random nudges to a spark's velocity every step, in pixels a step.
    ("frand(0.1) - 0.05", "(frand(0.1) - 0.05) * FIREWORKX_SIZE", 2),
    ("frand(0.01) - 0.005", "(frand(0.01) - 0.005) * FIREWORKX_SIZE", 1),
    ("frand(0.005) - 0.0025", "(frand(0.005) - 0.0025) * FIREWORKX_SIZE", 1),
]


def scale_for_a_small_window(source: str) -> str:
    for old, new, expected in REPLACEMENTS:
        found = source.count(old)
        if found != expected:
            raise ValueError(
                f"expected {expected} of `{old}` in fireworkx.c, found {found}; "
                "has upstream changed?"
            )
        source = source.replace(old, new)
    return source
