"""PlatformIO pre-build script: converts xscreensaver's pacman.png into the
raw blob pacman.c includes as images/gen/pacman_png.h (see
src/x11shim/include/ximage-loader.h for the format) and puts it on the include
path. The image is read from vendor/ and never committed; its header is
3 MB of C text. Needs uv, because Pillow is not in PlatformIO's Python."""

import os
import shutil
import subprocess
import sys
from pathlib import Path

Import("env")  # noqa: F821

repo = Path(env["PROJECT_DIR"]).parent  # noqa: F821
image = repo / "vendor" / "xscreensaver-6.16" / "hacks" / "images" / "pacman.png"
converter = repo / "tools" / "make_logo_blob.py"
generated = Path(env.subst("$BUILD_DIR")) / "generated"  # noqa: F821
header = generated / "images" / "gen" / "pacman_png.h"

if not image.is_file():
    sys.exit(f"{image} is missing: run `uv run tools/fetch_xscreensaver.py`")
if (
    not header.exists()
    or header.stat().st_mtime < image.stat().st_mtime
    or header.stat().st_mtime < converter.stat().st_mtime
):
    uv = shutil.which("uv")
    if not uv:
        sys.exit("pacman_sprites.py needs uv on PATH to convert pacman.png")
    header.parent.mkdir(parents=True, exist_ok=True)
    # Written beside the header and renamed, so a killed build never leaves a
    # truncated header that the mtime check above would trust.
    partial = header.with_name(header.name + ".partial")
    partial.unlink(missing_ok=True)
    subprocess.run(
        [uv, "run", str(converter), str(image), "--name", "pacman_png", "-o", str(partial)],
        check=True,
    )
    os.replace(partial, header)

env.Append(CPPPATH=[str(generated)])  # noqa: F821
