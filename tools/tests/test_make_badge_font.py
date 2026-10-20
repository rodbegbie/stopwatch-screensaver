import shutil
import subprocess
import sys
from pathlib import Path

import pytest
from PIL import Image, ImageDraw, ImageFont

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import make_badge_font as mbf

FIRST, LAST = 32, 126
SIZE = 12


@pytest.fixture(scope="module")
def pil_font():
    return ImageFont.load_default(size=SIZE)


@pytest.fixture(scope="module")
def font(pil_font):
    return mbf.build_font(pil_font, SIZE)


def glyph(font, ch):
    return font["glyphs"][ord(ch) - FIRST]


def render(pil_font, ch):
    """The ink of ch as rows of 0/1, cropped, plus its left and rows above the baseline."""
    canvas = Image.new("L", (SIZE * 3, SIZE * 3), 0)
    draw = ImageDraw.Draw(canvas)
    draw.fontmode = "1"
    draw.text((SIZE, SIZE * 2), ch, fill=255, font=pil_font, anchor="ls")
    box = canvas.getbbox()
    if box is None:
        return [], 0, 0
    ink = canvas.crop(box)
    rows = [
        [1 if ink.getpixel((x, y)) else 0 for x in range(ink.width)]
        for y in range(ink.height)
    ]
    return rows, box[0] - SIZE, SIZE * 2 - box[1]


def test_every_printable_ascii_glyph_is_present(font):
    assert (font["first"], font["last"]) == (FIRST, LAST)
    assert len(font["glyphs"]) == LAST - FIRST + 1


def test_advance_is_the_rounded_text_length(font, pil_font):
    for ch in "H.i W":
        assert glyph(font, ch)["advance"] == round(pil_font.getlength(ch))


class Fractional:
    """The real font, but with advances a fraction of a pixel longer."""

    def __init__(self, font, extra):
        self._font = font
        self._extra = extra

    def __getattr__(self, name):
        return getattr(self._font, name)

    def getlength(self, text, *args, **kwargs):
        return self._font.getlength(text, *args, **kwargs) + self._extra


@pytest.mark.parametrize("extra, grows_by", [(0.55, 1), (0.3, 0)])
def test_fractional_advances_round_to_the_nearest_pixel(pil_font, extra, grows_by):
    plain = mbf.build_font(pil_font, SIZE)
    longer = mbf.build_font(Fractional(pil_font, extra), SIZE)
    for a, b in zip(plain["glyphs"], longer["glyphs"]):
        assert b["advance"] - a["advance"] == grows_by


def test_space_has_an_advance_and_no_ink(font):
    space = glyph(font, " ")
    assert space["advance"] > 0
    assert (space["w"], space["h"]) == (0, 0)


def test_unpacked_bitmap_is_the_one_bit_render(font, pil_font):
    for code in range(FIRST, LAST + 1):
        ch = chr(code)
        rows, _, _ = render(pil_font, ch)
        assert mbf.unpack(font["bitmap"], glyph(font, ch)) == rows, ch


def test_y_offset_counts_rows_above_the_baseline_as_negative(font, pil_font):
    for ch in "Hgp.":
        _, _, above = render(pil_font, ch)
        assert glyph(font, ch)["y_offset"] == -above, ch


def test_a_descender_reaches_below_the_baseline_and_a_capital_does_not(font):
    g, h = glyph(font, "g"), glyph(font, "H")
    assert g["y_offset"] + g["h"] > 0
    assert h["y_offset"] + h["h"] <= 0


def test_x_offset_is_the_left_bearing(font, pil_font):
    for ch in "Hjy,":
        _, left, _ = render(pil_font, ch)
        assert glyph(font, ch)["x_offset"] == left, ch


def test_line_height_spans_the_tallest_ascent_and_deepest_descent(font):
    above = max(-g["y_offset"] for g in font["glyphs"] if g["h"])
    below = max(g["y_offset"] + g["h"] for g in font["glyphs"] if g["h"])
    assert font["y_advance"] == above + below


def test_each_glyph_starts_on_a_byte_and_offsets_leave_no_gaps(font):
    end = 0
    for g in font["glyphs"]:
        if g["w"] * g["h"]:
            assert g["offset"] == end
            end += (g["w"] * g["h"] + 7) // 8
    assert end == len(font["bitmap"])


def test_the_first_bit_of_a_glyph_is_the_top_left_pixel_in_the_high_bit(font, pil_font):
    for ch in "Hl":
        rows, _, _ = render(pil_font, ch)
        g = glyph(font, ch)
        assert bool(font["bitmap"][g["offset"]] & 0x80) == bool(rows[0][0])


def test_a_file_that_is_not_a_font_is_rejected(tmp_path):
    bad = tmp_path / "bad.ttf"
    bad.write_bytes(b"not a font")
    with pytest.raises(ValueError, match="not a usable font"):
        mbf.open_font(bad, SIZE)


def test_header_declares_the_gfxfont_the_overlay_draws_with(font):
    text = mbf.c_header("pix_chicago", font, "pixChicago.ttf")
    assert "pixChicago.ttf" in text
    assert "Do not edit" in text
    assert "static uint8_t pix_chicago_bitmap[]" in text
    assert "static lgfx::GFXglyph pix_chicago_glyphs[]" in text
    assert (
        "static const lgfx::GFXfont pix_chicago_font(pix_chicago_bitmap, "
        f"pix_chicago_glyphs, {FIRST}, {LAST}, {font['y_advance']});"
    ) in text
    assert text == mbf.c_header("pix_chicago", font, "pixChicago.ttf")


@pytest.mark.skipif(shutil.which("c++") is None, reason="no C++ compiler")
def test_header_compiles_against_the_gfxfont_layout(font, tmp_path):
    (tmp_path / "stub.hpp").write_text(
        """
#include <stdint.h>
namespace lgfx {
struct IFont { virtual ~IFont() = default; };
struct EncodeRange { uint16_t start, end, base; };
struct GFXglyph {
  uint32_t bitmapOffset; uint8_t width, height; uint8_t xAdvance;
  int8_t xOffset, yOffset;
};
struct GFXfont : public IFont {
  uint8_t *bitmap; GFXglyph *glyph; uint16_t first, last; uint8_t yAdvance;
  uint16_t range_num; EncodeRange *range;
  constexpr GFXfont(uint8_t *b, GFXglyph *g, uint16_t f, uint16_t l, uint8_t y,
                    uint16_t rn = 0, EncodeRange *r = nullptr)
      : bitmap(b), glyph(g), first(f), last(l), yAdvance(y), range_num(rn), range(r) {}
};
}
"""
    )
    (tmp_path / "pix_chicago.h").write_text(mbf.c_header("pix_chicago", font, "t.ttf"))
    (tmp_path / "use.cpp").write_text(
        '#include "stub.hpp"\n#include "pix_chicago.h"\n'
        f'static_assert(sizeof(pix_chicago_glyphs) / sizeof(lgfx::GFXglyph) == {LAST - FIRST + 1}, "");\n'
        "int main() { return pix_chicago_font.first == 32 ? 0 : 1; }\n"
    )
    result = subprocess.run(
        [
            "c++",
            "-std=c++17",
            "-Werror",
            "-I",
            str(tmp_path),
            "-o",
            str(tmp_path / "use"),
            str(tmp_path / "use.cpp"),
        ],
        capture_output=True,
        text=True,
        check=False,
    )
    assert result.returncode == 0, result.stderr
    assert subprocess.run([str(tmp_path / "use")], check=False).returncode == 0
