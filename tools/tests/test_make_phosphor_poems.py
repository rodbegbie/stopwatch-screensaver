import re
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import make_phosphor_poems as mpp


def test_a_short_line_is_one_piece():
    assert mpp.wrap("so much depends") == ["so much depends"]


def test_a_long_line_wraps_at_words_and_hangs_the_rest():
    pieces = mpp.wrap("Once upon a midnight dreary, while I pondered")
    assert pieces == ["Once upon a", "  midnight dreary,", "  while I pondered"]
    assert all(len(p) <= mpp.WIDTH for p in pieces)


def test_the_first_piece_may_use_the_full_width_and_later_ones_two_less():
    first = "a" * mpp.WIDTH
    assert mpp.wrap(first) == [first]
    hanging = "x" * (mpp.WIDTH - mpp.HANGING)
    assert mpp.wrap(f"{first} {hanging}") == [first, "  " + hanging]


def test_a_word_wider_than_the_room_is_an_error():
    with pytest.raises(ValueError, match="wider"):
        mpp.wrap("a" * (mpp.WIDTH + 1))
    with pytest.raises(ValueError, match="wider"):
        mpp.wrap("ok " + "a" * (mpp.WIDTH - mpp.HANGING + 1))


def test_parse_reads_names_bylines_and_stanza_breaks():
    text = "# note\n== a.txt | Poe, 1845\none\n\ntwo\n\n== b.txt | Frost, 1923\nthree\n"
    assert mpp.parse(text) == [
        ("a.txt", "Poe, 1845", ["one", "", "two"]),
        ("b.txt", "Frost, 1923", ["three"]),
    ]


def test_parse_refuses_text_that_is_not_ascii():
    with pytest.raises(ValueError, match="ASCII"):
        mpp.parse("== a.txt | Poe, 1845\ncafé\n")


def test_parse_refuses_a_poem_without_lines_or_a_byline():
    with pytest.raises(ValueError):
        mpp.parse("== a.txt | Poe, 1845\n")
    with pytest.raises(ValueError):
        mpp.parse("== a.txt\nline\n")
    with pytest.raises(ValueError):
        mpp.parse("stray\n== a.txt | Poe\nline\n")


def test_a_short_poem_is_one_page_that_clears_types_a_command_and_ends_on_a_prompt():
    out = mpp.render("a.txt", "Poe, 1845", ["one", "", "two"])
    assert out.startswith("\x1b[2J\x1b[H\r\n\r\n    > cat a.txt\r\n\r\n")
    assert "    one\r\n\r\n    two\r\n\r\n" in out
    assert out.endswith("    -- Poe, 1845\r\n    > _")
    assert mpp.PAUSE not in out


def strip_escapes(text: str) -> str:
    return re.sub(r"\x1b\[[0-9;]*[A-Za-z]", "", text)


def pages_of(name, byline, lines):
    return mpp.render(name, byline, lines).split(mpp.PAUSE)


def test_a_long_poem_is_paged_and_each_page_but_the_last_ends_with_more():
    lines = [f"line {i}" for i in range(30)]
    pages = pages_of("a", "X, 1900", lines)
    assert len(pages) > 1
    for page in pages[:-1]:
        assert page.endswith("    --More--")
    assert pages[-1].endswith("    > _")
    assert all(page.startswith("\x1b[2J\x1b[H") for page in pages)


def test_no_page_has_more_rows_than_the_circle_shows_and_none_is_empty():
    for name, byline, lines in mpp.parse(mpp.SOURCE.read_text()):
        for page in pages_of(name, byline, lines):
            rows = strip_escapes(page).split("\r\n")
            assert len(rows) <= mpp.LAST_ROW + 1, (name, len(rows))
            assert rows[mpp.FIRST_ROW].strip() == f"> cat {name}"
            assert any(r.strip() for r in rows[mpp.FIRST_ROW + 2 :]), (name, page)


def test_every_word_of_every_poem_is_typed_in_order():
    for name, byline, lines in mpp.parse(mpp.SOURCE.read_text()):
        typed = []
        for page in pages_of(name, byline, lines):
            rows = strip_escapes(page).split("\r\n")[mpp.FIRST_ROW + 2 :]
            typed += [w for r in rows for w in r.split()]
        typed = [w for w in typed if w not in ("--More--", ">", "_")]
        byline_words = f"-- {byline}".split()
        assert typed[-len(byline_words) :] == byline_words
        typed = typed[: -len(byline_words)]
        assert typed == [w for line in lines for w in line.split()], name


def test_every_rendered_line_stays_in_the_columns_the_circle_shows():
    for name, byline, lines in mpp.parse(mpp.SOURCE.read_text()):
        shown = strip_escapes(mpp.render(name, byline, lines).replace(mpp.PAUSE, ""))
        for row in shown.split("\r\n"):
            assert len(row) <= mpp.INDENT + mpp.WIDTH, (name, row)


def test_a_verse_line_is_never_split_across_pages():
    lines = [f"alpha beta gamma delta epsilon zeta eta theta {i}" for i in range(8)]
    for page in pages_of("a", "X, 1900", lines):
        rows = [r for r in strip_escapes(page).split("\r\n")[4:] if r.strip()]
        text = " ".join(
            r.strip() for r in rows if not r.strip().startswith(("--", ">"))
        )
        assert text == "" or text.startswith("alpha")
        assert text == "" or text.split()[-1].isdigit()


def test_the_committed_poems_are_all_ascii_and_each_has_a_distinct_name():
    poems = mpp.parse(mpp.SOURCE.read_text())
    assert len(poems) >= 5
    names = [p[0] for p in poems]
    assert len(set(names)) == len(names)


def test_the_committed_header_is_what_the_tool_makes():
    assert mpp.OUTPUT.read_text() == mpp.header(mpp.SOURCE.read_text())


def test_c_strings_escape_the_escape_character_and_quotes():
    text = mpp.c_string('\x1b[2J"hi"\r\nx')
    assert text == '    "\\033[2J\\"hi\\"\\r\\n"\n    "x"'
