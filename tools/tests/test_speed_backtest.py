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
    assert table[2].startswith("| pedal | fitted | 1.100 | high | 126-519 | ")
    assert table[3].startswith("| pyro | fitted | 0.005 | low | 0.8-1.1 | ")


def test_a_held_out_hack_is_marked_in_the_table():
    rows = sb.join_rows(["pyro"], {"pyro": 0.005}, {"pyro": (0.8, 1.1)})
    table = sb.render_table(rows, holdout={"pyro"}).splitlines()
    assert table[2].startswith("| pyro | held out | 0.005 | low | ")


def _rows():
    def full(name, host_ms, device, band):
        return {**row(name, host_ms, device, device), "device_mid": float(device),
                "band": band, "ratio": device / host_ms}

    return [
        full("a", 0.001, 1, "low"),
        full("b", 0.002, 2, "low"),
        full("c", 1.0, 300, "high"),
        full("miss", 0.0005, 900, "low"),
        full("fine", 0.07, 8, "high"),
    ]


def test_the_fit_figures_leave_the_held_out_hacks_out():
    text = sb.render(_rows(), holdout={"miss", "fine"})
    result = " ".join(text.split("## Result")[1].split("## Out of sample")[0].split())
    assert "correlation of 1.00." in result
    assert "slowest hack in the low band took 2.0 ms" in result


def test_the_held_out_hacks_get_their_own_section_with_the_misses():
    text = sb.render(_rows(), holdout={"miss", "fine"})
    held = " ".join(text.split("## Out of sample")[1].split("## Table")[0].split())
    assert "Missed: miss." in held
    assert "High band but under 50 ms on the device: fine." in held


def test_there_is_no_held_out_section_without_held_out_hacks():
    assert "## Out of sample" not in sb.render(_rows(), holdout=frozenset())


def _hack_source(root, name):
    path = root / "firmware" / "src" / "hacks" / name / f"{name}.c"
    path.parent.mkdir(parents=True)
    path.write_text(f"/* {name} */\n")


def test_a_shelved_hack_with_source_and_a_measured_step_is_probed(tmp_path):
    _hack_source(tmp_path, "celtic")
    seen = []

    def fake(name, source, root):
        seen.append((name, source, root))
        return {"host_ms": 1.1}

    got = sb.probe_shelved(["celtic"], {"celtic": (29.0, 1025.0)}, tmp_path, fake)
    assert got == {"celtic": 1.1}
    assert seen == [("celtic", "/* celtic */\n", tmp_path)]


def test_a_shelved_hack_without_a_measured_step_or_source_is_skipped(tmp_path):
    _hack_source(tmp_path, "unmeasured")

    def fake(name, source, root):
        raise AssertionError("must not be probed")

    got = sb.probe_shelved(
        ["unmeasured", "nosource"], {"nosource": (1.0, 2.0)}, tmp_path, fake
    )
    assert got == {}


def test_a_shelved_hack_that_cannot_be_probed_is_an_error(tmp_path):
    _hack_source(tmp_path, "celtic")

    def fake(name, source, root):
        return {"error": "does not link: XFoo"}

    with pytest.raises(sb.BacktestError, match="celtic: does not link"):
        sb.probe_shelved(["celtic"], {"celtic": (1.0, 2.0)}, tmp_path, fake)


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
