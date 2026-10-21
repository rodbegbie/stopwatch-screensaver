import re
import shutil
import subprocess
import sys
import textwrap
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import analogtv_patch as ap

ROOT = Path(__file__).resolve().parents[2]
ANALOGTV = ROOT / "firmware" / "src" / "hacks" / "pong" / "analogtv.c"


def _add_signal(source: str) -> str:
    """The text of analogtv_add_signal in `source`."""
    start = source.index("static void analogtv_add_signal(")
    end = source.index("\n}\n", start) + 3
    return source[start:end]


def test_the_ghost_filter_taps_are_read_once_as_floats():
    out = _add_signal(ap.float_ghost_filter(ANALOGTV.read_text()))
    assert out.count("rec->ghostfir[") == 4
    assert "float ghostfir0=rec->ghostfir[0]" in out
    assert "dp[1]*ghostfir0" in out and "dp[4]*ghostfir3" in out


def test_only_add_signal_changes():
    before = ANALOGTV.read_text()
    after = ap.float_ghost_filter(before)
    assert after.replace(_add_signal(after), "") == before.replace(
        _add_signal(before), ""
    )


def test_a_changed_upstream_function_fails_loudly():
    with pytest.raises(ValueError, match="has upstream changed"):
        ap.float_ghost_filter("static void other(void) {}\n")


def test_a_second_copy_fails_loudly():
    with pytest.raises(ValueError, match="found 2"):
        ap.float_ghost_filter(ANALOGTV.read_text() + ANALOGTV.read_text())


def test_the_copied_source_still_has_the_lines_the_patch_replaces():
    ap.float_ghost_filter(ANALOGTV.read_text())


@pytest.mark.skipif(not shutil.which("cc"), reason="needs a C compiler")
def test_the_float_filter_matches_the_double_one_at_every_magnitude(tmp_path):
    """Run the real analogtv_add_signal, as copied and as patched, on the same
    signal. A tap is double in the struct, so the old loop multiplied in double;
    the new one in float. The results must agree to float's own precision, at
    small, middling and huge levels and tap sizes."""
    source = ANALOGTV.read_text()
    rnd = source[
        source.index("static void rnd_combine") : source.index(
            "static void analogtv_init_signal"
        )
    ]
    old = _add_signal(source).replace("analogtv_add_signal", "add_old")
    new = _add_signal(ap.float_ghost_filter(source)).replace(
        "analogtv_add_signal", "add_new"
    )
    harness = textwrap.dedent(
        """\
        #include <limits.h>
        #include <math.h>
        #include <stdio.h>
        #include <stdlib.h>
        #include <string.h>
        #include <assert.h>
        #define FASTRND_A 1103515245
        #define FASTRND_C 12345
        enum { ANALOGTV_SIGNAL_LEN = 262 * 912 };
        typedef struct { signed char signal[262][912]; } analogtv_input;
        typedef struct {
          analogtv_input *input;
          double ofs, level, multipath, freqerr;
          double ghostfir[4], ghostfir2[4];
          double hfloss, hfloss2;
        } analogtv_reception;
        typedef struct { float *rx_signal; unsigned random1; } analogtv;
        %s
        %s
        %s
        int main(void) {
          static analogtv_input input;
          srandom(11);
          for (int y = 0; y < 262; y++)
            for (int x = 0; x < 912; x++) input.signal[y][x] = (signed char)(random() %% 120 - 20);
          const double levels[] = {0.001, 0.5, 2.0, 40.0, 1.0e4};
          const double taps[] = {0.0, 0.01, -0.02, 0.2, -0.8, 3.0};
          static float a[ANALOGTV_SIGNAL_LEN + 2048], b[ANALOGTV_SIGNAL_LEN + 2048];
          int checked = 0;
          double worst = 0;
          for (unsigned li = 0; li < 5; li++)
            for (unsigned ti = 0; ti < 6; ti++) {
              analogtv_reception rec = {&input, (double)(random() %% 5000), levels[li], 0, 0,
                                        {taps[ti], -taps[ti] / 2, taps[(ti + 1) %% 6], 0.04},
                                        {0}, 0.0123 * (li + 1), 0};
              analogtv it = {a, (unsigned)random()};
              analogtv jt = {b, it.random1};
              const unsigned start = 2048 * (unsigned)(random() %% 100);
              const unsigned end = start + 2048;
              for (unsigned i = 0; i < 2048; i++) a[start + i] = b[start + i] = (float)(i %% 7) - 3.0f;
              add_old(&it, &rec, start, end, 0);
              add_new(&jt, &rec, start, end, 0);
              for (unsigned i = start; i < end; i++) {
                const double d = fabs((double)a[i] - (double)b[i]);
                const double scale = fmax(1.0, fabs((double)a[i]));
                if (d / scale > worst) worst = d / scale;
                checked++;
              }
            }
          printf("%%d %%.3e\\n", checked, worst);
          return 0;
        }
        """
    ) % (rnd, old, new)
    src = tmp_path / "h.c"
    src.write_text(harness)
    exe = tmp_path / "h"
    subprocess.run(["cc", "-O1", "-o", str(exe), str(src), "-lm"], check=True)
    checked, worst = subprocess.run(
        [str(exe)], check=True, capture_output=True, text=True
    ).stdout.split()
    assert int(checked) == 30 * 2048
    assert float(worst) < 1e-4
    assert re.fullmatch(r"[0-9.e+-]+", worst)
