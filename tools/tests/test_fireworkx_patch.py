import difflib
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import fireworkx_patch as fp

ROOT = Path(__file__).resolve().parents[2]
UPSTREAM = ROOT / "vendor" / "xscreensaver-6.16" / "hacks" / "fireworkx.c"

SAMPLE = """\
#define PIXCOUNT 500              /* 500     */
#define POWDER 5.0                /* 5.0     */
#define FTWEAK 12                 /* 12      */
#define FLASH_ZOOM 0.8            /* 1.0     */
#define G_ACCELERATION 0.001      /* GRAVITY */
static int explode(void)
{
	if (fs->flies)
	{
		fp->x += fp->xv = fp->xv * air_drag + frand(0.1) - 0.05;
		fp->y += fp->yv = fp->yv * air_drag + frand(0.1) - 0.05 + G_ACCELERATION;
	}
	else
	{
		fp->x += fp->xv = fp->xv * air_drag + frand(0.01) - 0.005;
		fp->y += fp->yv = fp->yv * air_drag + frand(0.005) - 0.0025 + G_ACCELERATION;
	}
}
"""


def test_the_constants_that_are_pixel_lengths_are_scaled_by_the_render_size():
    out = fp.scale_for_a_small_window(SAMPLE)
    assert "#define POWDER (5.0 * FIREWORKX_SIZE)" in out
    assert "#define FLASH_ZOOM (0.8 * FIREWORKX_SIZE)" in out
    assert "#define G_ACCELERATION (0.001 * FIREWORKX_SIZE)" in out


def test_the_particle_count_comes_from_a_macro():
    out = fp.scale_for_a_small_window(SAMPLE)
    assert "#define PIXCOUNT FIREWORKX_PIXCOUNT" in out
    assert "#define PIXCOUNT 500" not in out


def test_every_velocity_jitter_term_is_scaled():
    out = fp.scale_for_a_small_window(SAMPLE)
    assert out.count("* FIREWORKX_SIZE;") + out.count("* FIREWORKX_SIZE +") == 4
    assert "(frand(0.1) - 0.05) * FIREWORKX_SIZE + G_ACCELERATION" in out
    assert "(frand(0.005) - 0.0025) * FIREWORKX_SIZE + G_ACCELERATION" in out
    assert "(frand(0.01) - 0.005) * FIREWORKX_SIZE;" in out


def test_nothing_else_changes():
    out = fp.scale_for_a_small_window(SAMPLE)
    changed = [
        line
        for line in difflib.unified_diff(
            SAMPLE.splitlines(), out.splitlines(), lineterm="", n=0
        )
        if line[0] in "+-" and line[:3] not in ("+++", "---")
    ]
    assert len(changed) == 2 * 8  # eight lines, each out and in


@pytest.mark.parametrize(
    "missing",
    [
        "#define POWDER 5.0",
        "#define FLASH_ZOOM 0.8",
        "#define G_ACCELERATION 0.001",
        "#define PIXCOUNT 500",
        "frand(0.01) - 0.005",
        "frand(0.005) - 0.0025",
    ],
)
def test_it_refuses_a_source_that_has_changed_upstream(missing):
    with pytest.raises(ValueError, match="upstream"):
        fp.scale_for_a_small_window(SAMPLE.replace(missing, "REMOVED"))


def test_it_refuses_a_term_that_appears_the_wrong_number_of_times():
    with pytest.raises(ValueError, match="upstream"):
        fp.scale_for_a_small_window(SAMPLE + "x = frand(0.1) - 0.05;\n")


@pytest.mark.skipif(not UPSTREAM.is_file(), reason="vendor/ is not fetched")
def test_the_real_upstream_file_patches_cleanly():
    out = fp.scale_for_a_small_window(UPSTREAM.read_text())
    assert out.count("FIREWORKX_SIZE") == 7
    assert "FIREWORKX_PIXCOUNT" in out
