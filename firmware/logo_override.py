"""PlatformIO pre-build script: with XS_LOGO=<image> set, writes a logo header
for Maze from that image; without it, removes any header left by an earlier
build so the committed logo is used.

maze_patched.c includes "images/gen/logo-50_png.h" from its own directory
first, so the generated copy wins without touching the include path. Relative
paths are taken from the repository root. Needs uv, because Pillow is not in
PlatformIO's Python."""

import os
import shutil
import subprocess
import sys
from pathlib import Path

Import("env")  # noqa: F821

project = Path(env["PROJECT_DIR"])  # noqa: F821
header = (
    Path(env.subst("$BUILD_DIR"))  # noqa: F821
    / "generated"
    / "images"
    / "gen"
    / "logo-50_png.h"
)
LOGO_SIZE = 50

logo = os.environ.get("XS_LOGO")
if not logo:
    header.unlink(missing_ok=True)
else:
    image = Path(logo)
    if not image.is_absolute():
        image = project.parent / image
    if not image.is_file():
        sys.exit(f"XS_LOGO={logo}: no such file ({image})")
    uv = shutil.which("uv")
    if not uv:
        sys.exit("XS_LOGO needs uv on PATH to convert the image")
    subprocess.run(
        [
            uv,
            "run",
            str(project.parent / "tools" / "make_logo_blob.py"),
            str(image),
            "--name",
            "logo_50_png",
            "--size",
            str(LOGO_SIZE),
            "-o",
            str(header),
        ],
        check=True,
    )
    print(f"Maze logo: {image}")
