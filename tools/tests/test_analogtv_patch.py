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
ANALOGTV = ROOT / "firmware" / "src" / "hacks" / "analogtv" / "analogtv.c"


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


def _draw_lines(source: str) -> str:
    start = source.index("static void analogtv_thread_draw_lines(")
    return source[start : source.index("\n}\n", start) + 3]


def test_the_line_loop_steps_by_the_wrappers_variable():
    out = _draw_lines(ap.step_the_line_loop(ANALOGTV.read_text()))
    assert "lineno += it->threads.count * analogtv_line_step) {" in out
    assert "lineno += it->threads.count) {" not in out


def test_a_stepped_line_writes_its_colours_into_the_rows_of_the_lines_it_skips():
    out = _draw_lines(ap.step_the_line_loop(ANALOGTV.read_text()))
    tail = out.split("for (k = 1; k < analogtv_line_step")[1]
    assert "lineno + k < ANALOGTV_BOT" in tail
    # Each skipped line is drawn at its own height, not as one tall line.
    assert "analogtv_blast_imagerow(it, raw_rgb_start, raw_rgb_end,\n" in tail
    assert "ytop2, ybot2" in tail


def test_the_first_blast_of_a_line_is_still_its_own_rows():
    out = _draw_lines(ap.step_the_line_loop(ANALOGTV.read_text()))
    assert out.index("ytop,ybot);") < out.index("ytop2, ybot2")


def test_each_drawn_line_is_counted_for_the_tests():
    out = _draw_lines(ap.step_the_line_loop(ANALOGTV.read_text()))
    assert out.count("analogtv_lines_drawn++;") == 1
    assert out.index("analogtv_lines_drawn++;") > out.index("analogtv_get_line(")


def test_only_the_line_loop_changes_when_the_step_is_patched():
    before = ANALOGTV.read_text()
    after = ap.step_the_line_loop(before)
    assert after.replace(_draw_lines(after), "") == before.replace(
        _draw_lines(before), ""
    )


def test_the_two_patches_apply_together_in_either_order():
    source = ANALOGTV.read_text()
    a = ap.step_the_line_loop(ap.float_ghost_filter(source))
    b = ap.float_ghost_filter(ap.step_the_line_loop(source))
    assert a == b


def test_a_changed_line_loop_fails_loudly():
    with pytest.raises(ValueError, match="has upstream changed"):
        ap.step_the_line_loop("static void other(void) {}\n")


def test_a_second_line_loop_fails_loudly():
    with pytest.raises(ValueError, match="found 2"):
        ap.step_the_line_loop(ANALOGTV.read_text() + ANALOGTV.read_text())


def _load_ximage(source: str) -> str:
    start = source.index("int\nanalogtv_load_ximage(")
    return source[start : source.index("\n}\n", start) + 3]


def test_load_ximage_keeps_no_row_buffers_and_reads_each_pixel_as_it_goes():
    out = _load_ximage(ap.load_ximage_without_row_buffers(ANALOGTV.read_text()))
    for name in ("col1", "col2", "mask"):
        assert f"{name}[" not in out
    assert "malloc" not in out
    assert "XColor col1, col2;" in out
    assert out.count("xshim_query_rgb565(") == 2
    assert "XQueryColor" not in out
    assert "if (!pic_im) return 0;" in out


def test_only_load_ximage_changes_when_its_row_buffers_go():
    before = ANALOGTV.read_text()
    after = ap.load_ximage_without_row_buffers(before)
    assert after.replace(_load_ximage(after), "") == before.replace(
        _load_ximage(before), ""
    )


def test_a_changed_load_ximage_fails_loudly():
    with pytest.raises(ValueError, match="has upstream changed"):
        ap.load_ximage_without_row_buffers("int\nanalogtv_load_ximage(void) {}\n")


def test_a_second_load_ximage_fails_loudly():
    with pytest.raises(ValueError, match="found 2"):
        ap.load_ximage_without_row_buffers(ANALOGTV.read_text() + ANALOGTV.read_text())


def test_the_three_patches_apply_together():
    out = ap.load_ximage_without_row_buffers(
        ap.step_the_line_loop(ap.float_ghost_filter(ANALOGTV.read_text()))
    )
    assert (
        "ghostfir0" in out
        and "analogtv_line_step" in out
        and "XColor col1, col2;" in out
    )


@pytest.mark.skipif(not shutil.which("cc"), reason="needs a C compiler")
def test_load_ximage_gives_the_same_signal_and_a_small_stack_frame(tmp_path):
    """Run the real analogtv_load_ximage, as copied and as patched, on the same
    picture (with and without a mask) and compare the signal it writes. The
    copied one keeps about 28 KB of colours on the stack, which overflowed the
    board's 16 KB loop stack, and filling and reading them back cost half the
    time of a picture; the patched one must keep under 4 KB."""
    source = ANALOGTV.read_text()
    old = _load_ximage(source).replace("analogtv_load_ximage", "load_old")
    new = _load_ximage(ap.load_ximage_without_row_buffers(source)).replace(
        "analogtv_load_ximage", "load_new"
    )
    harness = textwrap.dedent(
        """\
        #include <math.h>
        #include <stdio.h>
        #include <stdlib.h>
        #include <string.h>
        #define ANALOGTV_PIC_LEN 582
        #define ANALOGTV_VISLINES 200
        #define ANALOGTV_SCALE 1
        #define ANALOGTV_TOP 30
        #define ANALOGTV_PIC_START 60
        #define ANALOGTV_BLACK_LEVEL 10
        #define ANALOGTV_V 262
        #define ANALOGTV_H 912
        typedef struct { unsigned long pixel; unsigned short red, green, blue; char flags, pad; } XColor;
        typedef struct { int width, height; char *data; int bits_per_pixel, bytes_per_line; } XImage;
        typedef struct { signed char signal[ANALOGTV_V+1][ANALOGTV_H]; } analogtv_input;
        typedef struct { void *dpy; int colormap; struct { int width, height; } xgwa; } analogtv;
        static unsigned long XGetPixel(XImage *im, int x, int y) {
          if (x < 0 || y < 0 || x >= im->width || y >= im->height) return 0;
          return ((unsigned int *)(im->data + (size_t)y * im->bytes_per_line))[x];
        }
        static int XQueryColor(void *d, int cm, XColor *c) {
          (void)d; (void)cm;
          const unsigned long p = c->pixel;
          c->red = (unsigned short)((p & 0xFF) * 257);
          c->green = (unsigned short)(((p >> 8) & 0xFF) * 257);
          c->blue = (unsigned short)(((p >> 16) & 0xFF) * 257);
          return 1;
        }
        static void XQueryColors(void *d, int cm, XColor *c, int n) {
          for (int i = 0; i < n; i++) XQueryColor(d, cm, &c[i]);
        }
        static void xshim_query_rgb565(XColor *c) { XQueryColor(0, 0, c); }
        %s
        %s
        int main(void) {
          /* One row spare: the last scanline's second sample can fall just past the picture. */
          static unsigned int pic[97 * 62], msk[97 * 62];
          static analogtv_input a, b;
          srandom(5);
          for (int i = 0; i < 97 * 62; i++) { pic[i] = (unsigned int)(random() & 0xFFFFFF); msk[i] = (random() %% 4) != 0; }
          XImage im = {97, 61, (char *)pic, 32, 97 * 4}, mm = {97, 61, (char *)msk, 32, 97 * 4};
          XImage im16 = {97, 61, (char *)pic, 16, 97 * 4};
          analogtv it = {0, 0, {640, 480}};
          int differ = 0;
          for (int variant = 0; variant < 4; variant++) {
            memset(&a, 0, sizeof a); memset(&b, 0, sizeof b);
            XImage *mask = (variant & 1) ? &mm : NULL;
            const int xoff = (variant & 2) ? 40 : 0, tw = (variant & 2) ? 130 : 0;
            const int ra = load_old(&it, (analogtv_input *)&a, &im, mask, xoff, xoff, tw, tw);
            const int rb = load_new(&it, (analogtv_input *)&b, &im, mask, xoff, xoff, tw, tw);
            differ += ra != rb || memcmp(&a, &b, sizeof a) != 0;
          }
          /* A picture that is not 32 bits a pixel goes through XGetPixel. */
          memset(&a, 0, sizeof a); memset(&b, 0, sizeof b);
          const int r16a = load_old(&it, (analogtv_input *)&a, &im16, NULL, 0, 0, 0, 0);
          const int r16b = load_new(&it, (analogtv_input *)&b, &im16, NULL, 0, 0, 0, 0);
          differ += r16a != r16b || memcmp(&a, &b, sizeof a) != 0;
          /* With no picture it loads nothing and says so. */
          memset(&b, 0, sizeof b);
          differ += load_new(&it, (analogtv_input *)&b, NULL, NULL, 0, 0, 0, 0) != 0;
          for (size_t i = 0; i < sizeof b; i++) differ += ((signed char *)&b)[i] != 0;
          printf("%%d\\n", differ);
          return 0;
        }
        """
    ) % (old, new)
    src = tmp_path / "h.c"
    src.write_text(harness)
    exe = tmp_path / "h"
    subprocess.run(
        ["cc", "-O1", "-fsanitize=address", "-o", str(exe), str(src), "-lm"], check=True
    )
    # The frame sizes come from a plain build: AddressSanitizer moves arrays off
    # the stack.
    subprocess.run(
        ["cc", "-O1", "-fstack-usage", "-c", str(src), "-o", str(tmp_path / "h.o")],
        check=True,
        cwd=tmp_path,
    )
    ran = subprocess.run([str(exe)], check=False, capture_output=True, text=True)
    assert ran.returncode == 0, ran.stderr[:2000]
    assert ran.stdout.split() == ["0"]
    frames = {}
    for line in (tmp_path / "h.su").read_text().splitlines():
        parts = line.split("\t")
        frames[parts[0].split(":")[-1]] = int(parts[1])
    assert frames["load_old"] > 8000
    assert frames["load_new"] < 4096
