import re
import subprocess
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import glmatrix_patch as gp
import xs_vendor

ROOT = Path(__file__).resolve().parents[2]
UPSTREAM = xs_vendor.upstream(ROOT) / "hacks" / "glx" / "glmatrix.c"

SAMPLE = """\
          int j = WAVE_SIZE - ((i + (GRID_SIZE - s->wave_position))
                               % WAVE_SIZE);
          brightness = mp->brightness_ramp[j];
"""


def test_the_wave_index_wraps_to_zero():
    out = gp.keep_the_wave_index_in_range(SAMPLE)
    assert "% WAVE_SIZE)) % WAVE_SIZE;" in out
    assert "brightness = mp->brightness_ramp[j];" in out


def test_it_refuses_a_source_that_has_changed():
    with pytest.raises(ValueError, match="upstream"):
        gp.keep_the_wave_index_in_range(SAMPLE.replace("GRID_SIZE", "GRID_WIDTH"))


def _wave_size(source: str) -> int:
    found = re.search(r"#define WAVE_SIZE\s+(\d+)", source)
    assert found
    return int(found[1])


def _wave_indexes(tmp_path: Path, source: str) -> list[int]:
    """Every index the hack's own statement gives, over each row of the grid and
    each wave position, by compiling that statement with the hack's constants."""
    wave = re.search(r"#define WAVE_SIZE\s+(\d+)", source)
    grid = re.search(r"#define GRID_SIZE\s+(\d+)", source)
    stmt = re.search(r"int j = \(?WAVE_SIZE - .*?;", source, re.DOTALL)
    assert wave and grid and stmt
    program = (
        "#include <stdio.h>\n"
        f"#define WAVE_SIZE {wave[1]}\n"
        f"#define GRID_SIZE {grid[1]}\n"
        "struct S { int wave_position; };\n"
        "int main(void) {\n"
        "  struct S st, *s = &st;\n"
        f"  for (int wp = 0; wp < WAVE_SIZE; wp++) {{ st.wave_position = wp;\n"
        "    for (int i = 0; i < GRID_SIZE; i++) {\n"
        f"      {stmt.group(0)}\n"
        '      printf("%d\\n", j);\n'
        "    } }\n"
        "  return 0; }\n"
    )
    c = tmp_path / "wave.c"
    c.write_text(program)
    exe = tmp_path / "wave"
    subprocess.run(["cc", "-o", str(exe), str(c)], check=True)
    out = subprocess.run([str(exe)], check=True, capture_output=True, text=True)
    return [int(x) for x in out.stdout.split()]


@pytest.mark.skipif(not UPSTREAM.exists(), reason=f"needs {xs_vendor.UPSTREAM}")
def test_patched_indexes_stay_inside_the_table(tmp_path):
    source = UPSTREAM.read_text()
    size = _wave_size(source)
    indexes = _wave_indexes(tmp_path, gp.keep_the_wave_index_in_range(source))
    assert min(indexes) == 0 and max(indexes) == size - 1


@pytest.mark.skipif(not UPSTREAM.exists(), reason=f"needs {xs_vendor.UPSTREAM}")
def test_upstream_reaches_one_past_the_table(tmp_path):
    source = UPSTREAM.read_text()
    size = _wave_size(source)
    assert max(_wave_indexes(tmp_path, source)) == size


def _run_the_pixel_packing(tmp_path: Path, source: str) -> subprocess.CompletedProcess:
    """The hack's own packing statement with the alpha of a bright glyph
    pixel, built with the undefined behaviour sanitizer."""
    stmt = re.search(r"p = \(r << rpos\).*?;", source, re.DOTALL)
    assert stmt
    program = (
        "#include <stdio.h>\n"
        "int main(void) {\n"
        "  int rpos = 0, gpos = 8, bpos = 16, apos = 24;\n"
        "  unsigned char r = 102, g = 255, b = 94, a = 246;\n"
        "  unsigned long p;\n"
        f"  {stmt.group(0)}\n"
        '  printf("%08lx", p & 0xFFFFFFFFul);\n'
        "  return 0; }\n"
    )
    c = tmp_path / "pack.c"
    c.write_text(program)
    exe = tmp_path / "pack"
    subprocess.run(
        [
            "cc",
            "-fsanitize=undefined",
            "-fno-sanitize-recover=all",
            "-o",
            str(exe),
            str(c),
        ],
        check=True,
    )
    return subprocess.run([str(exe)], check=False, capture_output=True, text=True)


@pytest.mark.skipif(not UPSTREAM.exists(), reason=f"needs {xs_vendor.UPSTREAM}")
def test_patched_packing_is_defined_and_gives_the_same_bits(tmp_path):
    patched = gp.shift_the_alpha_unsigned(UPSTREAM.read_text())
    done = _run_the_pixel_packing(tmp_path, patched)
    assert done.returncode == 0, done.stderr
    assert done.stdout == "f65eff66"


@pytest.mark.skipif(not UPSTREAM.exists(), reason=f"needs {xs_vendor.UPSTREAM}")
def test_upstream_packing_overflows_a_signed_int(tmp_path):
    done = _run_the_pixel_packing(tmp_path, UPSTREAM.read_text())
    assert done.returncode != 0
    assert "cannot be represented" in done.stderr


def test_the_alpha_shift_patch_refuses_a_source_that_has_changed():
    with pytest.raises(ValueError, match="upstream"):
        gp.shift_the_alpha_unsigned("p = (r << rpos) | (g << gpos);")
