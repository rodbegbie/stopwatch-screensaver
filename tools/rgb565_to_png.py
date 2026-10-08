"""Convert a raw little-endian RGB565 frame dump to PNG (stdlib only).

Run with: uv run tools/rgb565_to_png.py in.raw WIDTH HEIGHT out.png
"""

import struct
import sys
import zlib


def _chunk(kind: bytes, body: bytes) -> bytes:
    crc = zlib.crc32(kind + body) & 0xFFFFFFFF
    return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", crc)


def convert(raw: bytes, w: int, h: int) -> bytes:
    if len(raw) != w * h * 2:
        raise ValueError(f"expected {w * h * 2} bytes for {w}x{h}, got {len(raw)}")
    pixels = struct.unpack(f"<{w * h}H", raw)
    rows = bytearray()
    for y in range(h):
        rows.append(0)
        for p in pixels[y * w : (y + 1) * w]:
            r, g, b = (p >> 11) & 0x1F, (p >> 5) & 0x3F, p & 0x1F
            rows += bytes((r << 3 | r >> 2, g << 2 | g >> 4, b << 3 | b >> 2))
    ihdr = struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)
    return (
        b"\x89PNG\r\n\x1a\n"
        + _chunk(b"IHDR", ihdr)
        + _chunk(b"IDAT", zlib.compress(bytes(rows), 9))
        + _chunk(b"IEND", b"")
    )


def main(argv: list[str]) -> int:
    if len(argv) != 4:
        print(__doc__)
        return 2
    src, w, h, dst = argv[0], int(argv[1]), int(argv[2]), argv[3]
    with open(src, "rb") as f:
        png = convert(f.read(), w, h)
    with open(dst, "wb") as f:
        f.write(png)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
