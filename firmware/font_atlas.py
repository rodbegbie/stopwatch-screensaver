"""PlatformIO pre-build script: rasterises Special Elite (Apache 2.0,
firmware/assets/fonts) into generated/special_elite.h in the build directory.
main.cpp hands it to the shim (xshim_set_font), and the Xft stand-in draws
XJack's text from it. The sizes are the ones XJack's and FilmLeader's font
resources can land on after the shim picks the nearest. Needs uv, because Pillow is not in
PlatformIO's Python."""

import shutil
import subprocess
from pathlib import Path

Import("env")  # noqa: F821

project = Path(env["PROJECT_DIR"])  # noqa: F821
generated = Path(env.subst("$BUILD_DIR")) / "generated"  # noqa: F821
header = generated / "special_elite.h"
font = project / "assets" / "fonts" / "SpecialElite-Regular.ttf"
SIZES = "12,16,20,24"

uv = shutil.which("uv")
if not uv:
    raise SystemExit("the Xft font needs uv on PATH to rasterise Special Elite")
generated.mkdir(parents=True, exist_ok=True)
fresh = generated / "special_elite.h.new"
subprocess.run(
    [
        uv,
        "run",
        str(project.parent / "tools" / "make_font_atlas.py"),
        str(font),
        "--name",
        "special_elite",
        "--sizes",
        SIZES,
        "-o",
        str(fresh),
    ],
    check=True,
)
if not header.exists() or header.read_bytes() != fresh.read_bytes():
    fresh.replace(header)
else:
    fresh.unlink()
env.Append(CPPPATH=[str(generated)])  # noqa: F821
