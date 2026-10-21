"""PlatformIO pre-build script: converts xscreensaver's three TV test cards for
XAnalogTV into blobs in the build directory. Their licence is not stated, so
they are never committed. They are shrunk to fit 400 pixels (the board holds
each as a 32-bit image: 640 by 480 would be 1.2 MB, 768 by 576 1.8 MB)."""

import os
import shutil
import subprocess
import sys
from pathlib import Path

Import("env")  # noqa: F821

repo = Path(env["PROJECT_DIR"]).parent  # noqa: F821
sys.path.insert(0, str(repo / "tools"))
import xs_vendor

images_dir = xs_vendor.upstream(repo) / "hacks" / "images"
converter = repo / "tools" / "make_logo_blob.py"
generated = Path(env.subst("$BUILD_DIR")) / "generated"  # noqa: F821

for name in ("testcard_rca", "testcard_pm5544", "testcard_bbcf"):
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
    uv = shutil.which("uv")
    if not uv:
        raise SystemExit("testcards.py needs uv on PATH")
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
            "--size",
            "400",
            "--deflate",
            "-o",
            str(partial),
        ],
        check=True,
    )
    os.replace(partial, header)

env.Append(CPPPATH=[str(generated)])  # noqa: F821
