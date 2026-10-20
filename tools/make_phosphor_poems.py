"""Turn firmware/assets/phosphor-poems.txt into the C header Phosphor's text
source reads.

Each poem becomes one string of terminal output: clear the screen, type a
`cat` command, then the poem word-wrapped to fit the part of the round panel
that shows (see the table below). A poem that does not fit is paged: the page
ends with `--More--` and a pause byte, and the next page clears the screen.
Phosphor leaves a ghost of every character a scroll moves, which reads as
noise under the new text, so nothing scrolls.

Run with: uv run tools/make_phosphor_poems.py
"""

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / "firmware" / "assets" / "phosphor-poems.txt"
OUTPUT = ROOT / "firmware" / "src" / "hacks" / "phosphor_poems.h"

# At scale 3 the grid is 25 by 15 cells, and the circle shows columns 4 to 21
# on rows 2 to 13 (rows 0, 1 and 14 are mostly off the glass).
INDENT = 4
WIDTH = 18
HANGING = 2
FIRST_ROW = 2
LAST_ROW = 13
# The command takes row 2 and a gap row 3, so a page has rows 4 to 13: ten.
PAGE_ROWS = LAST_ROW - (FIRST_ROW + 2) + 1
MORE = "--More--"
# The last page ends with a blank row, the byline and a prompt row.
TAIL_ROWS = 3
# The byte the text source waits on; the terminal never sees it.
PAUSE = "\x01"

CLEAR = "\x1b[2J\x1b[H"


def wrap(line: str) -> list[str]:
    """Greedy word wrap: the first piece WIDTH wide, later ones HANGING
    narrower and indented by it. A word longer than the room is an error."""
    pieces: list[str] = []
    current = ""

    def room() -> int:
        return WIDTH if not pieces else WIDTH - HANGING

    for word in line.split():
        if current and len(current) + 1 + len(word) <= room():
            current += " " + word
            continue
        if current:
            pieces.append(current)
        if len(word) > room():
            raise ValueError(f"{word!r} is wider than {room()} columns")
        current = word
    pieces.append(current)
    return [pieces[0]] + [" " * HANGING + p for p in pieces[1:]]


def parse(text: str) -> list[tuple[str, str, list[str]]]:
    """(file name, byline, lines) for each poem; a blank line is kept."""
    poems: list[tuple[str, str, list[str]]] = []
    for raw in text.splitlines():
        if raw.startswith("#"):
            continue
        if raw.startswith("== "):
            name, _, byline = raw[3:].partition("|")
            poems.append((name.strip(), byline.strip(), []))
        elif poems:
            poems[-1][2].append(raw.rstrip())
        elif raw.strip():
            raise ValueError(f"text before the first poem: {raw!r}")
    for name, byline, lines in poems:
        while lines and not lines[-1]:
            lines.pop()
        if not name or not byline or not lines:
            raise ValueError(f"poem {name!r} needs a name, a byline and lines")
        for line in [name, byline, *lines]:
            if not line.isascii():
                raise ValueError(
                    f"{line!r} is not ASCII: the font has no glyphs for it"
                )
    return poems


def paginate(groups: list[list[str]]) -> list[list[str]]:
    """Rows of each page. A group (one verse line, wrapped) is never split. A
    page that is not the last keeps a row for --More--, and the last keeps rows
    for a blank, the byline and the prompt. Blank rows at a page's edges go."""
    final_room = PAGE_ROWS - TAIL_ROWS
    pages: list[list[str]] = []
    i = 0
    while True:
        while i < len(groups) and groups[i] == [""]:
            i += 1
        rest = groups[i:]
        while rest and rest[-1] == [""]:
            rest.pop()
        if sum(len(g) for g in rest) <= final_room:
            pages.append([row for g in rest for row in g])
            return pages
        page: list[str] = []
        last = i + len(rest) - 1  # the last page keeps at least the last group
        while i < last and len(page) + len(groups[i]) <= PAGE_ROWS - 1:
            page += groups[i]
            i += 1
        if not page:
            raise ValueError(f"{groups[i]!r} does not fit on a page")
        while page and page[-1] == "":
            page.pop()
        pages.append(page)


def render(name: str, byline: str, lines: list[str]) -> str:
    """The terminal output for one poem."""
    groups = [wrap(line) if line else [""] for line in lines]
    pages = paginate(groups)
    out = []
    for number, rows in enumerate(pages):
        out.append(CLEAR + "\r\n" * FIRST_ROW)
        out.append(" " * INDENT + f"> cat {name}\r\n\r\n")
        if number + 1 < len(pages):
            rows = [*rows, MORE]
        else:
            rows = [*rows, "", f"-- {byline}", "> _"]
        for i, row in enumerate(rows):
            out.append(" " * INDENT + row if row else "")
            if i + 1 < len(rows):
                out.append("\r\n")
        if number + 1 < len(pages):
            out.append(PAUSE)
    return "".join(out)


def c_string(text: str) -> str:
    """A C string literal, split after each newline so the lines stay short."""
    chunks = text.split("\n")
    pieces = []
    for i, chunk in enumerate(chunks):
        piece = chunk + ("\n" if i + 1 < len(chunks) else "")
        if not piece:
            continue
        escaped = (
            piece.replace("\\", "\\\\")
            .replace('"', '\\"')
            .replace("\x1b", "\\033")
            .replace("\r", "\\r")
            .replace("\n", "\\n")
        )
        pieces.append(f'    "{escaped}"')
    return "\n".join(pieces)


def header(text: str) -> str:
    poems = parse(text)
    body = []
    for name, byline, lines in poems:
        body.append(f"    /* {name} */\n{c_string(render(name, byline, lines))},")
    entries = "\n".join(body)
    return (
        "/* Generated by tools/make_phosphor_poems.py from "
        "firmware/assets/phosphor-poems.txt; do not edit. */\n"
        "#ifndef PHOSPHOR_POEMS_H\n"
        "#define PHOSPHOR_POEMS_H\n\n"
        "static const char *const kPoems[] = {\n"
        f"{entries}\n"
        "};\n\n"
        "#define POEM_COUNT ((int)(sizeof kPoems / sizeof kPoems[0]))\n\n"
        "#endif\n"
    )


def main(argv: list[str]) -> int:
    text = header(SOURCE.read_text())
    if not OUTPUT.exists() or OUTPUT.read_text() != text:
        OUTPUT.write_text(text)
    print(f"{OUTPUT.relative_to(ROOT)}: {len(parse(SOURCE.read_text()))} poems")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
