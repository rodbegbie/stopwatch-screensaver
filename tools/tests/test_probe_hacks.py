import shutil
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import probe_hacks as ph  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
SQUIRAL = ROOT / "firmware" / "src" / "hacks" / "squiral" / "squiral.c"
needs_cc = pytest.mark.skipif(shutil.which("cc") is None, reason="no C compiler")


def test_module_entry_reads_the_class_and_prefix():
    source = 'int x;\nXSCREENSAVER_MODULE ("Braid", braid)\n'
    assert ph.module_entry(source) == ("Braid", "braid")


def test_module_entry_ignores_a_commented_out_entry():
    source = '/* XSCREENSAVER_MODULE ("Old", old) */\nXSCREENSAVER_MODULE ("New", new)\n'
    assert ph.module_entry(source) == ("New", "new")


def test_module_2_is_named_by_its_second_argument():
    source = 'XSCREENSAVER_MODULE_2 ("Hopalong", hopalong, hop)\n'
    assert ph.module_entry(source) == ("Hopalong", "hopalong")


def test_a_hack_with_several_modules_is_skipped():
    source = 'XSCREENSAVER_MODULE ("A", a)\nXSCREENSAVER_MODULE ("B", b)\n'
    with pytest.raises(ph.ProbeError, match="several"):
        ph.module_entry(source)


def test_a_source_without_a_module_is_an_error():
    with pytest.raises(ph.ProbeError, match="XSCREENSAVER_MODULE"):
        ph.module_entry("int main(void) { return 0; }\n")


def test_registry_for_a_plain_hack_names_its_hack_entry():
    out = ph.registry_source("Pyro", "pyro", xlockmore=False)
    assert "extern const HackEntry pyro_hack;" in out
    assert "{&pyro_hack}" in out
    assert "g_hack_count = 1" in out
    assert "XLOCKMORE_HACK(pyro" not in out


def test_registry_for_an_xlockmore_hack_uses_the_macro():
    out = ph.registry_source("Braid", "braid", xlockmore=True)
    assert 'XLOCKMORE_HACK(braid, "Braid");' in out
    assert "{&braid_hack}" in out
    assert "extern const HackEntry braid_hack;" not in out


def test_stats_output_is_parsed():
    assert ph.parse_stats("Braid host_ms=1.6011\n") == pytest.approx(1.6011)


def test_unreadable_stats_output_is_an_error():
    with pytest.raises(ph.ProbeError, match="host_ms"):
        ph.parse_stats("segmentation fault\n")


@pytest.mark.parametrize(
    "ms, expected",
    [(0.0, "low"), (0.0149, "low"), (0.015, "medium"), (0.0499, "medium"),
     (0.05, "high"), (1.6, "high")],
)
def test_bands(ms, expected):
    assert ph.band(ms) == expected


def test_undefined_symbols_are_read_from_a_macos_link_error():
    stderr = (
        'Undefined symbols for architecture arm64:\n'
        '  "_XFooBar", referenced from:\n      _draw in a.o\n'
        '  "_helper_fn", referenced from:\n      _draw in a.o\n'
        'ld: symbol(s) not found for architecture arm64\n'
    )
    assert ph.undefined_symbols(stderr) == ["XFooBar", "helper_fn"]


def test_undefined_symbols_are_read_from_a_gnu_link_error():
    stderr = "a.o: in function `draw': undefined reference to `XFooBar'\n"
    assert ph.undefined_symbols(stderr) == ["XFooBar"]


@needs_cc
def test_squiral_is_probed_to_a_small_host_time():
    result = ph.probe("squiral", SQUIRAL.read_text(), ROOT)
    assert result["host_ms"] > 0
    assert ph.band(result["host_ms"]) == "low"


@needs_cc
def test_a_hack_that_calls_something_the_shim_lacks_does_not_link():
    source = SQUIRAL.read_text().replace(
        "XSCREENSAVER_MODULE", "void shim_lacks_this(void);\n"
        "void keep(void) { shim_lacks_this(); }\nXSCREENSAVER_MODULE", 1
    )
    result = ph.probe("squiral", source, ROOT)
    assert "does not link" in result["error"]
    assert "shim_lacks_this" in result["error"]


@needs_cc
def test_a_hack_that_does_not_compile_is_reported():
    result = ph.probe("squiral", SQUIRAL.read_text() + "\nthis is not C;\n", ROOT)
    assert "does not compile" in result["error"]
