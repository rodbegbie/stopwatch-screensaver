import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import triage_hacks as th

LOG = """\
boot noise
run Vines fps=2.0 step=180.0ms push=3.0ms sync=0.0ms wait=190.0ms rows=80
run Vines fps=2.6 step=182.0ms push=1.0ms sync=0.0ms wait=180.0ms rows=80
run Vines fps=2.8 step=184.0ms push=1.0ms sync=0.0ms wait=178.0ms rows=80
run Gears fps=5.0 step=30.0ms push=40.0ms sync=0.0ms wait=0.0ms rows=466
run Gears fps=6.0 step=40.0ms push=30.0ms sync=0.0ms wait=0.0ms rows=466
run Gears fps=8.0 step=60.0ms push=30.0ms sync=0.0ms wait=0.0ms rows=466
run Pyro fps=30.0 step=1.0ms push=31.0ms sync=0.0ms wait=0.0ms rows=466
"""


def test_it_collects_windows_per_hack_and_ignores_other_lines():
    runs = th.parse(LOG)
    assert sorted(runs) == ["Gears", "Pyro", "Vines"]
    assert [w["fps"] for w in runs["Vines"]] == [2.0, 2.6, 2.8]


def test_a_longer_wait_than_step_is_paced():
    assert th.classify(step=10, push=3, wait=11) == "paced"


def test_a_longer_push_than_step_is_push_bound():
    assert th.classify(step=10, push=31, wait=0) == "push-bound"


def test_otherwise_the_step_is_the_bottleneck():
    assert th.classify(step=50, push=31, wait=0) == "step-bound"


def test_equal_step_and_wait_is_not_paced():
    assert th.classify(step=10, push=3, wait=10) == "step-bound"


def test_a_push_longer_than_the_wait_is_not_paced():
    assert th.classify(step=10, push=31, wait=20) == "push-bound"


def test_the_first_window_is_left_out():
    rows = {r.hack: r for r in th.summarise(th.parse(LOG))}
    assert rows["Vines"].fps == 2.7
    assert rows["Vines"].worst_fps == 2.6
    assert rows["Gears"].step == 50.0


def test_a_hack_with_one_window_keeps_it():
    rows = {r.hack: r for r in th.summarise(th.parse(LOG))}
    assert rows["Pyro"].fps == 30.0


def test_step_bound_hacks_come_first_slowest_step_first():
    out = th.render(th.summarise(th.parse(LOG)), None).splitlines()
    names = [line.split("|")[1].strip() for line in out[2:]]
    assert names == ["Vines", "Gears", "Pyro"]


def test_equal_step_and_push_is_step_bound():
    assert th.classify(step=10, push=10, wait=0) == "step-bound"


def test_a_name_inside_another_hacks_object_does_not_count():
    assert th.soft_for("Gears", {"gears_gl": 5, "hypergears": 9}) == 5


def test_soft_counts_add_every_object_that_starts_with_the_hack_name():
    counts = {"xspirograph_single": 4, "xspirograph": 40, "vines_single": 2}
    assert th.soft_for("XSpirograph", counts) == 44
    assert th.soft_for("Vines", counts) == 2
