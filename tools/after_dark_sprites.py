# /// script
# requires-python = ">=3.13"
# dependencies = ["machfs"]
# ///
"""Read After Dark 2.0 sprites out of its 800K disk image.

This module holds the pure-Python half: stripping sector tags from the image,
parsing a Mac resource fork and decoding the PICT resources the sprites use.
It never imports machfs at module level, so the tools tests run without it.
"""

import struct

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
                pixels[y * width + x] = clut.get(value, (255, 0, 255))
    return pos
