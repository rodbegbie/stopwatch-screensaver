import difflib
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import pacman_patch as pp  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
PACMAN_LEVEL_C = ROOT / "firmware" / "src" / "hacks" / "pacman" / "pacman_level.c"

SAMPLE = """#include "pacman.h"
#include "pacman_level.h"

int
pacman_createnewlevel (pacmangamestruct *pp)
{
    if (NRAND (2) == 0) {

        do {
            clearlevel (level);
            nextstep (pp, level, 1, 2, dirvec, 1);
        } while (ret * 100 < (LEVWIDTH * LEVHEIGHT * MINDOTPERC));

        filllevel (level);
    }
    else {
        (void) memcpy (level, stdlevel, sizeof (lev_t));
        frmtlevel (pp, level);
    }
}
"""

POOL_PICK = "memcpy (level, pacman_pool[NRAND (PACMAN_POOL_SIZE)], sizeof (lev_t))"


def test_the_random_branch_picks_from_the_pool():
    out = pp.patch_pacman_level(SAMPLE)
    assert POOL_PICK in out


def test_the_choice_between_pool_and_fixed_level_is_upstreams():
    out = pp.patch_pacman_level(SAMPLE)
    assert "if (NRAND (2) == 0) {" in out
    assert out.index("NRAND (2) == 0") < out.index(POOL_PICK) < out.index("else {")


def test_the_generator_loop_never_runs():
    out = pp.patch_pacman_level(SAMPLE)
    assert "if (0) do {" in out
    assert out.index("if (0) do {") > out.index(POOL_PICK)


def test_the_pool_header_is_included_after_the_level_header():
    out = pp.patch_pacman_level(SAMPLE)
    assert '#include "pacman_level.h"\n#include "pacman_pool.h"' in out


def test_the_fixed_level_branch_is_kept():
    out = pp.patch_pacman_level(SAMPLE)
    assert "memcpy (level, stdlevel, sizeof (lev_t));" in out


def test_the_rest_of_the_branch_is_kept():
    out = pp.patch_pacman_level(SAMPLE)
    assert "filllevel (level);" in out
    assert "frmtlevel (pp, level);" in out


def test_nothing_else_changes():
    out = pp.patch_pacman_level(SAMPLE)
    changed = [
        line
        for line in difflib.unified_diff(
            SAMPLE.splitlines(), out.splitlines(), lineterm="", n=0
        )
        if line[0] in "+-" and line[:3] not in ("+++", "---")
    ]
    assert changed == [
        '+#include "pacman_pool.h"',
        "-        do {",
        "+        " + POOL_PICK + ";",
        "+        if (0) do {",
    ]


def test_a_missing_generator_loop_is_an_error():
    with pytest.raises(ValueError, match="do {"):
        pp.patch_pacman_level(SAMPLE.replace("clearlevel (level);", "other (level);"))


def test_two_generator_loops_are_an_error():
    with pytest.raises(ValueError, match="found 2"):
        pp.patch_pacman_level(SAMPLE + SAMPLE)


def test_a_missing_include_is_an_error():
    with pytest.raises(ValueError, match="pacman_level.h"):
        pp.patch_pacman_level(SAMPLE.replace("pacman_level.h", "other.h"))


def test_a_missing_fixed_level_branch_is_an_error():
    with pytest.raises(ValueError, match="stdlevel"):
        pp.patch_pacman_level(SAMPLE.replace("stdlevel", "other"))


def test_the_copied_source_patches_cleanly():
    out = pp.patch_pacman_level(PACMAN_LEVEL_C.read_text())
    assert POOL_PICK in out
    assert "if (NRAND (2) == 0) {" in out


PACMAN_AI_C = ROOT / "firmware" / "src" / "hacks" / "pacman" / "pacman_ai.c"


def _real_ai() -> str:
    return PACMAN_AI_C.read_text()


def test_the_recursive_route_search_is_replaced():
    out = pp.patch_pacman_ai(_real_ai())
    assert "static int\nrecur_back_track (" in out
    assert "recur_back_track ( pp, g, new_row, new_col )" not in out
    assert "bt_frame" in out


def test_the_replacement_keeps_name_and_signature():
    out = pp.patch_pacman_ai(_real_ai())
    assert (
        "recur_back_track ( pacmangamestruct * pp, ghoststruct *g, int row, int col )"
        in out
    )
    assert "recur_back_track ( pp, tmp_ghost, r, c )" in out  # find_home's call


def test_nothing_outside_the_function_changes():
    src = _real_ai()
    out = pp.patch_pacman_ai(src)
    head = src[: src.index("static int\nrecur_back_track (")]
    assert out.startswith(head)
    assert out.endswith(src[src.index("static void\nfind_home") :])


def test_a_missing_function_is_an_error():
    src = _real_ai().replace("recur_back_track (", "other_search (")
    with pytest.raises(ValueError, match="recur_back_track"):
        pp.patch_pacman_ai(src)


def test_a_changed_body_is_an_error():
    src = _real_ai().replace("pos_down, &new_row", "pos_right, &new_row", 1)
    assert src != _real_ai()
    with pytest.raises(ValueError, match="changed"):
        pp.patch_pacman_ai(src)


def test_two_copies_are_an_error():
    src = _real_ai()
    a = src.index("static int\nrecur_back_track (")
    b = src.index("static void\nfind_home")
    with pytest.raises(ValueError, match="found 2"):
        pp.patch_pacman_ai(src[:b] + src[a:b] + src[b:])
