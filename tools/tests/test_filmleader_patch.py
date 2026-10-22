import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import filmleader_patch as fp

ROOT = Path(__file__).resolve().parents[2]
FILMLEADER = ROOT / "firmware" / "src" / "hacks" / "filmleader" / "filmleader.c"

SAMPLE = """\
  {
    double r = 16/9.0;

# ifdef HAVE_MOBILE
    r = st->xgwa.width / (double) st->xgwa.height;
# endif

    st->w = 712;
    st->h = st->w / r;
  }
"""


def test_the_picture_is_square_and_the_width_is_the_wrappers():
    out = fp.square_canvas(SAMPLE)
    assert "double r = 1.0;" in out
    assert "st->w = FILMLEADER_CANVAS;" in out
    assert "712" not in out
    assert "16/9" not in out


def test_the_rest_of_the_source_is_left_alone():
    out = fp.square_canvas(SAMPLE)
    assert "st->h = st->w / r;" in out
    assert "# ifdef HAVE_MOBILE" in out


@pytest.mark.parametrize("old", [fp.OLD_ASPECT, fp.OLD_WIDTH])
def test_a_missing_line_fails_loudly(old):
    with pytest.raises(ValueError, match="has upstream changed"):
        fp.square_canvas(SAMPLE.replace(old, ""))


@pytest.mark.parametrize("old", [fp.OLD_ASPECT, fp.OLD_WIDTH])
def test_a_repeated_line_fails_loudly(old):
    with pytest.raises(ValueError, match="has upstream changed"):
        fp.square_canvas(SAMPLE + old)


@pytest.mark.skipif(not FILMLEADER.exists(), reason="filmleader.c is not copied yet")
def test_the_real_source_patches_cleanly():
    out = fp.square_canvas(FILMLEADER.read_text())
    assert "st->w = FILMLEADER_CANVAS;" in out


EXTENTS = """\
                  XftTextExtentsUtf8 (dpy, xftfont, (FcChar8 *)
                                      blurbs[i].s[0], strlen(blurbs[i].s[j]),
                                      &extents);
"""


def test_the_extents_call_measures_the_string_it_was_given_the_length_of():
    out = fp.measure_the_string_it_was_given(EXTENTS)
    assert "blurbs[i].s[0], strlen(blurbs[i].s[0])," in out
    assert "s[j]" not in out


@pytest.mark.parametrize("sample", ["", EXTENTS + EXTENTS])
def test_a_missing_or_repeated_extents_call_fails_loudly(sample):
    with pytest.raises(ValueError, match="has upstream changed"):
        fp.measure_the_string_it_was_given(sample)


@pytest.mark.skipif(not FILMLEADER.exists(), reason="filmleader.c is not copied yet")
def test_the_real_source_has_the_over_read_and_the_patch_removes_it():
    source = FILMLEADER.read_text()
    assert fp.OLD_EXTENTS in source
    assert fp.OLD_EXTENTS not in fp.measure_the_string_it_was_given(source)
