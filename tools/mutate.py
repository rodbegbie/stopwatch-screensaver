"""Mutation-checks a file: breaks it one way at a time, runs the tests, and
reports whether they noticed.

    uv run tools/mutate.py <file> '<old>=><new>' ['<old>=><new>' ...] \\
        [--cmd 'pio test -e native -f test_float_polygon'] [--cwd firmware]

Each mutation replaces the first `<old>` in the file with `<new>`. A mutation is

- CAUGHT if the command fails with a test failure, a sanitizer report or a
  crash,
- BUILD ERROR if it fails without one (the mutation broke the compile: it proves
  nothing, so write a smaller one),
- SURVIVED if the command passes: the tests cannot tell the code from the
  broken code. Either a test is missing, or the mutation is equivalent (it
  changes nothing observable: say so rather than adding a test).

The file is always restored, and the run says so. Use `source tools/env.sh`
first for the default `pio` command. A multi-line mutation can be written with
real newlines inside the quotes.
"""

import argparse
import os
import re
import shlex
import subprocess
import sys
from pathlib import Path

BUILD_FAILURE = re.compile(
    r"\berror:|Building stage has failed|^\*\*\* \[.*\] Error \d+"
)
FAILURE_MARKERS = (
    "[FAILED]",
    "Sanitizer",
    "FAILED",
    "AssertionError",
    "Program received signal",  # a test crashed (ASan aborts print this under pio)
)


def run_mutations(
    path: Path, mutations: list[tuple[str, str]], cmd: list[str], cwd: Path
):
    """Returns [(old, status, first_failure_line)] and leaves the file as it was."""
    original = path.read_text()
    # Every pattern is checked before the first run: a typo in the last one
    # must not cost the minutes the earlier ones take.
    for old, _ in mutations:
        if old not in original:
            raise ValueError(f"`{old[:60]}` is not in {path}")
    results = []
    try:
        for old, new in mutations:
            path.write_text(original.replace(old, new, 1))
            r = subprocess.run(
                cmd,
                cwd=cwd,
                capture_output=True,
                text=True,
                check=False,
                env=dict(os.environ, NO_COLOR="1"),
            )
            out = r.stdout + r.stderr
            failures = [
                l for l in out.splitlines() if any(m in l for m in FAILURE_MARKERS)
            ]
            if r.returncode == 0:
                results.append((old, "SURVIVED", ""))
            elif any(BUILD_FAILURE.search(l) for l in out.splitlines()):
                results.append((old, "BUILD ERROR", ""))
            elif failures:
                results.append((old, "CAUGHT", failures[0].strip()[-100:]))
            else:
                results.append((old, "BUILD ERROR", ""))
    finally:
        path.write_text(original)
    assert path.read_text() == original
    return results


def main(argv: list[str]) -> int:
    p = argparse.ArgumentParser(description=(__doc__ or "").split("\n\n")[0])
    p.add_argument("file")
    p.add_argument("mutation", nargs="+", help="'<old>=><new>'")
    p.add_argument("--cmd", default="pio test -e native")
    p.add_argument("--cwd", default=".")
    a = p.parse_args(argv)
    mutations = []
    for m in a.mutation:
        if "=>" not in m:
            print(f"mutate: `{m}` has no `=>`", file=sys.stderr)
            return 2
        old, new = m.split("=>", 1)
        mutations.append((old, new))
    try:
        results = run_mutations(
            Path(a.file), mutations, shlex.split(a.cmd), Path(a.cwd)
        )
    except ValueError as e:
        print(f"mutate: {e}", file=sys.stderr)
        return 2
    for old, status, detail in results:
        print(f"{old[:50]!r:54} {status:11} {detail}")
    print("file restored")
    return 1 if any(s != "CAUGHT" for _, s, _ in results) else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
