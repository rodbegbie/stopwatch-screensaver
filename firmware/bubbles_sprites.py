"""PlatformIO pre-build script: converts xscreensaver's Bubbles PNGs."""

import os
import shutil
import subprocess
import sys
from pathlib import Path

Import("env")  # noqa: F821

repo = Path(env["PROJECT_DIR"]).parent  # noqa: F821
sys.path.insert(0, str(repo / "tools"))
import xs_vendor

images_dir = xs_vendor.upstream(repo) / "hacks" / "images" / "bubbles"
converter = repo / "tools" / "make_logo_blob.py"
generated = Path(env.subst("$BUILD_DIR")) / "generated"  # noqa: F821

names = []
for stem in ("blood", "blue", "glass", "jade"):
    for n in range(1, 12):
        names.append(f"{stem}{n}")

uv = shutil.which("uv")
if not uv:
    raise SystemExit("bubbles_sprites.py needs uv on PATH")

for name in names:
    image = images_dir / f"{name}.png"
    header = generated / "images" / "gen" / f"{name}_png.h"
    if not image.is_file():
        sys.exit(f"{image} is missing")
    if (
        header.exists()
        and header.stat().st_mtime >= image.stat().st_mtime
        and header.stat().st_mtime >= converter.stat().st_mtime
    ):
        continue
    header.parent.mkdir(parents=True, exist_ok=True)
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
