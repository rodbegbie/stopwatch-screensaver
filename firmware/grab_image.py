"""PlatformIO pre-build script: turns the picture hacks "grab" into a header,
generated/grab_image.h in the build directory, that main.cpp hands to the shim
(xshim_set_grab_image). DecayScreen melts it; upstream grabs the screen.

The picture is firmware/assets/grab-image.png (a Windows 95 screenshot), or the
file named by XS_GRAB_IMAGE (relative paths are taken from the repository
root). It is scaled to fit 466 pixels (a 640 by 480 screenshot becomes 466 by
350, stored deflated in about 41 KB of flash). With XS_GRAB_IMAGE=none nothing is generated and
a hack that wants a picture loads nothing. Needs uv, because Pillow is not in
PlatformIO's Python."""

import os
import shutil
import subprocess
import sys
from pathlib import Path

Import("env")  # noqa: F821

project = Path(env["PROJECT_DIR"])  # noqa: F821
header = Path(env.subst("$BUILD_DIR")) / "generated" / "grab_image.h"  # noqa: F821
GRAB_SIZE = 466

choice = os.environ.get("XS_GRAB_IMAGE", "")
image = None
if choice != "none":
    image = Path(choice) if choice else project / "assets" / "grab-image.png"
    if not image.is_absolute():
        image = project.parent / image
    if not image.is_file():
        sys.exit(f"XS_GRAB_IMAGE={choice or 'default'}: no such file ({image})")

if image is None:
    header.unlink(missing_ok=True)
else:
    uv = shutil.which("uv")
    if not uv:
        raise SystemExit("the grab image needs uv on PATH to convert the image")
    subprocess.run(
        [
            uv,
            "run",
            str(project.parent / "tools" / "make_logo_blob.py"),
            str(image),
            "--name",
            "grab_image_blob",
            "--size",
            str(GRAB_SIZE),
            "--deflate",
            "-o",
            str(header),
        ],
        check=True,
    )
    print(f"Grab image: {image}")
