import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import ab_flash

LOG = """\
ESP-ROM:esp32s3-20210327
rotate -> CubicGrid heap=300000 psram=7000000
run CubicGrid fps=11.0 step=50.0ms push=40.0ms sync=0.0ms wait=0.0ms rows=466 heap=1 psram=2
glprof CubicGrid frames=55 clear=9.00ms list=30.00ms nvertex=8000
run CubicGrid fps=12.0 step=46.0ms push=33.0ms sync=0.0ms wait=0.0ms rows=466 heap=1 psram=2
glprof CubicGrid frames=60 clear=8.00ms list=28.00ms nvertex=8000
run Gears fps=99.0 step=1.0ms push=1.0ms sync=0.0ms wait=0.0ms rows=1 heap=1 psram=2
run CubicGrid fps=13.0 step=44.0ms push=31.0ms sync=0.0ms wait=0.0ms rows=465 heap=1 psram=2
"""


def test_a_log_gives_the_hacks_windows_in_order():
    cap = ab_flash.parse_log(LOG, "cubicgrid")
    assert [w["step"] for w in cap.runs] == [50.0, 46.0, 44.0]
    assert [w["list"] for w in cap.glprof] == [30.0, 28.0]
    assert cap.runs[0]["rows"] == 466


def test_the_first_window_is_left_out_of_the_mean():
    cap = ab_flash.parse_log(LOG, "CubicGrid")
    mean = ab_flash.summarise(cap.runs)
    assert mean["step"] == 45.0
    assert mean["push"] == 32.0
    assert ab_flash.summarise(cap.runs[:1]) == {}


def test_the_table_has_a_row_per_variant_and_round():
    cap = ab_flash.parse_log(LOG, "CubicGrid")
    table = ab_flash.render(
        {"base": [cap, cap], "iram": [cap]}, ["step", "list", "missing"]
    )
    rows = table.splitlines()
    assert rows[0] == "| Variant | Round | Windows | step | list | missing |"
    assert len(rows) == 5
    assert rows[2] == "| base | 1 | 2 | 45.00 | 28.00 | - |"
