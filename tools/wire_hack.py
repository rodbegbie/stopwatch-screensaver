"""Copies an xscreensaver hack into the firmware and registers it.

    uv run tools/wire_hack.py <name> <Class> "<copyright text>" \\
        [--notice "<licence column>"] [--xlockmore] [--glx]

Does the mechanical steps of `.claude/skills/port-hack/adding-a-hack.md`
(steps 2 and 3): copies `vendor/xscreensaver-6.16/hacks/<name>.c` to
`firmware/src/hacks/<name>/`, adds the registry entry (last in the button
order), the name to the expected list in `test_hacks.c`, a frame hash of `0`
(skipped until pinned: see "Batches" in that skill), a leak test, and the
THIRD_PARTY_NOTICES.md row. It does not touch `platformio.ini`, write a
wrapper, or fill a shim gap. Read the hack's licence first.

--xlockmore registers with `XLOCKMORE_HACK(name, "Class")`, for a hack built on
xlockmore.h (it has `MI_DISPLAY`); otherwise it expects the hack to define its
own `<name>_hack` through XSCREENSAVER_MODULE.
"""

import argparse
import re
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
UPSTREAM = "vendor/xscreensaver-6.16"


def _insert_before(text: str, marker: str, addition: str) -> str:
    i = text.find(marker)
    if i < 0:
        raise ValueError(f"cannot find `{marker}`")
    return text[:i] + addition + text[i:]


# The After Dark toasters stay last (built only with the disk image), so a new
# hack goes before this guard in each list that has it.
GUARD = "#ifdef HAVE_AFTER_DARK_TOASTERS\n"


def _guard_in(text: str, start: str, end: str) -> int:
    """Index of GUARD between `start` and `end`, or -1."""
    a = text.find(start)
    b = text.find(end, a)
    if a < 0 or b < 0:
        return -1
    return text.find(GUARD, a, b)


def wire(
    root: Path,
    name: str,
    cls: str,
    copyright_text: str,
    notice: str = "jwz permission notice (above)",
    xlockmore: bool = False,
    glx: bool = False,
) -> None:
    src = root / UPSTREAM / "hacks" / ("glx" if glx else "") / f"{name}.c"
    if not src.is_file():
        raise FileNotFoundError(src)
    dst = root / "firmware" / "src" / "hacks" / name / f"{name}.c"
    registry = root / "firmware" / "src" / "hacks" / "registry.c"
    tests = root / "firmware" / "test" / "test_hacks" / "test_hacks.c"
    notices = root / "THIRD_PARTY_NOTICES.md"

    reg = registry.read_text()
    if re.search(rf"\b{re.escape(name)}_hack\b", reg):
        raise ValueError(f"{name} is already in registry.c")
    if xlockmore:
        decl = f'XLOCKMORE_HACK({name}, "{cls}");\n'
    else:
        decl = f"extern const HackEntry {name}_hack;\n"
    reg = _insert_before(reg, "const HackEntry *const g_hacks[]", decl + "\n")
    g = _guard_in(reg, "g_hacks[] = {", "};")
    if g >= 0:
        reg = reg[:g] + f"    &{name}_hack,\n" + reg[g:]
    else:
        reg, n = re.subn(
            r"    &(\w+)_hack\};", rf"    &\1_hack,\n    &{name}_hack}};", reg, count=1
        )
        if n != 1:
            raise ValueError("cannot find the end of g_hacks[]")

    t = tests.read_text()
    g = _guard_in(t, "expected[] = {", "const int n = sizeof(expected)")
    if g >= 0:
        t = t[:g] + f'                                         "{cls}",\n' + t[g:]
    else:
        t, n = re.subn(
            r'"\};\n  const int n = sizeof\(expected\)',
            f'",\n                                         "{cls}"}};\n  const int n = sizeof(expected)',
            t,
            count=1,
        )
        if n != 1:
            raise ValueError("cannot find the expected list in test_hacks.c")
    g = _guard_in(
        t, "kBaseline[] = {", "void test_frames_of_every_hack_but_maze_match_main"
    )
    if g >= 0:
        t = t[:g] + f"    0, /* {cls}: not yet pinned */\n" + t[g:]
    else:
        t, n = re.subn(
            r"(\};\n\nvoid test_frames_of_every_hack_but_maze_match_main)",
            f"    0, /* {cls}: not yet pinned */\n\\1",
            t,
            count=1,
        )
        if n != 1:
            raise ValueError("cannot find kBaseline in test_hacks.c")
    leak = (
        f"void test_{name}_stops_do_not_leak(void) {{\n"
        f'  check_restarts_and_stops_do_not_leak("{cls}", {1 if xlockmore else 0});\n'
        "}\n\n"
    )
    t = _insert_before(t, "int main(void) {\n  UNITY_BEGIN();", leak)
    t = _insert_before(
        t,
        "  RUN_TEST(test_gears_stack_use_on_the_host_has_not_run_away);\n",
        f"  RUN_TEST(test_{name}_stops_do_not_leak);\n",
    )

    n_text = notices.read_text()
    rows = list(re.finditer(r"\| `firmware/src/hacks/[^\n]*\n", n_text))
    if not rows:
        raise ValueError("no hack rows in THIRD_PARTY_NOTICES.md")
    row = f"| `firmware/src/hacks/{name}/{name}.c` | {copyright_text} | {notice} |\n"
    n_text = n_text[: rows[-1].end()] + row + n_text[rows[-1].end() :]

    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy(src, dst)
    registry.write_text(reg)
    tests.write_text(t)
    notices.write_text(n_text)


def main(argv: list[str]) -> int:
    p = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    p.add_argument("name")
    p.add_argument("cls", metavar="Class")
    p.add_argument("copyright", metavar="copyright")
    p.add_argument("--notice", default="jwz permission notice (above)")
    p.add_argument("--xlockmore", action="store_true")
    p.add_argument("--glx", action="store_true", help="the source is under hacks/glx/")
    a = p.parse_args(argv)
    try:
        wire(ROOT, a.name, a.cls, a.copyright, a.notice, a.xlockmore, a.glx)
    except (ValueError, FileNotFoundError) as e:
        print(f"wire_hack: {e}", file=sys.stderr)
        return 1
    print(f"wired {a.name} ({a.cls}); now fill the shim gaps and run `pio run -e dump`")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
