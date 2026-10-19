"""PlatformIO pre-build script: converts the textures GL hacks load
(GLMatrix's matrix3.png) into the raw blobs they include as
images/gen/<name>_png.h (see src/x11shim/include/ximage-loader.h for the
format), as matrix_glyphs.py does for XMatrix, and puts them on the include
path. The images are read from vendor/ and never committed. Needs uv, because
Pillow is not in PlatformIO's Python."""

import os
import shutil
import subprocess
import sys
from pathlib import Path

Import("env")  # noqa: F821

repo = Path(env["PROJECT_DIR"]).parent  # noqa: F821
sys.path.insert(0, str(repo / "tools"))
import xs_vendor

images = xs_vendor.upstream(repo) / "hacks" / "images"
converter = repo / "tools" / "make_logo_blob.py"
generated = Path(env.subst("$BUILD_DIR")) / "generated"  # noqa: F821

for name in ("matrix3",):
    image = images / f"{name}.png"
    header = generated / "images" / "gen" / f"{name}_png.h"
    if not image.is_file():
        sys.exit(f"{image} is missing: run `uv run tools/fetch_xscreensaver.py`")
    if (
        header.exists()
        and header.stat().st_mtime >= image.stat().st_mtime
        and header.stat().st_mtime >= converter.stat().st_mtime
    ):
        continue
    uv = shutil.which("uv")
    if not uv:
        raise SystemExit("gl_textures.py needs uv on PATH to convert the textures")
    header.parent.mkdir(parents=True, exist_ok=True)
    # Written beside the header and renamed, so a killed build never leaves a
    # truncated header that the mtime check above would trust.
    partial = header.with_name(header.name + ".partial")
    partial.unlink(missing_ok=True)
    subprocess.run(
        [
            uv,
            "run",
            str(converter),
            str(image),
            "--name",
            f"{name}_png",
            "--deflate",
            "-o",
            str(partial),
        ],
        check=True,
    )
    os.replace(partial, header)

env.Append(CPPPATH=[str(generated)])  # noqa: F821
