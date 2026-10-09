import difflib
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import pacman_patch as pp  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
PACMAN_LEVEL_C = ROOT / "firmware" / "src" / "hacks" / "pacman" / "pacman_level.c"

SAMPLE = """int
pacman_createnewlevel (pacmangamestruct *pp)
{
    if (NRAND (2) == 0) {
        do {
            nextstep (pp, level, 1, 2, dirvec, 1);
        } while (ret * 100 < (LEVWIDTH * LEVHEIGHT * MINDOTPERC));
    }
    else {
        (void) memcpy (level, stdlevel, sizeof (lev_t));
        frmtlevel (pp, level);
    }
}
"""


def test_the_random_branch_is_switched_off():
    out = pp.patch_pacman_level(SAMPLE)
    assert "if (0) {" in out
    assert "NRAND (2)" not in out


def test_the_fixed_level_branch_is_kept():
    out = pp.patch_pacman_level(SAMPLE)
    assert "memcpy (level, stdlevel, sizeof (lev_t));" in out


def test_nothing_else_changes():
    out = pp.patch_pacman_level(SAMPLE)
    changed = [
        line
        for line in difflib.unified_diff(
            SAMPLE.splitlines(), out.splitlines(), lineterm="", n=0
        )
        if line[0] in "+-" and line[:3] not in ("+++", "---")
    ]
    assert len(changed) == 2  # one line out, one line in


def test_a_missing_test_is_an_error():
    with pytest.raises(ValueError, match="NRAND"):
        pp.patch_pacman_level(SAMPLE.replace("if (NRAND (2) == 0) {", "if (1) {"))


def test_two_matches_are_an_error():
    with pytest.raises(ValueError, match="found 2"):
        pp.patch_pacman_level(SAMPLE + SAMPLE)


def test_a_missing_fixed_level_branch_is_an_error():
    with pytest.raises(ValueError, match="stdlevel"):
        pp.patch_pacman_level(SAMPLE.replace("stdlevel", "other"))


def test_the_copied_source_patches_cleanly():
    out = pp.patch_pacman_level(PACMAN_LEVEL_C.read_text())
    assert out.count("if (0) {") >= 1
    assert "NRAND (2) == 0" not in out
