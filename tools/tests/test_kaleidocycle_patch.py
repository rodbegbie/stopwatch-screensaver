import re
import subprocess
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import kaleidocycle_patch as kp
import xs_vendor

ROOT = Path(__file__).resolve().parents[2]
UPSTREAM = xs_vendor.upstream(ROOT) / "hacks" / "glx" / "kaleidocycle.c"

SAMPLE = """\
      glPushMatrix();
      glRotatef ((i/2) * 4 * 180 / bp->count, 0, 0, 1);
      if (flip_p) reflect (-sin(a), cos(a), 0);
"""


def test_the_count_is_guarded():
    out = kp.guard_the_zero_count(SAMPLE)
    assert "/ (bp->count > 0 ? bp->count : 1), 0, 0, 1);" in out
    assert "if (flip_p) reflect" in out


def test_it_refuses_a_source_that_has_changed():
    with pytest.raises(ValueError, match="upstream"):
        kp.guard_the_zero_count(SAMPLE.replace("180", "360"))


def _first_angle_at_count_zero(tmp_path: Path, source: str) -> str:
    """The angle the hack's own glRotatef call gets for piece 0 at count 0."""
    call = re.search(r"glRotatef \(\(i/2\).*?, 0, 0, 1\);", source)
    assert call
    program = (
        "#include <stdio.h>\n"
        "static void glRotatef(float a, float x, float y, float z) {\n"
        '  printf("%f", a); (void)x; (void)y; (void)z; }\n'
        "struct B { float count; };\n"
        "int main(void) { struct B b = {0}, *bp = &b; int i = 0;\n"
        f"  {call.group(0)}\n"
        "  return 0; }\n"
    )
    c = tmp_path / "angle.c"
    c.write_text(program)
    exe = tmp_path / "angle"
    subprocess.run(["cc", "-o", str(exe), str(c)], check=True)
    return subprocess.run([str(exe)], check=True, capture_output=True, text=True).stdout


@pytest.mark.skipif(not UPSTREAM.exists(), reason=f"needs {xs_vendor.UPSTREAM}")
def test_patched_first_angle_is_a_number(tmp_path):
    patched = kp.guard_the_zero_count(UPSTREAM.read_text())
    assert _first_angle_at_count_zero(tmp_path, patched) == "0.000000"


@pytest.mark.skipif(not UPSTREAM.exists(), reason=f"needs {xs_vendor.UPSTREAM}")
def test_upstream_first_angle_is_not_a_number(tmp_path):
    assert "nan" in _first_angle_at_count_zero(tmp_path, UPSTREAM.read_text()).lower()
