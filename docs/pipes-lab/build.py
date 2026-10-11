"""Builds Pipes for the host against the shim and gl_budget's instrumented
TinyGL, from vendor/ (nothing from Pipes is copied into the repo).

    python3 -I docs/pipes-lab/build.py <out-dir> [--incremental] [override ...]
    <out-dir>/prog <frames> <seed>   # one line per frame: counts and a hash

ASAN=1 keeps AddressSanitizer on. See README.md."""

import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
LAB = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT / "tools"))
import gl_budget
import probe_hacks

out = Path(sys.argv[1])
incremental = "--incremental" in sys.argv
overrides = [a for a in sys.argv[2:] if a != "--incremental"]
out.mkdir(parents=True, exist_ok=True)
glx = ROOT / "vendor/xscreensaver-6.16/hacks/glx"
for h in ("sphere.h", "teapot.h", "buildlwo.h", "teapot2.h"):
    shutil.copy(glx / h, out / h)
generated = out / "generated"
gl_budget.write_tinygl(ROOT, generated)
pre = "#define USE_GL\n" + ('#include "pipes_incremental.h"\n' if incremental else "")
shutil.copy(LAB / "pipes_incremental.h", out / "pipes_incremental.h")
(out / "pipes.c").write_text(pre + (glx / "pipes.c").read_text())
helpers = []
for name in ("sphere.c", "teapot.c", "buildlwo.c", "pipeobjs.c"):
    extra = {
        "teapot.c": "#define HAVE_JWZGLES\n",
        "buildlwo.c": '#include "GL/gl.h"\n#define GL_POLYGON GL_TRIANGLE_FAN\n',
    }.get(name, "")
    (out / name).write_text("#define USE_GL\n" + extra + (glx / name).read_text())
    helpers.append(str(out / name))
(out / "probe_registry.c").write_text(
    probe_hacks.registry_source("Pipes", "pipes", True, overrides or ["*delay: 10000"])
)
src = ROOT / "firmware/src"
shim = [str(f) for f in gl_budget.shim_sources(ROOT) if f.name != "gl_budget_main.c"]
flags = (
    list(probe_hacks.BUILD_FLAGS)
    if os.environ.get("ASAN")
    else [f for f in probe_hacks.BUILD_FLAGS if not f.startswith("-fsanitize")]
)
cmd = [
    "cc",
    *flags,
    "-w",
    "-DSTANDALONE",
    "-DHAVE_MOBILE",
    "-DXSHIM_NATIVE",
    f"-I{out}",
    f"-I{src}",
    f"-I{src / 'x11shim/include'}",
    f"-I{src / 'xs_support'}",
    f"-I{src / 'xs_support/glx'}",
    f"-I{generated / 'tinygl/include'}",
    f"-I{generated}",
    str(out / "pipes.c"),
    *helpers,
    str(out / "probe_registry.c"),
    *shim,
    str(LAB / "lab_main.c"),
    "-lm",
    "-lpthread",
    "-o",
    str(out / "prog"),
]
r = subprocess.run(cmd, check=False, capture_output=True, text=True)
if r.returncode:
    print(r.stderr[-4000:])
    sys.exit(1)
print("built", out / "prog")
