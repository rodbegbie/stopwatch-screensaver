"""Run an unported hack on the host and time it, to rank how slow it will be.

The static scorer cannot tell how often a hack's code runs per frame, so it
could not tell that Braid, rated S, would draw about 7,500 wide lines a frame.
Running the hack can. `probe` builds the hack with the shim and the runner into
a program with a one-hack registry (`firmware/native/dump_main.c`, `stats`
mode) and reads its mean milliseconds per step.

A host time is not a device time: the device took between about 60 and 1,500
times as long, depending on how much of the hack is software double-precision
maths (see docs/speed-backtest.md). It ranks well, though: across the 29
measured hacks it ranks the device step with a Spearman correlation of 0.89.
"""

import re
import statistics
import subprocess
import tempfile
from pathlib import Path

# Host milliseconds per step. Fitted to the measured hacks in
# docs/speed-backtest.md, so the fit is in-sample: no hack below HIGH_MS had a
# device step midpoint over 20 ms, and every hack at or over 50 ms is above it.
LOW_MS = 0.015
HIGH_MS = 0.05

# The flags of firmware's `dump` environment, which the bands were calibrated
# on: with -Os the same hacks ran about a third as long.
BUILD_FLAGS = ["-g"]

RUNS = 3
RUN_TIMEOUT_S = 120

MODULE = re.compile(
    r'\bXSCREENSAVER_MODULE(_2)?\s*\(\s*"([^"]+)"\s*,\s*(\w+)', re.DOTALL
)
COMMENT = re.compile(r"/\*.*?\*/|//[^\n]*", re.DOTALL)
XLOCKMORE = re.compile(r'#\s*include\s*"xlockmore\.h"')
STATS = re.compile(r"\bhost_ms=([0-9.]+)")
MACOS_UNDEFINED = re.compile(r'^\s*"_([^"]+)", referenced from', re.MULTILINE)
GNU_UNDEFINED = re.compile(r"undefined reference to `([^']+)'")
FIRST_ERROR = re.compile(r"^.*?\berror: .+$", re.MULTILINE)

# The same macro registry.c uses for a hack built on xlockmore.h, which defines
# a function table rather than a HackEntry.
REGISTRY_HEAD = """\
#include "hacks/registry.h"

#define XLOCKMORE_HACK(NAME, CLASS)                                      \\
  extern struct xscreensaver_function_table NAME##_xscreensaver_function_table; \\
  static const HackEntry NAME##_hack = {                                 \\
      CLASS, NULL, NULL, NULL, NULL,                                     \\
      &NAME##_xscreensaver_function_table, NULL}
"""


class ProbeError(Exception):
    pass


def module_entry(source: str) -> tuple[str, str]:
    """The (class, name) a hack registers with XSCREENSAVER_MODULE. For
    XSCREENSAVER_MODULE_2 (CLASS, NAME, PREFIX) the function table is named
    after NAME, which is what the registry needs."""
    matches = MODULE.findall(COMMENT.sub(" ", source))
    if not matches:
        raise ProbeError("no XSCREENSAVER_MODULE entry found")
    if len(matches) > 1:
        raise ProbeError("the hack has several XSCREENSAVER_MODULE entries")
    _, class_name, name = matches[0]
    return class_name, name


def registry_source(class_name: str, prefix: str, xlockmore: bool) -> str:
    if xlockmore:
        declare = f'XLOCKMORE_HACK({prefix}, "{class_name}");\n'
    else:
        declare = f"extern const HackEntry {prefix}_hack;\n"
    return (
        REGISTRY_HEAD
        + "\n"
        + declare
        + f"\nconst HackEntry *const g_hacks[] = {{&{prefix}_hack}};\n"
        + "const int g_hack_count = 1;\n"
    )


def parse_stats(stdout: str) -> float:
    match = STATS.search(stdout)
    if not match:
        raise ProbeError(f"no host_ms in the program's output: {stdout.strip()[:80]!r}")
    return float(match.group(1))


def band(host_ms: float) -> str:
    if host_ms >= HIGH_MS:
        return "high"
    if host_ms >= LOW_MS:
        return "medium"
    return "low"


def undefined_symbols(stderr: str) -> list[str]:
    found = MACOS_UNDEFINED.findall(stderr) + GNU_UNDEFINED.findall(stderr)
    return list(dict.fromkeys(found))


def shim_sources(root: Path) -> list[Path]:
    """Everything a hack links against: the canvas, the Xlib shim, the runner
    and the copied xscreensaver support files, plus the probe's main."""
    src = root / "firmware" / "src"
    files = sorted((src / "core").glob("*.c"))
    files += sorted((src / "x11shim").glob("*.c"))
    files += sorted((src / "xs_support").glob("*.c"))
    files.append(src / "runner" / "hack_runner.c")
    files.append(root / "firmware" / "native" / "dump_main.c")
    return files


def probe(name: str, source: str, root: Path, cc: str = "cc") -> dict:
    """Builds and times the hack. Returns {"host_ms": float} or {"error": str}."""
    try:
        class_name, prefix = module_entry(source)
        with tempfile.TemporaryDirectory() as tmp_name:
            tmp = Path(tmp_name)
            (tmp / f"{name}.c").write_text(source)
            registry = tmp / "probe_registry.c"
            registry.write_text(
                registry_source(class_name, prefix, bool(XLOCKMORE.search(source)))
            )
            program = tmp / "probe"
            src = root / "firmware" / "src"
            cmd = [
                cc, *BUILD_FLAGS, "-w",
                "-DSTANDALONE", "-DHAVE_MOBILE", "-DXSHIM_NATIVE",
                f"-I{src}", f"-I{src / 'x11shim' / 'include'}",
                f"-I{src / 'xs_support'}",
                str(tmp / f"{name}.c"), str(registry),
                *map(str, shim_sources(root)),
                "-lm", "-lpthread", "-o", str(program),
            ]
            build = subprocess.run(cmd, capture_output=True, text=True)
            if build.returncode != 0:
                missing = undefined_symbols(build.stderr)
                if missing:
                    return {"error": "does not link: " + ", ".join(missing[:6])}
                first = FIRST_ERROR.search(build.stderr)
                detail = first.group(0).strip() if first else f"exit {build.returncode}"
                return {"error": "does not compile: " + detail[:120]}
            times = []
            for _ in range(RUNS):
                run = subprocess.run(
                    [str(program), "stats", "0"],
                    capture_output=True, text=True, timeout=RUN_TIMEOUT_S,
                )
                if run.returncode != 0:
                    how = (
                        f"signal {-run.returncode}"
                        if run.returncode < 0
                        else f"exit {run.returncode}"
                    )
                    return {"error": f"crashed on the host ({how})"}
                times.append(parse_stats(run.stdout))
            return {"host_ms": statistics.median(times)}
    except subprocess.TimeoutExpired:
        return {"error": f"timed out after {RUN_TIMEOUT_S} s on the host"}
    except FileNotFoundError:
        return {"error": f"compiler not found: {cc}"}
    except ProbeError as err:
        return {"error": str(err)}
