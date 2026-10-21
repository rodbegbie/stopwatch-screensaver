import sys
from pathlib import Path

import pytest
from PIL import ImageFont

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import make_font_atlas as mfa

ROOT = Path(__file__).resolve().parents[2]
TTF = ROOT / "firmware" / "assets" / "fonts" / "SpecialElite-Regular.ttf"
FIRST, LAST = 32, 126


@pytest.fixture(scope="module")
def font():
    return mfa.build_font(TTF, [12, 24])


def glyph(size, ch):
    return size["glyphs"][ord(ch) - FIRST]


def test_each_size_holds_every_printable_ascii_glyph(font):
    assert [s["pixel_size"] for s in font] == [12, 24]
    for size in font:
        assert len(size["glyphs"]) == LAST - FIRST + 1


def test_ascent_and_descent_come_from_the_font(font):
    ascent, descent = ImageFont.truetype(str(TTF), 24).getmetrics()
    big = font[1]
    assert (big["ascent"], big["descent"]) == (ascent, descent)


def test_advance_is_the_rounded_text_length(font):
    pil = ImageFont.truetype(str(TTF), 24)
    for ch in "N.i W":
        assert glyph(font[1], ch)["advance"] == round(pil.getlength(ch))


def test_space_has_an_advance_and_no_ink(font):
    space = glyph(font[1], " ")
    assert space["advance"] > 0
    assert (space["w"], space["h"]) == (0, 0)


def test_a_capital_has_solid_ink_sitting_on_the_baseline(font):
    size = font[1]
    n = glyph(size, "N")
    assert n["w"] > 0 and n["h"] > 0
    rows = [
        size["alpha"][n["offset"] + r * n["w"] : n["offset"] + (r + 1) * n["w"]]
        for r in range(n["h"])
    ]
    assert max(max(r) for r in rows) >= 200
    assert any(rows[0]) and any(rows[-1])
    assert 0 < n["top"] <= size["ascent"]
    assert n["top"] - n["h"] <= 1


def test_ink_sits_near_its_advance_box(font):
    for size in font:
        slack = size["pixel_size"] // 6
        for g in size["glyphs"]:
            if g["w"]:
                assert -slack <= g["left"]
                assert g["left"] + g["w"] <= g["advance"] + slack


def test_offsets_pack_the_alpha_bytes_with_no_gaps(font):
    for size in font:
        end = 0
        for g in size["glyphs"]:
            if g["w"] * g["h"]:
                assert g["offset"] == end
                end += g["w"] * g["h"]
        assert end == len(size["alpha"])


def test_the_header_is_deterministic_and_names_its_tables(font):
    one = mfa.c_header("special_elite", font, "SpecialElite-Regular.ttf")
    two = mfa.c_header("special_elite", font, "SpecialElite-Regular.ttf")
    assert one == two
    assert "special_elite_font" in one
    assert "XshimFont" in one
    assert one.count("XshimGlyph special_elite_glyphs_") == 2
    assert "SpecialElite-Regular.ttf" in one


def test_an_unreadable_font_is_an_error(tmp_path):
    bad = tmp_path / "bad.ttf"
    bad.write_bytes(b"not a font")
    with pytest.raises(ValueError):
        mfa.build_font(bad, [12])


def test_a_size_below_four_pixels_is_refused():
    with pytest.raises(ValueError):
        mfa.build_font(TTF, [3])


def test_a_size_from_the_digits_only_threshold_has_ink_for_digits_alone():
    font = mfa.build_font(TTF, [24, 100], digits_from=100)
    small, big = font
    assert glyph(small, "A")["w"] > 0
    for ch in "0123456789":
        assert glyph(big, ch)["w"] > 0
    for ch in "AMx.":
        assert glyph(big, ch)["w"] == 0
    assert glyph(big, "A")["advance"] > 0


def test_a_digits_only_size_keeps_the_advance_of_the_glyphs_it_drops():
    plain = mfa.build_font(TTF, [100])[0]
    digits = mfa.build_font(TTF, [100], digits_from=100)[0]
    assert glyph(digits, "M")["advance"] == glyph(plain, "M")["advance"]
    assert len(digits["alpha"]) < len(plain["alpha"]) // 4


def test_a_glyph_wider_than_the_table_can_hold_is_refused():
    with pytest.raises(ValueError, match="255"):
        mfa.build_font(TTF, [400])


def test_a_glyph_that_sits_too_far_from_its_pen_is_refused():
    with pytest.raises(ValueError, match="127"):
        mfa.build_font(TTF, [200], digits_from=100)
