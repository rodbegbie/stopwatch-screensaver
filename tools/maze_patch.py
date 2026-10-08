"""Shrink maze.c's fixed-size arrays so it fits in the ESP32-S3's memory.

maze.c keeps its maze and three move lists in one `struct state` sized by
`MAX_MAZE_SIZE_X/Y` (1000 each), about 20 MB. The board has 8 MB of PSRAM. On
a 466 px screen with 7 px cells (the smallest the hack picks) a maze is 66
cells across, so 80 leaves room and the struct drops to about 130 KB.

The copied maze.c stays byte-identical to upstream. The firmware build writes
a patched copy with `firmware/patch_maze.py` and compiles that instead.
"""

import re

MAZE_LIMIT = 80
UPSTREAM_LIMIT = 1000


def patch_maze(source: str, limit: int) -> str:
    for axis in ("X", "Y"):
        pattern = re.compile(
            rf"^#define[ \t]+MAX_MAZE_SIZE_{axis}[ \t]+{UPSTREAM_LIMIT}[ \t]*$",
            re.MULTILINE,
        )
        source, count = pattern.subn(f"#define MAX_MAZE_SIZE_{axis} {limit}", source)
        if count != 1:
            raise ValueError(
                f"expected one `#define MAX_MAZE_SIZE_{axis} {UPSTREAM_LIMIT}`, "
                f"found {count}; has upstream maze.c changed?"
            )
    return source
