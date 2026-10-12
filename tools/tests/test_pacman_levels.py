import re
import shutil
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import pacman_levels as pl

ROOT = Path(__file__).resolve().parents[2]
PACMAN_LEVEL_C = ROOT / "firmware" / "src" / "hacks" / "pacman" / "pacman_level.c"
POOL_HEADER = ROOT / "firmware" / "src" / "hacks" / "pacman" / "pacman_pool.h"


def stdlevel() -> list[str]:
    text = PACMAN_LEVEL_C.read_text()
    body = text.split("static const lev_t stdlevel = {", 1)[1].split("};", 1)[0]
    return re.findall(r'"([^"]*)"', body)


def test_the_fixed_level_is_inside_the_circle():
    # Calibrates the predicate: the level upstream ships fits the glass.
    assert pl.outside_circle(stdlevel()) == []


def test_a_cell_in_the_corner_is_outside_the_circle():
    assert not pl.cell_visible(1, 1)
    assert not pl.cell_visible(38, 30)


def test_the_middle_of_the_maze_is_inside_the_circle():
    assert pl.cell_visible(20, 16)
    assert pl.cell_visible(19, 15)


def test_the_mask_is_left_right_symmetric():
    for y in range(pl.HEIGHT):
        for x in range(pl.WIDTH):
            assert pl.cell_visible(x, y) == pl.cell_visible(pl.WIDTH - 1 - x, y)


def test_a_dot_in_a_corner_is_reported():
    level = ["#" * pl.WIDTH for _ in range(pl.HEIGHT)]
    level[1] = "#`" + "#" * (pl.WIDTH - 2)
    assert pl.outside_circle(level) == [(1, 1)]


def test_an_asymmetric_level_is_rejected():
    level = stdlevel()
    level[6] = level[6][:20] + "#" + level[6][21:]
    assert any("symmetric" in problem for problem in pl.problems(level))


def test_a_level_with_too_few_dots_is_rejected():
    level = ["#" * pl.WIDTH for _ in range(pl.HEIGHT)]
    assert any("dots" in problem for problem in pl.problems(level))


def test_a_level_with_an_unreachable_dot_is_rejected():
    level = stdlevel()
    # Wall off a pocket: a lone dot cell in a block of wall.
    row = list(level[3])
    row[4] = "`"
    level[3] = "".join(row)
    assert any("connected" in problem for problem in pl.problems(level))


def test_the_fixed_level_passes_every_check_but_the_jail_is_its_own():
    assert pl.problems(stdlevel()) == []


def test_the_header_holds_every_level_in_order():
    a, b = stdlevel(), list(reversed(stdlevel()))
    text = pl.render_header([a, b])
    assert "PACMAN_POOL_SIZE 2" in text
    assert text.index(a[2]) < text.index(b[2])
    assert text.count('"') == 2 * pl.HEIGHT * 2


def test_a_malformed_level_is_refused():
    with pytest.raises(ValueError, match="rows"):
        pl.render_header([stdlevel()[:-1]])


needs_cc = pytest.mark.skipif(shutil.which("cc") is None, reason="no C compiler")


@needs_cc
def test_the_real_generator_makes_levels_that_pass(tmp_path):
    levels = pl.generate(count=2, seed=1, workdir=tmp_path)
    assert len(levels) == 2
    for level in levels:
        assert pl.problems(level) == []


@needs_cc
def test_the_same_seed_makes_the_same_levels(tmp_path):
    a = pl.generate(count=2, seed=7, workdir=tmp_path / "a")
    b = pl.generate(count=2, seed=7, workdir=tmp_path / "b")
    assert a == b


def test_the_committed_pool_is_valid_and_distinct():
    levels = pl.read_header(POOL_HEADER.read_text())
    assert len(levels) >= 8
    assert len({tuple(level) for level in levels}) == len(levels)
    assert stdlevel() not in levels
    for number, level in enumerate(levels):
        assert pl.problems(level) == [], f"pool level {number}"
