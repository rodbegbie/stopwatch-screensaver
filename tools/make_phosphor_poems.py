"""Turn firmware/assets/phosphor-poems.txt into the C header Phosphor's text
source reads.

Each poem becomes one string of terminal output: clear the screen, type a
`cat` command, then the poem word-wrapped to fit the part of the round panel
that shows (see the table below). Two layouts are made, one header each:

- paged (`phosphor_poems.h`): a poem that does not fit is paged. A page ends
  with `--More--` and a pause byte, and the next page clears the screen.
  Phosphor leaves a ghost of every character a scroll moves, which reads as
  noise under the new text, so nothing scrolls.
- scrolling (`phosphor_poems_scroll.h`, `-DPHOSPHOR_SCROLL`): the poem types
  into a scroll region over the rows the circle shows, and scrolls there.

Run with: uv run tools/make_phosphor_poems.py
"""

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / "firmware" / "assets" / "phosphor-poems.txt"
OUTPUT = ROOT / "firmware" / "src" / "hacks" / "phosphor_poems.h"
OUTPUT_SCROLL = ROOT / "firmware" / "src" / "hacks" / "phosphor_poems_scroll.h"

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
# The last page ends with a blank row, the byline (which may wrap) and a prompt.
# The byte the text source waits on; the terminal never sees it.
PAUSE = "\x01"

CLEAR = "\x1b[r\x1b[2J\x1b[H"


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


def byline_rows(byline: str) -> list[str]:
    return wrap(f"-- {byline}")


def paginate(groups: list[list[str]], tail_rows: int) -> list[list[str]]:
    """Rows of each page. A group (one verse line, wrapped) is never split. A
    page that is not the last keeps a row for --More--, and the last keeps
    tail_rows for a blank, the byline and the prompt. Blank rows at a page's
    edges go."""
    final_room = PAGE_ROWS - tail_rows
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


def indent(rows: list[str]) -> str:
    return "\r\n".join(" " * INDENT + row if row else "" for row in rows)


def render(name: str, byline: str, lines: list[str], paged: bool = True) -> str:
    """The terminal output for one poem."""
    groups = [wrap(line) if line else [""] for line in lines]
    tail = ["", *byline_rows(byline), "> _"]
    command = CLEAR + "\r\n" * FIRST_ROW + " " * INDENT + f"> cat {name}\r\n"
    if not paged:
        # Rows 2 to 13 of the circle, 1-based: a region from row 5 to row 14.
        region = f"\x1b[{FIRST_ROW + 3};{LAST_ROW + 1}r"
        body = [row for group in groups for row in group]
        return command + region + indent([*body, *tail])
    pages = paginate(groups, len(tail))
    out = []
    for number, rows in enumerate(pages):
        last_page = number + 1 == len(pages)
        rows = [*rows, *tail] if last_page else [*rows, MORE]
        out.append(command + "\r\n" + indent(rows) + ("" if last_page else PAUSE))
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


def header(text: str, paged: bool = True) -> str:
    poems = parse(text)
    body = []
    for name, byline, lines in poems:
        shown = c_string(render(name, byline, lines, paged))
        body.append(f"    /* {name} */\n{shown},")
    entries = "\n".join(body)
    guard = "PHOSPHOR_POEMS_H" if paged else "PHOSPHOR_POEMS_SCROLL_H"
    return (
        "/* Generated by tools/make_phosphor_poems.py from "
        "firmware/assets/phosphor-poems.txt; do not edit. */\n"
        f"#ifndef {guard}\n"
        f"#define {guard}\n\n"
        "static const char *const kPoems[] = {\n"
        f"{entries}\n"
        "};\n\n"
        "#define POEM_COUNT ((int)(sizeof kPoems / sizeof kPoems[0]))\n\n"
        "#endif\n"
    )


def main(argv: list[str]) -> int:
    source = SOURCE.read_text()
    for path, paged in ((OUTPUT, True), (OUTPUT_SCROLL, False)):
        text = header(source, paged)
        if not path.exists() or path.read_text() != text:
            path.write_text(text)
        print(f"{path.relative_to(ROOT)}: {len(parse(source))} poems")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
