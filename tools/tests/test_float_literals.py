import re
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import float_literals as fl  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
BRAID_C = ROOT / "firmware" / "src" / "hacks" / "braid" / "braid.c"


def test_decimal_literals_become_float():
    assert fl.float_literals("x = 0.5 * (1.0 + y) / 12.0;") == (
        "x = 0.5f * (1.0f + y) / 12.0f;"
    )


def test_leading_and_trailing_point_forms():
    assert fl.float_literals("a = .5 + 5.;") == "a = .5f + 5.f;"


def test_literals_that_already_have_a_suffix_are_left_alone():
    assert fl.float_literals("a = 2.0f + 3.0L + 4.0;") == "a = 2.0f + 3.0L + 4.0f;"


def test_integers_member_access_and_identifiers_are_left_alone():
    source = "n = 15 + v.x + arr[1].y + p->a1.b + x2.c;"
    assert fl.float_literals(source + " k = 1.5;") == source + " k = 1.5f;"


def test_strings_comments_and_chars_are_left_alone():
    source = (
        '"*delay: 1.0 \\n" /* 2.0 */ // 3.0\n'
        "c = '.'; s = \"a\\\"4.0\"; z = 6.0;"
    )
    out = fl.float_literals(source)
    assert '"*delay: 1.0 \\n"' in out
    assert "/* 2.0 */" in out
    assert "// 3.0\n" in out
    assert "'.'" in out
    assert '"a\\"4.0"' in out
    assert out.endswith("z = 6.0f;")


def test_nothing_else_changes():
    source = "if (a >= 0.25) {\n\tb = c * 4;\n}\n"
    out = fl.float_literals(source)
    assert out.replace("0.25f", "0.25") == source


def test_a_source_with_no_literals_is_an_error():
    with pytest.raises(ValueError, match="no floating-point literals"):
        fl.float_literals("int x = 1;\n")


def test_braid_leaves_no_double_literal_outside_strings_and_comments():
    out = fl.float_literals(BRAID_C.read_text())
    code = re.sub(r'/\*.*?\*/|"(?:\\.|[^"\\])*"', "", out, flags=re.DOTALL)
    assert not re.search(r"(?<![\w.])\d+\.\d*(?![\w.])", code)
    assert "0.30f" in out and "M_PI_2" in out
