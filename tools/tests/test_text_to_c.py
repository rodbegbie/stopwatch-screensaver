import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import text_to_c as tc


def body(header: str) -> bytes:
    start = header.index('"') + 1
    end = header.rindex('"')
    return (
        header[start:end].encode("latin-1").decode("unicode_escape").encode("latin-1")
    )


def test_the_bytes_come_back_from_the_c_string():
    data = 'Primary\npower "systems"\\ ??=\t\xe9\n1\n'.encode()
    assert body(tc.c_string(data, "graphstat")) == data


def test_it_declares_a_static_array_of_that_name():
    out = tc.c_string(b"a\n", "graphstat")
    assert "static const unsigned char graphstat[] =" in out
    assert out.rstrip().endswith('";')


def test_no_trigraph_or_line_break_reaches_the_string():
    out = tc.c_string(b"??=\n\r", "x")
    literal = out[out.index('"') :]
    assert "??" not in literal
    assert "\n" not in literal.rstrip('\n;"')
