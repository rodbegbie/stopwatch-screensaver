import struct
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import after_dark_sprites as ads

BLACK = (0, 0, 0)
WHITE = (255, 255, 255)


def make_tagged_image(volume: bytes) -> bytes:
    assert len(volume) % 512 == 0
    out = bytearray()
    for i in range(0, len(volume), 512):
        out += volume[i : i + 512]
        out += bytes([0xEE]) * 12
    return bytes(out)


def make_resource_fork(resources: dict[tuple[str, int], bytes]) -> bytes:
    data = bytearray()
    offsets = {}
    for key, body in resources.items():
        offsets[key] = len(data)
        data += struct.pack(">I", len(body)) + body

    by_type: dict[str, list[int]] = {}
    for rtype, rid in resources:
        by_type.setdefault(rtype, []).append(rid)

    type_list_size = 2 + 8 * len(by_type)
    type_entries = b""
    refs = b""
    for rtype, ids in by_type.items():
        ref_offset = type_list_size + len(refs)
        type_entries += struct.pack(
            ">4sHH", rtype.encode("mac_roman"), len(ids) - 1, ref_offset
        )
        for rid in ids:
            attr_offset = offsets[(rtype, rid)]
            refs += struct.pack(">hhI", rid, -1, attr_offset) + bytes(4)
    type_list = struct.pack(">H", len(by_type) - 1) + type_entries + refs

    map_header_size = 28
    type_list_offset = map_header_size
    name_list_offset = type_list_offset + len(type_list)
    resource_map = (
        bytes(24) + struct.pack(">HH", type_list_offset, name_list_offset) + type_list
    )

    data_offset = 256
    map_offset = data_offset + len(data)
    header = struct.pack(">IIII", data_offset, map_offset, len(data), len(resource_map))
    return header + bytes(data_offset - len(header)) + bytes(data) + resource_map


def packbits(row: bytes) -> bytes:
    out = bytearray()
    i = 0
    literal = bytearray()

    def flush():
        nonlocal literal
        while literal:
            chunk, literal = literal[:128], literal[128:]
            out.append(len(chunk) - 1)
            out.extend(chunk)

    while i < len(row):
        run = 1
        while i + run < len(row) and row[i + run] == row[i] and run < 128:
            run += 1
        if run >= 3:
            flush()
            out.append(257 - run)
            out.append(row[i])
            i += run
        else:
            literal.append(row[i])
            i += 1
    flush()
    return bytes(out)


def encode_rows(rows: list[bytes], row_bytes: int) -> bytes:
    out = b""
    for row in rows:
        assert len(row) == row_bytes
        if row_bytes < 8:
            out += row
            continue
        packed = packbits(row)
        count = (
            struct.pack(">H", len(packed)) if row_bytes > 250 else bytes([len(packed)])
        )
        out += count + packed
    return out


def rect(top: int, left: int, bottom: int, right: int) -> bytes:
    return struct.pack(">hhhh", top, left, bottom, right)


def clip_region(box: tuple[int, int, int, int]) -> bytes:
    return struct.pack(">H", 10) + rect(*box)


def make_pict_v1_bitmap(frame, bounds, rows) -> bytes:
    top, left, bottom, right = bounds
    row_bytes = (right - left + 7) // 8
    assert len(rows) == bottom - top
    stream = (
        bytes([0x11, 0x01])
        + bytes([0x01])
        + clip_region(frame)
        + bytes([0xA0])
        + struct.pack(">H", 7)
        + bytes([0x98])
        + struct.pack(">H", row_bytes)
        + rect(*bounds)
        + rect(*bounds)
        + rect(*frame)
        + struct.pack(">h", 1)
        + encode_rows(rows, row_bytes)
        + bytes([0xFF])
    )
    return struct.pack(">H", len(stream) + 10) + rect(*frame) + stream


V2_PREAMBLE = struct.pack(">HH", 0x0011, 0x02FF) + struct.pack(">H", 0x0C00) + bytes(24)


def pict_v2(frame, ops: bytes) -> bytes:
    stream = V2_PREAMBLE + ops
    return struct.pack(">H", len(stream) + 10) + rect(*frame) + stream


def make_pict_v2_indexed(frame, clut, rows, flags=0, depth=4) -> bytes:
    top, left, bottom, right = frame
    per_byte = 8 // depth
    row_bytes = (right - left + per_byte - 1) // per_byte
    assert len(rows) == bottom - top
    pixmap_header = struct.pack(
        ">hhIiihhhhIII", 0, 0, 0, 0x480000, 0x480000, 0, depth, 1, depth, 0, 0, 0
    )
    entries = b""
    for position, (index, (r, g, b)) in enumerate(clut.items()):
        stored = index if not flags & 0x8000 else 0xFFFF - position
        entries += struct.pack(">HHHH", stored, r, g, b)
    table = struct.pack(">IHH", 0x1234, flags, len(clut) - 1) + entries
    long_comment = (
        struct.pack(">H", 0x00A1) + struct.pack(">HH", 100, 3) + b"abc" + b"\x00"
    )
    ops = (
        struct.pack(">H", 0x001E)
        + struct.pack(">H", 0x0001)
        + clip_region(frame)
        + struct.pack(">H", 0x00A0)
        + struct.pack(">H", 7)
        + long_comment
        + struct.pack(">H", 0x0098)
        + struct.pack(">H", row_bytes | 0x8000)
        + rect(*frame)
        + pixmap_header
        + table
        + rect(*frame)
        + rect(*frame)
        + struct.pack(">h", 0)
        + encode_rows(rows, row_bytes)
    )
    ops += bytes(len(ops) & 1) + struct.pack(">H", 0x00FF)
    return pict_v2(frame, ops)


def test_strips_twelve_byte_tag_after_each_512_byte_sector():
    volume = bytes(range(256)) * 3200
    assert len(volume) == 819_200
    tagged = make_tagged_image(volume)
    assert len(tagged) == 838_400
    assert ads.strip_sector_tags(tagged) == volume


def test_plain_volume_passes_through():
    volume = bytes(819_200)
    assert ads.strip_sector_tags(volume) == volume


def test_rejects_image_of_unexpected_size():
    with pytest.raises(ads.ExtractError, match="838,400 or 819,200"):
        ads.strip_sector_tags(bytes(819_284))


def test_resource_fork_round_trip():
    res = {("PICT", 128): b"abc", ("PICT", 228): b"defg", ("snd ", 130): b""}
    assert ads.parse_resource_fork(make_resource_fork(res)) == res


def test_resource_fork_with_ids_across_types_and_negative_id():
    res = {("ICN#", -16455): b"\x01\x02", ("PICT", 128): b"x" * 300, ("ICN#", 7): b"y"}
    assert ads.parse_resource_fork(make_resource_fork(res)) == res


V1_FRAME = (0, 0, 2, 8)
V1_BOUNDS = (0, -32, 2, 40)


def v1_rows():
    row0 = bytes([0xFF, 0xFF, 0xFF, 0xFF, 0x80, 0x00, 0x00, 0x00, 0xFF])
    row1 = bytes([0x00, 0x00, 0x00, 0x00, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF])
    return [row0, row1]


def test_v1_one_bit_pict_draws_through_picframe():
    w, h, px = ads.decode_pict(make_pict_v1_bitmap(V1_FRAME, V1_BOUNDS, v1_rows()))
    assert (w, h) == (8, 2)
    assert len(px) == 16
    assert px[0] == BLACK and px[1] == WHITE
    assert px[2:8] == [WHITE] * 6
    assert px[8] == WHITE and px[9] == BLACK and px[15] == BLACK
    # the set bits in the margin outside the frame do not appear
    assert BLACK not in px[2:8]


def test_v1_pict_with_unpacked_rows_when_row_bytes_under_eight():
    frame = (0, 0, 2, 8)
    bounds = (0, -4, 2, 12)
    rows = [bytes([0xF0, 0x00]), bytes([0x0F, 0xFF])]
    w, h, px = ads.decode_pict(make_pict_v1_bitmap(frame, bounds, rows))
    assert (w, h) == (8, 2)
    assert px[:8] == [WHITE] * 8
    assert px[8:] == [BLACK] * 8


def test_v1_pict_with_two_byte_packbits_counts_when_row_bytes_over_250():
    frame = (0, 0, 1, 8)
    bounds = (0, 0, 1, 2064)
    row = bytes([0xAA]) + bytes([0x01, 0x23, 0x45]) * 85 + bytes(2)
    assert len(row) == 258
    w, h, px = ads.decode_pict(make_pict_v1_bitmap(frame, bounds, [row]))
    assert (w, h) == (8, 1)
    assert px == [BLACK, WHITE, BLACK, WHITE, BLACK, WHITE, BLACK, WHITE]


def test_v1_frame_offset_from_bounds_uses_top_and_left():
    frame = (3, 10, 5, 18)
    bounds = (2, 8, 6, 80)
    rows = [
        bytes(9),
        bytes([0b00100000]) + bytes(8),
        bytes([0b00000010]) + bytes(8),
        bytes(9),
    ]
    w, h, px = ads.decode_pict(make_pict_v1_bitmap(frame, bounds, rows))
    assert (w, h) == (8, 2)
    assert px[0] == BLACK
    assert px[8 + 4] == BLACK
    assert px.count(BLACK) == 2


V2_FRAME = (0, 0, 2, 16)
V2_CLUT = {0: (0, 0, 0), 5: (0xFFFF, 0x8000, 0x0000)}


def v2_rows():
    row0 = bytes([0x00, 0x05]) + bytes(6)
    row1 = bytes([0x55]) * 8
    return [row0, row1]


def test_v2_four_bit_pict_maps_through_its_colour_table():
    w, h, px = ads.decode_pict(make_pict_v2_indexed(V2_FRAME, V2_CLUT, v2_rows()))
    assert (w, h) == (16, 2)
    assert px[3] == (255, 128, 0)
    assert px[2] == BLACK and px[0] == BLACK
    assert px[16:] == [(255, 128, 0)] * 16


def test_v2_positional_colour_table_ignores_stored_indices():
    clut = {0: (0, 0, 0), 1: (0x1200, 0x3400, 0x5600), 2: (0xFF00, 0, 0)}
    rows = [bytes([0x01, 0x20]) + bytes(6), bytes(8)]
    pict = make_pict_v2_indexed(V2_FRAME, clut, rows, flags=0x8000)
    _, _, px = ads.decode_pict(pict)
    assert px[1] == (0x12, 0x34, 0x56)
    assert px[2] == (255, 0, 0)
    assert px[3] == BLACK


def test_v2_stored_indices_used_when_flag_clear():
    clut = {3: (0x1000, 0x2000, 0x3000), 9: (0xFF00, 0xFF00, 0xFF00)}
    rows = [bytes([0x39]) + bytes(7), bytes(8)]
    _, _, px = ads.decode_pict(make_pict_v2_indexed(V2_FRAME, clut, rows))
    assert px[0] == (0x10, 0x20, 0x30)
    assert px[1] == (255, 255, 255)


def test_unknown_opcode_raises_with_its_value():
    pict = pict_v2((0, 0, 2, 2), struct.pack(">H", 0x0031) + bytes(8))
    with pytest.raises(ads.ExtractError, match="0x0031"):
        ads.decode_pict(pict)


def test_pict_without_an_image_raises():
    pict = pict_v2((0, 0, 2, 2), struct.pack(">H", 0x00FF))
    with pytest.raises(ads.ExtractError, match="no image"):
        ads.decode_pict(pict)
