import re
import struct
import sys
import zlib
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import teapot_table as tt

GLX = Path(__file__).resolve().parents[2] / "firmware" / "src" / "xs_support" / "glx"

SMALL = """/* A comment, with, commas 1.5 */

static const GLfloat teapot_triangles[] = {
	1.368074,2.435437,-0.227403,	1.381968,2.4,-0.229712,	1.4,2.4,0,
	-3,0.5,1e-3,	2,2,2,	0,0,0,
};
"""


def blob(text):
    body = text.split("teapot_triangles_z[] = {", 1)[1].split("};", 1)[0]
    return bytes(int(h, 16) for h in re.findall(r"0x([0-9a-f]{2})", body))


def test_parse_reads_only_the_table_numbers_in_order():
    assert tt.parse_floats(SMALL) == [
        1.368074, 2.435437, -0.227403, 1.381968, 2.4, -0.229712, 1.4, 2.4, 0.0,
        -3.0, 0.5, 0.001, 2.0, 2.0, 2.0, 0.0, 0.0, 0.0,
    ]  # fmt: skip


def test_the_stream_holds_the_floats_as_the_c_compiler_rounds_them():
    values = tt.parse_floats(SMALL)
    unpacked = zlib.decompress(blob(tt.render_header(values, "teapot2.h")))
    assert unpacked == struct.pack(f"<{len(values)}f", *values)


def test_the_header_states_the_count_and_renames_the_table():
    text = tt.render_header(tt.parse_floats(SMALL), "teapot2.h")
    assert "#define XSHIM_TEAPOT_FLOATS 18\n" in text
    assert "#define teapot_triangles (*xshim_teapot_table())" in text
    assert "teapot2.h" in text


def test_a_table_that_is_not_whole_triangles_is_an_error():
    with pytest.raises(ValueError, match="not a multiple of 9"):
        tt.render_header([1.0] * 10, "x")


def test_a_file_without_the_table_is_an_error():
    with pytest.raises(ValueError, match="teapot_triangles"):
        tt.parse_floats("static const GLfloat other[] = { 1, 2 };")


def test_the_real_table_round_trips_and_shrinks():
    values = tt.parse_floats((GLX / "teapot2.h").read_text())
    assert len(values) == 56880
    text = tt.render_header(values, "teapot2.h")
    assert zlib.decompress(blob(text)) == struct.pack("<56880f", *values)
    assert len(blob(text)) < 227520 // 4


def test_main_copies_teapot_c_unchanged_beside_the_new_header(tmp_path):
    out = tmp_path / "teapot"
    assert tt.main([str(GLX / "teapot.c"), str(GLX / "teapot2.h"), str(out)]) == 0
    assert (out / "teapot.c").read_bytes() == (GLX / "teapot.c").read_bytes()
    assert "XSHIM_TEAPOT_FLOATS 56880" in (out / "teapot2.h").read_text()


def test_main_leaves_unchanged_outputs_alone(tmp_path):
    out = tmp_path / "teapot"
    args = [str(GLX / "teapot.c"), str(GLX / "teapot2.h"), str(out)]
    tt.main(args)
    stamps = [(out / n).stat().st_mtime_ns for n in ("teapot.c", "teapot2.h")]
    tt.main(args)
    assert [(out / n).stat().st_mtime_ns for n in ("teapot.c", "teapot2.h")] == stamps
