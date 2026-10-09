"""Make pacman_level.c always use its fixed level.

`pacman_createnewlevel` builds half its levels with a random generator
(`creatlevelblock` and `nextstep`) and copies the fixed `stdlevel` for the
rest. The generator calls itself once per tile and keeps a 1.3 KB copy of the
level in every frame, so a level takes up to about 300 KB of stack. The board's
loop task has 16 KB, and Pacman reboots the board on its first frame when the
generator runs.

The copied pacman_level.c stays byte-identical to upstream. The firmware build
writes a patched copy with `firmware/patch_pacman.py` and compiles that
instead. The generator is still compiled but never called.
"""

RANDOM_LEVEL_TEST = "if (NRAND (2) == 0) {"
FIXED_LEVEL_COPY = "memcpy (level, stdlevel, sizeof (lev_t))"
REPLACEMENT = "if (0) { /* patched: the random generator needs ~300 KB of stack */"


def patch_pacman_level(source: str) -> str:
    found = source.count(RANDOM_LEVEL_TEST)
    if found != 1:
        raise ValueError(
            f"expected one `{RANDOM_LEVEL_TEST}` (the NRAND choice in "
            f"pacman_createnewlevel), found {found}; has upstream changed?"
        )
    if FIXED_LEVEL_COPY not in source:
        raise ValueError(
            f"`{FIXED_LEVEL_COPY}` (the stdlevel branch) is gone; has upstream changed?"
        )
    return source.replace(RANDOM_LEVEL_TEST, REPLACEMENT)
