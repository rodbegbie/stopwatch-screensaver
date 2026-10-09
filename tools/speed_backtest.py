"""Check the host speed bands against the hacks that have run on the device.

Runs each registered hack on the host (the `dump` program's `stats` mode),
joins the host time to the step measured on the device in
tools/assessment_measured.md, and writes docs/speed-backtest.md: how well the
host time ranked the device step, and what each band turned out to mean.

Run: (cd firmware && pio run -e dump) && uv run tools/speed_backtest.py
"""

import argparse
import statistics
import subprocess
import sys
from pathlib import Path

import probe_hacks
import score_hacks

SLOW_DEVICE_MS = 50.0
RUNS = 3
BANDS = ("low", "medium", "high")

# Ported after the bands were fixed, with predictions committed first in
# docs/speed-predictions.md. They are kept out of the fit and reported apart.
HOLDOUT = frozenset({"mountain", "epicycle", "kaleidescope", "celtic"})

class BacktestError(Exception):
    pass


def registry_names(registry: str) -> list[str]:
    """Hack names in g_hacks[] order, which is the index `dump` takes."""
    array = score_hacks.REGISTRY_ARRAY.search(score_hacks.strip_noise(registry))
    if not array:
        raise BacktestError("no g_hacks[] array found in the registry source")
    return score_hacks.REGISTRY_ENTRY.findall(array.group(1))


def _ranks(values: list[float]) -> list[float]:
    order = sorted(range(len(values)), key=lambda i: values[i])
    ranks = [0.0] * len(values)
    i = 0
    while i < len(order):
        j = i
        while j + 1 < len(order) and values[order[j + 1]] == values[order[i]]:
            j += 1
        for k in range(i, j + 1):
            ranks[order[k]] = (i + j) / 2
        i = j + 1
    return ranks


def spearman(a: list[float], b: list[float]) -> float:
    """Rank correlation, with tied values sharing their average rank."""
    ra, rb = _ranks(a), _ranks(b)
    ma, mb = statistics.fmean(ra), statistics.fmean(rb)
    num = sum((x - ma) * (y - mb) for x, y in zip(ra, rb))
    den = (
        sum((x - ma) ** 2 for x in ra) * sum((y - mb) ** 2 for y in rb)
    ) ** 0.5
    return num / den if den else 0.0


def measure_host(program: Path, index: int) -> float:
    times = []
    for _ in range(RUNS):
        run = subprocess.run(
            [str(program), "stats", str(index)], capture_output=True, text=True, check=False
        )
        if run.returncode != 0:
            raise BacktestError(
                f"{program} stats {index} exited {run.returncode}: {run.stderr.strip()}"
            )
        times.append(probe_hacks.parse_stats(run.stdout))
    return statistics.median(times)


def probe_shelved(
    names: list[str],
    measured: dict[str, tuple[float, float]],
    root: Path,
    probe=probe_hacks.probe,
) -> dict[str, float]:
    """Host times for hacks that were ported, measured on the device and then
    taken out of the registry (tools/failed_ports.txt). The harness cannot run
    them, but their source is still in firmware/src/hacks, and the standalone
    probe agrees with the harness (0.73-1.43x on 20 hacks)."""
    host = {}
    for name in names:
        source = root / "firmware" / "src" / "hacks" / name / f"{name}.c"
        if name not in measured or not source.exists():
            continue
        result = probe(name, source.read_text(errors="replace"), root)
        if "error" in result:
            raise BacktestError(f"{name}: {result['error']}")
        host[name] = result["host_ms"]
    return host


def join_rows(
    names: list[str],
    host: dict[str, float],
    measured: dict[str, tuple[float, float]],
) -> list[dict]:
    rows = []
    for name in names:
        if name not in host or name not in measured:
            continue
        lo, hi = measured[name]
        mid = (lo + hi) / 2
        rows.append(
            {
                "name": name,
                "host_ms": host[name],
                "device_lo": lo,
                "device_hi": hi,
                "device_mid": mid,
                "band": probe_hacks.band(host[name]),
                "ratio": mid / host[name] if host[name] else float("inf"),
            }
        )
    return rows


def summarise(rows: list[dict]) -> dict:
    bands = {}
    for band in BANDS:
        steps = [r["device_mid"] for r in rows if r["band"] == band]
        bands[band] = {
            "count": len(steps),
            "max_device_ms": max(steps) if steps else None,
        }
    slow = sorted(
        (r for r in rows if r["device_mid"] >= SLOW_DEVICE_MS),
        key=lambda r: -r["device_mid"],
    )
    return {
        "bands": bands,
        "slow": [r["name"] for r in slow],
        "caught": [r["name"] for r in slow if r["band"] == "high"],
        "missed": [r["name"] for r in slow if r["band"] != "high"],
        "spearman": spearman(
            [r["host_ms"] for r in rows], [r["device_mid"] for r in rows]
        ),
        "false_high": [
            r["name"]
            for r in rows
            if r["band"] == "high" and r["device_mid"] < SLOW_DEVICE_MS
        ],
    }


def _range(lo: float, hi: float) -> str:
    return f"{lo:g}" if lo == hi else f"{lo:g}-{hi:g}"


def render_table(rows: list[dict], holdout=frozenset()) -> str:
    lines = [
        "| Hack | Set | Host ms | Band | Device step ms | Device / host |",
        "| --- | --- | --- | --- | --- | --- |",
    ]
    for r in sorted(rows, key=lambda r: (-r["device_mid"], r["name"])):
        which = "held out" if r["name"] in holdout else "fitted"
        lines.append(
            f"| {r['name']} | {which} | {r['host_ms']:.3f} | {r['band']} | "
            f"{_range(r['device_lo'], r['device_hi'])} | {r['ratio']:.0f} |"
        )
    return "\n".join(lines) + "\n"


def _worst(band: dict) -> str:
    return "-" if band["max_device_ms"] is None else f"{band['max_device_ms']:.1f}"


def _slow_and_bands(s: dict) -> str:
    """The bullets that say how the bands did, for one set of hacks."""
    low, med, high = s["bands"]["low"], s["bands"]["medium"], s["bands"]["high"]
    return f"""\
- The slowest hack in the low band took {_worst(low)} ms on the device
  ({low['count']} hacks), in the medium band {_worst(med)} ms ({med['count']}
  hacks) and in the high band {_worst(high)} ms ({high['count']} hacks).
- Of the {len(s['slow'])} {'hack' if len(s['slow']) == 1 else 'hacks'} with a
  device step of {SLOW_DEVICE_MS:g} ms or more, the high band caught
  {len(s['caught'])}.
  Missed: {', '.join(s['missed']) or 'none'}.
- High band but under {SLOW_DEVICE_MS:g} ms on the device:
  {', '.join(s['false_high']) or 'none'}.
"""


def render(rows: list[dict], holdout=HOLDOUT) -> str:
    fitted = [r for r in rows if r["name"] not in holdout]
    held = [r for r in rows if r["name"] in holdout]
    s = summarise(fitted)
    out_of_sample = ""
    if held:
        out_of_sample = f"""\
## Out of sample

These {len(held)} hacks were ported after the bands were fixed, with their
predictions committed first (`docs/speed-predictions.md`), so they are left out
of the figures above and are the real test of them.

{_slow_and_bands(summarise(held))}
"""
    return f"""\
# Speed backtest

How well a hack's time on the host predicts its step on the device, over the
{len(fitted)} registered hacks that had run on the board when the bands were
fixed, and then the {len(held)} ported since. Generated by
`tools/speed_backtest.py`; the host time is `tools/probe_hacks.py`'s measure.

## Result

The bands were fitted to the {len(fitted)} hacks in this section, so these
figures are in-sample.

- Host time ranks the device step with a Spearman correlation of
  {s['spearman']:.2f}.
- Bands (host ms per step): low is under {probe_hacks.LOW_MS}, medium is under
  {probe_hacks.HIGH_MS}, high is {probe_hacks.HIGH_MS} or more.
{_slow_and_bands(s)}
{out_of_sample}## Table

Device step is the range measured in `tools/assessment_measured.md`; the ratio
uses its midpoint.

{render_table(rows, holdout)}
## Reading it

- The band says how likely a hack is to be slow, not how many milliseconds it
  will take. The device ran between about 60 and 1,500 times the host time.
- The ratio is lowest (about 60-120) for hacks bound by drawing or already
  built in single precision, and highest (about 700-1,500) for hacks that
  still do software double-precision maths on the device (Vines, Hopalong,
  Spiral, XSpirograph). A low host time does not clear a hack that does a lot
  of `double` maths.
- The bands were chosen from the fitted hacks, so their catch rate is
  in-sample. A hack ported from here on is a further test: add it to `HOLDOUT`
  in `tools/speed_backtest.py` with its prediction committed first, and re-run
  this.
- Only hacks that were ported are here, and they were chosen because they
  looked easy, so there are few slow ones.
- The thresholds belong to the `dump` environment's build (`-g`, no
  optimisation) on the machine they were measured on. Re-run this after
  changing either, and move the bands in `tools/probe_hacks.py` if they no
  longer fit.
"""


def main(argv: list[str], root: Path | None = None) -> int:
    root = root or Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--dump", default=str(root / "firmware/.pio/build/dump/program")
    )
    parser.add_argument("--registry", default=str(root / "firmware/src/hacks/registry.c"))
    parser.add_argument("--measured", default=str(root / "tools/assessment_measured.md"))
    parser.add_argument("--failed-ports", default=str(root / "tools/failed_ports.txt"))
    parser.add_argument("--out", default=str(root / "docs/speed-backtest.md"))
    args = parser.parse_args(argv)
    try:
        program = Path(args.dump)
        if not program.exists():
            raise BacktestError(
                f"{program} not found; build it with `pio run -e dump` in firmware/"
            )
        names = registry_names(Path(args.registry).read_text())
        measured = probe_hacks.parse_measured(Path(args.measured).read_text())
        host = {name: measure_host(program, i) for i, name in enumerate(names)}
        failed_path = Path(args.failed_ports)
        shelved = (
            list(score_hacks.read_failed_ports(failed_path.read_text()))
            if failed_path.exists()
            else []
        )
        host.update(probe_shelved(shelved, measured, root))
        rows = join_rows(names + shelved, host, measured)
        Path(args.out).write_text(render(rows))
        fitted = summarise([r for r in rows if r["name"] not in HOLDOUT])
        held_rows = [r for r in rows if r["name"] in HOLDOUT]
        message = (
            f"wrote {args.out}: {len(rows)} hacks; fitted: spearman "
            f"{fitted['spearman']:.2f}, high band caught {len(fitted['caught'])} "
            f"of {len(fitted['slow'])} slow"
        )
        if held_rows:
            held = summarise(held_rows)
            message += (
                f"; held out ({len(held_rows)}): high band caught "
                f"{len(held['caught'])} of {len(held['slow'])} slow"
            )
        print(message)
        return 0
    except (BacktestError, probe_hacks.ProbeError) as err:
        print(f"error: {err}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
