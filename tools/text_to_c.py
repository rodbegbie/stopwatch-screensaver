"""Writes a file's bytes as a C string, as xscreensaver's utils/bin2c does for
Graphstat's word list (graphstat.txt becomes graphstat.h). Ours, not
xscreensaver's: firmware/graphstat_strings.py runs it at build time on the
file in vendor/, so the list is never committed.

    uv run tools/text_to_c.py in.txt out.h
"""

import sys
from pathlib import Path

# Printable ASCII other than `"`, `?` (trigraphs) and `\` goes in as itself.
_PLAIN = {c for c in range(0x20, 0x7F)} - {ord('"'), ord("?"), ord("\\")}


def c_string(data: bytes, name: str) -> str:
    text = "".join(chr(b) if b in _PLAIN else f"\\{b:03o}" for b in data)
    return f'static const unsigned char {name}[] =\n "{text}";\n'


def main(argv: list[str]) -> int:
    if len(argv) != 2:
        print(__doc__)
        return 2
    src, dst = Path(argv[0]), Path(argv[1])
    name = dst.stem.replace("-", "_").replace(".", "_")
    dst.write_text(c_string(src.read_bytes(), name))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
