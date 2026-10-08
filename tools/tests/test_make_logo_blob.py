import os
import re
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import make_logo_blob as mlb  # noqa: E402

RED = bytes([255, 0, 0, 255])
GREEN = bytes([0, 255, 0, 255])
WHITE = bytes([255, 255, 255, 255])
CLEAR_BLUE = bytes([0, 0, 255, 0])


def test_rgb565_packs_five_six_five_bits():
    assert mlb.rgb565(255, 0, 0) == 0xF800
    assert mlb.rgb565(0, 255, 0) == 0x07E0
    assert mlb.rgb565(0, 0, 255) == 0x001F
    assert mlb.rgb565(255, 255, 255) == 0xFFFF


def test_blob_layout_matches_the_spec_in_ximage_loader_h():
    blob = mlb.build_blob(2, 2, RED + GREEN + CLEAR_BLUE + WHITE)
    assert blob == (
        b"565M"
        + bytes([2, 0, 2, 0])
        + bytes([0x00, 0xF8, 0xE0, 0x07])  # red, green: little-endian
        + bytes([0x00, 0x00, 0xFF, 0xFF])  # transparent pixels are stored as 0
        + bytes([0b11000000, 0b01000000])  # one mask byte a row, MSB first
    )


def test_alpha_of_128_or_more_counts_as_opaque():
    blob = mlb.build_blob(2, 1, bytes([9, 9, 9, 127, 9, 9, 9, 128]))
    assert blob[-1] == 0b01000000


def test_mask_rows_are_padded_to_whole_bytes():
    blob = mlb.build_blob(9, 3, RED * 27)
    assert len(blob) == 8 + 2 * 9 * 3 + 2 * 3
    assert blob[-6:] == bytes([0xFF, 0x80] * 3)


@pytest.mark.parametrize(
    "width, height, pixels",
    [(0, 1, b""), (1, 0, b""), (65536, 1, b""), (2, 2, RED * 3), (1, 1, b"abc")],
)
def test_bad_dimensions_or_pixel_count_are_errors(width, height, pixels):
    with pytest.raises(ValueError):
        mlb.build_blob(width, height, pixels)


def test_header_holds_the_blob_bytes_under_the_name_the_hack_expects():
    blob = mlb.build_blob(2, 2, RED + GREEN + CLEAR_BLUE + WHITE)
    text = mlb.c_header("logo_50_png", blob, "logo-50.gif")
    assert "static const unsigned char logo_50_png[] = {" in text
    assert "logo-50.gif" in text
    assert "#ifndef XSHIM_LOGO_50_PNG_H" in text
    body = text.split("= {", 1)[1].split("};", 1)[0]
    assert bytes(int(h, 16) for h in re.findall(r"0x([0-9a-f]{2})", body)) == blob
    assert text.endswith("#endif\n")


def blob_from_header(path):
    body = Path(path).read_text().split("= {", 1)[1].split("};", 1)[0]
    return bytes(int(h, 16) for h in re.findall(r"0x([0-9a-f]{2})", body))


def make_image(tmp_path, size, name="in.png"):
    pil = pytest.importorskip("PIL.Image")
    path = tmp_path / name
    pil.new("RGBA", size, (255, 0, 0, 255)).save(path)
    return path


def size_of(blob):
    return int.from_bytes(blob[4:6], "little"), int.from_bytes(blob[6:8], "little")


def test_size_option_shrinks_to_fit_keeping_the_aspect_ratio(tmp_path):
    src = make_image(tmp_path, (256, 128))
    out = tmp_path / "logo.h"
    assert mlb.main([str(src), "--name", "n", "--size", "50", "-o", str(out)]) == 0
    assert size_of(blob_from_header(out)) == (50, 25)


def test_size_option_never_enlarges_a_small_image(tmp_path):
    src = make_image(tmp_path, (20, 20))
    out = tmp_path / "logo.h"
    assert mlb.main([str(src), "--name", "n", "--size", "50", "-o", str(out)]) == 0
    assert size_of(blob_from_header(out)) == (20, 20)


def test_without_size_the_image_keeps_its_own_size(tmp_path):
    src = make_image(tmp_path, (64, 64))
    out = tmp_path / "logo.h"
    assert mlb.main([str(src), "--name", "n", "-o", str(out)]) == 0
    assert size_of(blob_from_header(out)) == (64, 64)


def test_output_directories_are_created(tmp_path):
    src = make_image(tmp_path, (8, 8))
    out = tmp_path / "a" / "b" / "logo.h"
    assert mlb.main([str(src), "--name", "n", "-o", str(out)]) == 0
    assert out.exists()


def test_an_unchanged_header_is_not_rewritten(tmp_path):
    src = make_image(tmp_path, (8, 8))
    out = tmp_path / "logo.h"
    args = [str(src), "--name", "n", "-o", str(out)]
    assert mlb.main(args) == 0
    os.utime(out, (1_000_000, 1_000_000))
    assert mlb.main(args) == 0
    assert out.stat().st_mtime == 1_000_000


def test_a_changed_image_does_rewrite_the_header(tmp_path):
    src = make_image(tmp_path, (8, 8))
    out = tmp_path / "logo.h"
    args = [str(src), "--name", "n", "-o", str(out)]
    assert mlb.main(args) == 0
    os.utime(out, (1_000_000, 1_000_000))
    make_image(tmp_path, (9, 9))
    assert mlb.main(args) == 0
    assert out.stat().st_mtime != 1_000_000


def test_a_missing_image_is_an_error_not_a_silent_fallback(tmp_path):
    pytest.importorskip("PIL.Image")
    with pytest.raises(FileNotFoundError):
        mlb.main([str(tmp_path / "nope.png"), "--name", "n", "-o", str(tmp_path / "x.h")])


def test_command_line_converts_an_image_file(tmp_path):
    pil = pytest.importorskip("PIL.Image")
    src = tmp_path / "tiny.png"
    image = pil.new("RGBA", (3, 2), (255, 0, 0, 255))
    image.putpixel((1, 0), (0, 0, 255, 0))
    image.save(src)
    out = tmp_path / "logo.h"
    assert mlb.main([str(src), "--name", "logo_50_png", "-o", str(out)]) == 0
    body = out.read_text().split("= {", 1)[1].split("};", 1)[0]
    blob = bytes(int(h, 16) for h in re.findall(r"0x([0-9a-f]{2})", body))
    assert blob[:8] == b"565M" + bytes([3, 0, 2, 0])
    assert blob[-2:] == bytes([0b10100000, 0b11100000])
