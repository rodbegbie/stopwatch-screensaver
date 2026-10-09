import stat
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import speed_backtest as sb  # noqa: E402

REGISTRY = """\
const HackEntry *const g_hacks[] = {
    &pyro_hack,     &hypercube_hack,
    &xspirograph_hack,
    &pedal_hack};
const int g_hack_count = 4;
"""


def row(name, host_ms, lo, hi):
    return {"name": name, "host_ms": host_ms, "device_lo": lo, "device_hi": hi}


def test_registry_names_keep_the_array_order():
    assert sb.registry_names(REGISTRY) == ["pyro", "hypercube", "xspirograph", "pedal"]


def test_spearman_of_a_monotonic_pair_is_one_and_reversed_is_minus_one():
    assert sb.spearman([1, 2, 3, 4], [10, 20, 35, 90]) == pytest.approx(1.0)
    assert sb.spearman([1, 2, 3, 4], [9, 7, 5, 1]) == pytest.approx(-1.0)


def test_spearman_shares_ranks_between_ties():
    assert sb.spearman([1, 1, 2, 2], [1, 1, 2, 2]) == pytest.approx(1.0)
    assert sb.spearman([1, 2, 3], [5, 5, 5]) == 0.0


def test_a_hack_without_a_measured_step_is_left_out():
    rows = sb.join_rows(
        ["pyro", "hypercube"], {"pyro": 0.005, "hypercube": 0.03}, {"pyro": (0.8, 1.1)}
    )
    assert [r["name"] for r in rows] == ["pyro"]
    assert rows[0]["device_mid"] == pytest.approx(0.95)
    assert rows[0]["band"] == "low"


def test_summary_reports_each_bands_slowest_hack_and_the_slow_set():
    rows = [
        {**row("pyro", 0.005, 1, 1), "device_mid": 1.0, "band": "low"},
        {**row("hop", 0.013, 13, 14), "device_mid": 13.5, "band": "low"},
        {**row("coral", 0.09, 15, 25), "device_mid": 20.0, "band": "high"},
        {**row("braid", 1.6, 95, 296), "device_mid": 195.5, "band": "high"},
        {**row("spiro", 0.03, 54, 56), "device_mid": 55.0, "band": "medium"},
    ]
    s = sb.summarise(rows)
    assert s["bands"]["low"] == {"count": 2, "max_device_ms": 13.5}
    assert s["bands"]["high"] == {"count": 2, "max_device_ms": 195.5}
    assert s["slow"] == ["braid", "spiro"]
    assert s["caught"] == ["braid"]
    assert s["missed"] == ["spiro"]


def test_the_table_is_slowest_first_with_the_ratio():
    rows = sb.join_rows(
        ["pyro", "pedal"],
        {"pyro": 0.005, "pedal": 1.1},
        {"pyro": (0.8, 1.1), "pedal": (126.0, 519.0)},
    )
    table = sb.render_table(rows).splitlines()
    assert table[0].startswith("| Hack |")
    assert table[2].startswith("| pedal | 1.100 | high | 126-519 | ")
    assert table[3].startswith("| pyro | 0.005 | low | 0.8-1.1 | ")


def _script(tmp_path, body):
    path = tmp_path / "program"
    path.write_text("#!/bin/sh\n" + body)
    path.chmod(path.stat().st_mode | stat.S_IXUSR)
    return path


def test_host_time_is_the_median_of_three_runs(tmp_path):
    counter = tmp_path / "n"
    counter.write_text("0")
    program = _script(
        tmp_path,
        f'n=$(cat {counter}); n=$((n+1)); echo $n > {counter}\n'
        'case $n in 1) v=0.9;; 2) v=0.2;; *) v=0.5;; esac\n'
        'echo "Fake host_ms=$v"\n',
    )
    assert sb.measure_host(program, 0) == pytest.approx(0.5)


def test_a_program_that_fails_is_an_error(tmp_path):
    program = _script(tmp_path, "exit 3\n")
    with pytest.raises(sb.BacktestError, match="stats 7 exited 3"):
        sb.measure_host(program, 7)
