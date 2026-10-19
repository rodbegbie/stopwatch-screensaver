"""Keeps GLMatrix's brightness wave index inside its table, and its texture
alpha shift defined.

Upstream indexes `brightness_ramp[WAVE_SIZE]` with
`WAVE_SIZE - ((i + (GRID_SIZE - wave_position)) % WAVE_SIZE)`, which runs from
1 to WAVE_SIZE: a zero remainder reads one entry past the table, into the next
screen's state. The index counts down through the wave and the entry after 1 is
0, so the patch wraps WAVE_SIZE to 0. The build (firmware/patch_float_literals.py)
runs this on the hack's text; the copied glmatrix.c stays byte-identical. The
replacement is checked, so a change upstream fails the build.
"""

import re

OLD = re.compile(
    r"int j = WAVE_SIZE - \(\(i \+ \(GRID_SIZE - s->wave_position\)\)\s*% WAVE_SIZE\);"
)
NEW = (
    "int j = (WAVE_SIZE - ((i + (GRID_SIZE - s->wave_position)) % WAVE_SIZE))"
    " % WAVE_SIZE;"
)


def keep_the_wave_index_in_range(source: str) -> str:
    found = len(OLD.findall(source))
    if found != 1:
        raise ValueError(
            f"expected 1 brightness wave index in glmatrix.c, found {found}; "
            "has upstream changed?"
        )
    return OLD.sub(NEW, source)


SHIFT_OLD = "(a << apos)"
SHIFT_NEW = "((unsigned long) a << apos)"


def shift_the_alpha_unsigned(source: str) -> str:
    """`a` is an unsigned char, promoted to a signed int, so `a << 24` for an
    alpha of 128 or more overflows into the sign bit: undefined behaviour (found
    by an -fsanitize=undefined sweep). The bits packed are the same."""
    found = source.count(SHIFT_OLD)
    if found != 1:
        raise ValueError(
            f"expected 1 of `{SHIFT_OLD}` in glmatrix.c, found {found}; "
            "has upstream changed?"
        )
    return source.replace(SHIFT_OLD, SHIFT_NEW)
