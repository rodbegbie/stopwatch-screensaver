"""Builds the pool of Pacman levels that replaces upstream's random generator.

Upstream's `pacman_createnewlevel` plays the fixed `stdlevel` half the time and
a generated level the rest. The generator recurses once per tile with a 1.3 KB
copy of the level in each frame, far more than the board's 16 KB loop stack, so
the firmware never calls it (see pacman_patch.py). This tool runs the real
generator on the Mac, where the stack is large, and stores what it makes.

The generator knows nothing about the round display. At 11 px a cell the 40 by
32 maze is 440 by 352 px, centred on the 466 px panel, so cells near its corners
fall outside the circle. The harness fills those cells with wall before the
generator runs; `tryset` will not put a dot on a wall, so the generator routes
around them. `problems()` checks the result again, so a level that got through
anyway is refused.

    uv run tools/pacman_levels.py --count 24 --seed 1

writes firmware/src/hacks/pacman_pool.h.
"""

import argparse
import re
import subprocess
import sys
import tempfile
from collections import deque
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
HACKS = ROOT / "firmware" / "src" / "hacks"
POOL_HEADER = HACKS / "pacman_pool.h"

WIDTH = 40
HEIGHT = 32
MIN_DOT_PERCENT = 10  # MINDOTPERC in pacman.h

PANEL = 466
CELL = min(PANEL // WIDTH, PANEL // HEIGHT)
X_OFFSET = (PANEL - WIDTH * CELL) >> 1
Y_OFFSET = (PANEL - HEIGHT * CELL) >> 1
RADIUS = PANEL // 2

DOT = "`"
WALL = "#"


def cell_visible(x: int, y: int) -> bool:
    """True if the whole cell is inside the panel's circle."""
    for cx in (x, x + 1):
        for cy in (y, y + 1):
            dx = X_OFFSET + cx * CELL - RADIUS
            dy = Y_OFFSET + cy * CELL - RADIUS
            if dx * dx + dy * dy > RADIUS * RADIUS:
                return False
    return True


def outside_circle(level: list[str]) -> list[tuple[int, int]]:
    return [
        (x, y)
        for y, row in enumerate(level)
        for x, c in enumerate(row)
        if c == DOT and not cell_visible(x, y)
    ]


def _dots(level: list[str]) -> set[tuple[int, int]]:
    return {
        (x, y) for y, row in enumerate(level) for x, c in enumerate(row) if c == DOT
    }


def problems(level: list[str]) -> list[str]:
    found = []
    if len(level) != HEIGHT or any(len(row) != WIDTH for row in level):
        return [f"not {WIDTH} by {HEIGHT}"]
    if any(row != row[::-1] for row in level):
        found.append("not left-right symmetric")
    dots = _dots(level)
    if len(dots) * 100 < WIDTH * HEIGHT * MIN_DOT_PERCENT:
        found.append(f"only {len(dots)} dots")
    outside = outside_circle(level)
    if outside:
        found.append(f"{len(outside)} dot cells outside the circle, first {outside[0]}")
    if dots:
        start = min(dots)
        seen = {start}
        queue = deque([start])
        while queue:
            x, y = queue.popleft()
            for step in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                cell = (x + step[0], y + step[1])
                if cell in dots and cell not in seen:
                    seen.add(cell)
                    queue.append(cell)
        if len(seen) != len(dots):
            found.append(f"dots not connected ({len(dots) - len(seen)} cut off)")
    return found


HARNESS = r"""
#include "hacks/pacman/pacman_level.c"
#include <stdio.h>

static const unsigned char hidden[LEVHEIGHT][LEVWIDTH] = {
%(mask)s
};

int main(void) {
    pacmangamestruct pp;
    unsigned dirvec[1] = { GO_UP };
    int found = 0;
    long attempts = 0;
    lev_t level;
    unsigned x, y;

    memset(&pp, 0, sizeof pp);
    srandom(%(seed)d);
    while (found < %(count)d && attempts < %(max_attempts)d) {
        unsigned ret;
        attempts++;
        clearlevel(&level);
        for (y = 0; y < LEVHEIGHT; y++)
            for (x = 0; x < LEVWIDTH; x++)
                if (hidden[y][x])
                    level[y][x] = BLOCK_WALL;
        createjail(&level, JAILWIDTH, JAILHEIGHT);
        ret = nextstep(&pp, &level, LEVWIDTH / 2 - 1,
                       LEVHEIGHT / 2 - JAILHEIGHT / 2 - 3, dirvec, 1);
        if (ret == 0 || ret * 100 < LEVWIDTH * LEVHEIGHT * MINDOTPERC)
            continue;
        filllevel(&level);
        for (y = 0; y < LEVHEIGHT; y++)
            printf("%%.*s\n", (int) LEVWIDTH, level[y]);
        printf("--\n");
        found++;
    }
    fprintf(stderr, "attempts=%%ld accepted=%%d\n", attempts, found);
    return 0;
}
"""


def _harness_source(count: int, seed: int, max_attempts: int) -> str:
    rows = []
    for y in range(HEIGHT):
        cells = ",".join("0" if cell_visible(x, y) else "1" for x in range(WIDTH))
        rows.append("    {" + cells + "}")
    return HARNESS % {
        "mask": ",\n".join(rows),
        "count": count,
        "seed": seed,
        "max_attempts": max_attempts,
    }


def generate(
    count: int, seed: int, workdir: Path, max_attempts: int = 100000
) -> list[list[str]]:
    """Runs upstream's generator on the host and returns `count` distinct levels."""
    workdir.mkdir(parents=True, exist_ok=True)
    source = workdir / "pacman_pool_harness.c"
    program = workdir / "pacman_pool_harness"
    # Asking for extra lets duplicates be dropped without a second run.
    source.write_text(_harness_source(count * 2, seed, max_attempts))
    firmware = ROOT / "firmware"
    subprocess.run(
        [
            "cc",
            "-O1",
            "-w",
            "-DXSHIM_NATIVE",
            "-DSTANDALONE",
            f"-I{firmware / 'src'}",
            f"-I{firmware / 'src' / 'x11shim' / 'include'}",
            f"-I{firmware / 'src' / 'xs_support'}",
            str(source),
            "-o",
            str(program),
        ],
        check=True,
        capture_output=True,
        text=True,
    )
    run = subprocess.run(
        [str(program)], check=True, capture_output=True, text=True, timeout=600
    )
    print(run.stderr.strip(), file=sys.stderr)

    levels: list[list[str]] = []
    for block in run.stdout.split("--\n"):
        rows = block.splitlines()
        if len(rows) == HEIGHT and rows not in levels:
            levels.append(rows)
        if len(levels) == count:
            break
    return levels


def render_header(levels: list[list[str]]) -> str:
    for number, level in enumerate(levels):
        if len(level) != HEIGHT or any(len(row) != WIDTH for row in level):
            raise ValueError(f"level {number} is not {HEIGHT} rows of {WIDTH}")
    out = [
        "/* Generated by tools/pacman_levels.py: do not edit.",
        " *",
        " * Levels made by upstream's generator (pacman_level.c) on the host, in the",
        " * form `filllevel` leaves them and `stdlevel` has: walls and dots only, the",
        " * jail unfinished. Every dot cell is inside the panel's circle. The levels",
        " * are derived from xscreensaver's Pacman generator, (c) 2002 Edwin de Jong:",
        " * see THIRD_PARTY_NOTICES.md. */",
        f"#define PACMAN_POOL_SIZE {len(levels)}",
        "",
        "static const lev_t pacman_pool[PACMAN_POOL_SIZE] = {",
    ]
    for level in levels:
        out.append("    {")
        out.extend(f'        "{row}",' for row in level)
        out.append("    },")
    out.append("};")
    return "\n".join(out) + "\n"


def read_header(text: str) -> list[list[str]]:
    body = text.split("pacman_pool[PACMAN_POOL_SIZE] = {", 1)[1]
    rows = re.findall(r'"([^"]*)"', body)
    return [rows[i : i + HEIGHT] for i in range(0, len(rows), HEIGHT)]


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--count", type=int, default=24)
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--out", type=Path, default=POOL_HEADER)
    args = parser.parse_args()

    with tempfile.TemporaryDirectory() as tmp:
        levels = generate(args.count, args.seed, Path(tmp))
    bad = [(n, p) for n, level in enumerate(levels) if (p := problems(level))]
    if bad:
        raise SystemExit(f"generated levels failed their checks: {bad}")
    if len(levels) < args.count:
        raise SystemExit(f"only {len(levels)} of {args.count} distinct levels")
    args.out.write_text(render_header(levels))
    print(f"wrote {len(levels)} levels to {args.out}")


if __name__ == "__main__":
    main()
