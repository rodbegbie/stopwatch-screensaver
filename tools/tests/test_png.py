import struct
import sys
import zlib
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import rgb565_to_png as rp  # noqa: E402


def decode_png(data: bytes):
    assert data[:8] == b"\x89PNG\r\n\x1a\n"
    pos, idat, size = 8, b"", None
    while pos < len(data):
        (length,) = struct.unpack(">I", data[pos : pos + 4])
        kind = data[pos + 4 : pos + 8]
        body = data[pos + 8 : pos + 8 + length]
        if kind == b"IHDR":
            size = struct.unpack(">II", body[:8])
        elif kind == b"IDAT":
            idat += body
        pos += 12 + length
    raw = zlib.decompress(idat)
    w, h = size
    rows = []
    for y in range(h):
        row = raw[y * (1 + 3 * w) : (y + 1) * (1 + 3 * w)]
        assert row[0] == 0
        rows.append([tuple(row[1 + 3 * x : 4 + 3 * x]) for x in range(w)])
    return w, h, rows


def test_convert_has_png_signature():
    assert rp.convert(b"\x00\x00", 1, 1)[:8] == b"\x89PNG\r\n\x1a\n"


def test_convert_2x1_red_green_roundtrips():
    raw = struct.pack("<HH", 0xF800, 0x07E0)
    w, h, rows = decode_png(rp.convert(raw, 2, 1))
    assert (w, h) == (2, 1)
    assert rows[0][0] == (255, 0, 0)
    assert rows[0][1] == (0, 255, 0)


def test_convert_white_expands_to_255():
    raw = struct.pack("<H", 0xFFFF)
    _, _, rows = decode_png(rp.convert(raw, 1, 1))
    assert rows[0][0] == (255, 255, 255)


def test_convert_wrong_length_raises_value_error():
    with pytest.raises(ValueError):
        rp.convert(b"\x00\x00\x00", 2, 1)
