"""PlatformIO pre-build script: converts xscreensaver's four xmatrix glyph
sheets (matrix1, matrix1b, matrix2, matrix2b) into the raw blobs xmatrix.c
includes as images/gen/<name>_png.h (see src/x11shim/include/ximage-loader.h
for the format) and puts them on the include path. The images are read from
vendor/ and never committed. Needs uv, because Pillow is not in PlatformIO's
Python."""

import os
import shutil
import subprocess
import sys
from pathlib import Path

Import("env")  # noqa: F821

repo = Path(env["PROJECT_DIR"]).parent  # noqa: F821
images = repo / "vendor" / "xscreensaver-6.16" / "hacks" / "images"
converter = repo / "tools" / "make_logo_blob.py"
generated = Path(env.subst("$BUILD_DIR")) / "generated"  # noqa: F821

for name in ("matrix1", "matrix1b", "matrix2", "matrix2b"):
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
        raise SystemExit(
            "matrix_glyphs.py needs uv on PATH to convert the glyph sheets"
        )
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
            "-o",
            str(partial),
        ],
        check=True,
    )
    os.replace(partial, header)

env.Append(CPPPATH=[str(generated)])  # noqa: F821
