"""PlatformIO pre-build script: extracts the Flying Toasters sprites from a
user-supplied After Dark disk image into toaster_sprites.h in the build
directory. The image is never committed. When it is absent the hack is left
out (HAVE_AFTER_DARK_TOASTERS stays undefined). Needs uv, because machfs is
not in PlatformIO's Python."""

import hashlib
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

Import("env")  # noqa: F821

repo = Path(env["PROJECT_DIR"]).parent  # noqa: F821
image = Path(os.environ.get("AFTER_DARK_IMAGE") or "vendor/after-dark-v2.0v-800k")
if not image.is_absolute():
    image = repo / image
tool = repo / "tools" / "after_dark_sprites.py"
generated = Path(env.subst("$BUILD_DIR")) / "generated"  # noqa: F821
header = generated / "after_dark" / "toaster_sprites.h"

if not image.is_file():
    print(f"ad-flyingtoasters: no After Dark disk image at {image}; left out")
else:
    image_sha = hashlib.sha256(image.read_bytes()).hexdigest()
    recorded = (
        re.search(r"sha256 ([0-9a-f]{64})", header.read_text().split("\n", 1)[0])
        if header.exists()
        else None
    )
    if (
        recorded is None
        or recorded.group(1) != image_sha
        or header.stat().st_mtime < image.stat().st_mtime
        or header.stat().st_mtime < tool.stat().st_mtime
    ):
        uv = shutil.which("uv")
        if not uv:
            sys.exit("after_dark_sprites.py needs uv on PATH to read the disk image")
        header.parent.mkdir(parents=True, exist_ok=True)
        result = subprocess.run(
            [uv, "run", str(tool), str(image), "-o", str(header)],
            capture_output=True,
            text=True,
            check=False,
        )
        if result.returncode != 0:
            sys.exit(
                result.stderr.strip() or f"{tool.name} failed ({result.returncode})"
            )
    env.Append(  # noqa: F821
        CPPPATH=[str(generated)], CPPDEFINES=["HAVE_AFTER_DARK_TOASTERS"]
    )
