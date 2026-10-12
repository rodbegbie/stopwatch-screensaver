"""Shorten penrose.c's two pauses so a finished or failed tiling restarts soon.

penrose.c counts its pauses in frames, tuned for a fast X server:
`COMPLETION` (3141) after the tiles fill the screen and `CELEBRATE` (31415)
after an error it expects to recover from, which is about six minutes at the
board's 87 frames a second with the screen full and nothing moving. Both
become `PAUSE_FRAMES`, about ten seconds.

The copied penrose.c stays byte-identical to upstream. The firmware build
writes a patched copy with `firmware/patch_penrose.py` and compiles that.
"""

import re

PAUSE_FRAMES = 900
UPSTREAM = {"CELEBRATE": 31415, "COMPLETION": 3141}


def shorten_pauses(source: str, frames: int) -> str:
    for name, value in UPSTREAM.items():
        pattern = re.compile(
            rf"^#define[ \t]+{name}[ \t]+{value}\b[^\n]*$", re.MULTILINE
        )
        source, count = pattern.subn(f"#define {name} {frames}", source)
        if count != 1:
            raise ValueError(
                f"expected one `#define {name} {value}`, found {count}; "
                "has upstream penrose.c changed?"
            )
    return source
