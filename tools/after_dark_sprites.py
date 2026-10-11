# /// script
# requires-python = ">=3.13"
# dependencies = ["machfs"]
# ///
"""Read After Dark 2.0 sprites out of its 800K disk image.

Strips sector tags from the image, reads the Flying Toasters module's resource
fork through machfs, decodes its PICT sprites and writes them as a C header.
It never imports machfs at module level, so the tools tests run without it. machfs (the HFS reader uv fetches) is
MIT-licensed: https://github.com/elliotnunn/machfs.

    uv run tools/after_dark_sprites.py IMAGE -o HEADER
"""

import argparse
import hashlib
import struct
import sys
from pathlib import Path

TAGGED_IMAGE_SIZE = 838_400
PLAIN_VOLUME_SIZE = 819_200
SECTOR_DATA = 512
SECTOR_TAG = 12


class ExtractError(Exception):
    pass


def strip_sector_tags(image: bytes) -> bytes:
    if len(image) == PLAIN_VOLUME_SIZE:
        return image
    if len(image) != TAGGED_IMAGE_SIZE:
        raise ExtractError(
            f"expected a disk image of 838,400 or 819,200 bytes, got {len(image):,}"
        )
    stride = SECTOR_DATA + SECTOR_TAG
    return b"".join(image[i : i + SECTOR_DATA] for i in range(0, len(image), stride))


def parse_resource_fork(fork: bytes) -> dict[tuple[str, int], bytes]:
    data_offset, map_offset, _data_len, map_len = struct.unpack(">IIII", fork[:16])
    resource_map = fork[map_offset : map_offset + map_len]
    type_list_offset, _name_list_offset = struct.unpack(">HH", resource_map[24:28])
    type_list = resource_map[type_list_offset:]
    type_count = struct.unpack(">H", type_list[:2])[0] + 1

    resources = {}
    for t in range(type_count):
        entry = type_list[2 + t * 8 : 10 + t * 8]
        raw_type, count_minus_one, refs_offset = struct.unpack(">4sHH", entry)
        rtype = raw_type.decode("mac_roman")
        for r in range(count_minus_one + 1):
            ref = type_list[refs_offset + r * 12 : refs_offset + r * 12 + 12]
            rid, _name_offset, attr_and_offset = struct.unpack(">hhI", ref[:8])
            start = data_offset + (attr_and_offset & 0xFFFFFF)
            length = struct.unpack(">I", fork[start : start + 4])[0]
            resources[(rtype, rid)] = fork[start + 4 : start + 4 + length]
    return resources


def unpack_bits(src: bytes, pos: int, size: int) -> bytes:
    out = bytearray()
    while len(out) < size:
        count = src[pos]
        pos += 1
        if count < 128:
            out += src[pos : pos + count + 1]
            pos += count + 1
        elif count > 128:
            out += src[pos : pos + 1] * (257 - count)
            pos += 1
    return bytes(out[:size])


def read_rect(data: bytes, pos: int) -> tuple[int, int, int, int]:
    return struct.unpack(">hhhh", data[pos : pos + 8])


def decode_pict(data: bytes) -> tuple[int, int, list[tuple[int, int, int]]]:
    f_top, f_left, f_bottom, f_right = read_rect(data, 2)
    width, height = f_right - f_left, f_bottom - f_top
    pixels = [(255, 255, 255)] * (width * height)
    drawn = False

    pos = 10
    v2 = data[pos : pos + 4] == b"\x00\x11\x02\xff"
    if v2:
        pos += 4

    while pos < len(data):
        if v2:
            pos += pos & 1
            op = struct.unpack(">H", data[pos : pos + 2])[0]
            pos += 2
        else:
            op = data[pos]
            pos += 1

        if op == 0x11 and not v2:
            pos += 1
        elif op == 0x0C00 and v2:
            pos += 24
        elif op == 0x1E:
            pass
        elif op == 0x01:
            pos += struct.unpack(">H", data[pos : pos + 2])[0]
        elif op == 0xA0:
            pos += 2
        elif op == 0xA1:
            length = struct.unpack(">H", data[pos + 2 : pos + 4])[0]
            pos += 4 + length
        elif op == 0x98:
            pos = draw_packbits_rect(
                data, pos, (f_top, f_left), (width, height), pixels
            )
            drawn = True
        elif op == 0xFF:
            break
        else:
            raise ExtractError(f"unknown PICT opcode 0x{op:04X} at offset {pos}")

    if not drawn:
        raise ExtractError("PICT has no image")
    return width, height, pixels


def draw_packbits_rect(data, pos, frame_origin, frame_size, pixels) -> int:
    raw_row_bytes = struct.unpack(">H", data[pos : pos + 2])[0]
    is_pixmap = bool(raw_row_bytes & 0x8000)
    row_bytes = raw_row_bytes & 0x3FFF
    pos += 2
    b_top, b_left, b_bottom, b_right = read_rect(data, pos)
    pos += 8

    depth = 1
    clut: dict[int, tuple[int, int, int]] = {}
    if is_pixmap:
        depth = struct.unpack(">h", data[pos + 18 : pos + 20])[0]
        if depth not in (1, 2, 4, 8):
            raise ExtractError(f"unsupported PixMap depth {depth}")
        pos += 36
        _seed, flags, count_minus_one = struct.unpack(">IHH", data[pos : pos + 8])
        pos += 8
        for i in range(count_minus_one + 1):
            index, r, g, b = struct.unpack(">HHHH", data[pos : pos + 8])
            clut[i if flags & 0x8000 else index] = (r >> 8, g >> 8, b >> 8)
            pos += 8

    pos += 18  # srcRect, dstRect, mode

    rows = []
    for _ in range(b_bottom - b_top):
        if row_bytes < 8:
            rows.append(data[pos : pos + row_bytes])
            pos += row_bytes
            continue
        if row_bytes > 250:
            packed_len = struct.unpack(">H", data[pos : pos + 2])[0]
            pos += 2
        else:
            packed_len = data[pos]
            pos += 1
        rows.append(unpack_bits(data, pos, row_bytes))
        pos += packed_len

    f_top, f_left = frame_origin
    width, height = frame_size
    per_byte = 8 // depth
    mask = (1 << depth) - 1
    for y in range(height):
        row_index = f_top + y - b_top
        if not 0 <= row_index < len(rows):
            continue
        row = rows[row_index]
        for x in range(width):
            col = f_left + x - b_left
            if not 0 <= col < (b_right - b_left):
                continue
            if depth == 1:
                bit = (row[col >> 3] >> (7 - (col & 7))) & 1
                pixels[y * width + x] = (0, 0, 0) if bit else (255, 255, 255)
            else:
                shift = (per_byte - 1 - col % per_byte) * depth
                value = (row[col // per_byte] >> shift) & mask
                if value not in clut:
                    raise ExtractError(
                        f"PICT pixel uses colour {value}, not in its table"
                    )
                pixels[y * width + x] = clut[value]
    return pos


SPRITE_SIZE = 64
COLOUR_PICT_IDS = (228, 229, 230, 231, 232)
MONO_PICT_IDS = (128, 129, 130, 131, 132)
MODULE_FOLDER = "After Dark Files"
MODULE_FILE = "Flying Toasters"


def mono_bits(pixels: list[tuple[int, int, int]]) -> bytes:
    out = bytearray(len(pixels) // 8)
    for i, pixel in enumerate(pixels):
        if pixel == (0, 0, 0):
            out[i >> 3] |= 0x80 >> (i & 7)
    return bytes(out)


def sprites_from_fork(
    fork: bytes,
) -> tuple[list[list[tuple[int, int, int]]], list[bytes]]:
    resources = parse_resource_fork(fork)

    def load(pict_id: int) -> list[tuple[int, int, int]]:
        if ("PICT", pict_id) not in resources:
            raise ExtractError(f"PICT {pict_id} not found in the module")
        width, height, pixels = decode_pict(resources[("PICT", pict_id)])
        if (width, height) != (SPRITE_SIZE, SPRITE_SIZE):
            raise ExtractError(f"PICT {pict_id} is {width}x{height}, expected 64x64")
        return pixels

    colour = [load(i) for i in COLOUR_PICT_IDS]
    mono = [mono_bits(load(i)) for i in MONO_PICT_IDS]
    return colour, mono


def c_array(name: str, rows: list[bytes], row_len: str, comment: str) -> str:
    lines = [
        f"static const unsigned char {name}[{len(rows)}][{row_len}] = {{ /* {comment} */"
    ]
    for row in rows:
        lines.append("  {")
        for i in range(0, len(row), 16):
            lines.append("    " + ",".join(f"0x{b:02X}" for b in row[i : i + 16]) + ",")
        lines.append("  },")
    lines.append("};")
    return "\n".join(lines)


def render_header(
    colour, mono, sha256: str, source: str = "the After Dark 2.0 disk image"
) -> str:
    colour_rows = [bytes(c for pixel in sprite for c in pixel) for sprite in colour]
    return "\n".join(
        [
            (f"/* Generated from {source}, sha256 {sha256}. Do not commit. */"),
            "#ifndef AFTER_DARK_TOASTER_SPRITES_H",
            "#define AFTER_DARK_TOASTER_SPRITES_H",
            "",
            f"#define AD_ART_SIZE {SPRITE_SIZE}",
            "",
            c_array("ad_colour_rgb", colour_rows, "64 * 64 * 3", "frames 0-3, toast"),
            "",
            c_array("ad_mono_bits", list(mono), "64 * 64 / 8", "MSB first, 1 = black"),
            "",
            "#endif",
            "",
        ]
    )


def read_module_fork(volume: bytes) -> bytes:
    import machfs

    vol = machfs.Volume()
    try:
        vol.read(volume)
    except Exception as error:  # noqa: BLE001 - machfs raises assorted types on junk
        raise ExtractError(f"the volume is not readable HFS ({error})") from None
    try:
        folder = vol[MODULE_FOLDER]
    except KeyError:
        raise ExtractError(f"folder '{MODULE_FOLDER}' not found on the disk") from None
    try:
        module = folder[MODULE_FILE]
    except KeyError:
        raise ExtractError(
            f"file '{MODULE_FILE}' not found in '{MODULE_FOLDER}'"
        ) from None
    return module.rsrc


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("image", type=Path)
    parser.add_argument("-o", "--output", type=Path, required=True)
    args = parser.parse_args(argv)

    try:
        image = args.image.read_bytes()
        fork = read_module_fork(strip_sector_tags(image))
        colour, mono = sprites_from_fork(fork)
        text = render_header(
            colour, mono, hashlib.sha256(image).hexdigest(), args.image.name
        )
    except (ExtractError, OSError) as error:
        print(f"after_dark_sprites: {error}", file=sys.stderr)
        return 1

    partial = args.output.with_name(args.output.name + ".partial")
    try:
        partial.write_text(text)
        partial.replace(args.output)
    except OSError as error:
        partial.unlink(missing_ok=True)
        print(f"after_dark_sprites: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
