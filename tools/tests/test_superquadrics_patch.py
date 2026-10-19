import re
import subprocess
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import superquadrics_patch as sp
import xs_vendor

ROOT = Path(__file__).resolve().parents[2]
UPSTREAM = xs_vendor.upstream(ROOT) / "hacks" / "glx" / "superquadrics.c"

SAMPLE = """\
static int
myrand(int range)
{
\treturn ((int) (((float) range) * LRAND() / (MAXRAND)));
}
"""


def test_the_random_index_is_clamped():
    out = sp.keep_myrand_in_range(SAMPLE)
    assert "return r < range ? r : range - 1;" in out
    assert "((float) range) * LRAND()" in out


def test_it_refuses_a_source_that_has_changed():
    with pytest.raises(ValueError, match="upstream"):
        sp.keep_myrand_in_range(SAMPLE.replace("(float) range", "(double) range"))


def _myrand_at_the_top_of_the_range(tmp_path: Path, source: str, range_: int) -> int:
    body = re.search(r"static int\nmyrand\(int range\)\n\{.*?\n\}\n", source, re.DOTALL)
    assert body
    program = (
        "#include <stdio.h>\n"
        "#define LRAND() 2147483647L\n"
        "#define MAXRAND (2147483648.0)\n"
        f"{body.group(0)}"
        f'int main(void) {{ printf("%d", myrand({range_})); return 0; }}\n'
    )
    c = tmp_path / "myrand.c"
    c.write_text(program)
    exe = tmp_path / "myrand"
    subprocess.run(["cc", "-o", str(exe), str(c)], check=True)
    return int(
        subprocess.run([str(exe)], check=True, capture_output=True, text=True).stdout
    )


@pytest.mark.skipif(not UPSTREAM.exists(), reason=f"needs {xs_vendor.UPSTREAM}")
@pytest.mark.parametrize("range_", [2, 3, 4, 100, 200])
def test_the_largest_random_value_stays_below_the_range(tmp_path, range_):
    patched = sp.keep_myrand_in_range(UPSTREAM.read_text())
    assert _myrand_at_the_top_of_the_range(tmp_path, patched, range_) == range_ - 1


@pytest.mark.skipif(not UPSTREAM.exists(), reason=f"needs {xs_vendor.UPSTREAM}")
def test_upstream_returns_the_range_itself_there(tmp_path):
    assert _myrand_at_the_top_of_the_range(tmp_path, UPSTREAM.read_text(), 4) == 4
