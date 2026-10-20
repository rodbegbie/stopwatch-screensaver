"""PlatformIO pre-build script: rasterises pixChicago.ttf into a GFXfont header
(badge_font/pix_chicago.h in the build directory) that the name badge draws
with. The font's licence is unclear, so it is never committed: it is read from
vendor/pixChicago.ttf, or the path in PIX_CHICAGO_TTF. When it is absent the
badge keeps M5GFX's Font8x8C64 (HAVE_PIX_CHICAGO stays undefined). A present
but unusable font fails the build. Needs uv, because Pillow is not in
PlatformIO's Python."""

import hashlib
import os
import shutil
import subprocess
import sys
from pathlib import Path

Import("env")  # noqa: F821

repo = Path(env["PROJECT_DIR"]).parent  # noqa: F821
ttf = Path(os.environ.get("PIX_CHICAGO_TTF") or "vendor/pixChicago.ttf")
if not ttf.is_absolute():
    ttf = repo / ttf
tool = repo / "tools" / "make_badge_font.py"
generated = Path(env.subst("$BUILD_DIR")) / "generated"  # noqa: F821
header = generated / "badge_font" / "pix_chicago.h"
PIXEL_SIZE = "8"

if not ttf.is_file():
    print(f"badge: no pixChicago.ttf at {ttf}; the name badge keeps Font8x8C64")
else:
    stamp = header.with_suffix(".stamp")
    inputs = "\n".join(
        [
            str(ttf.resolve()),
            hashlib.sha256(ttf.read_bytes()).hexdigest(),
            hashlib.sha256(tool.read_bytes()).hexdigest(),
            PIXEL_SIZE,
        ]
    )
    if not header.exists() or not stamp.exists() or stamp.read_text() != inputs:
        uv = shutil.which("uv")
        if not uv:
            raise SystemExit("badge_font.py needs uv on PATH to rasterise the font")
        header.parent.mkdir(parents=True, exist_ok=True)
        result = subprocess.run(
            [
                uv,
                "run",
                str(tool),
                str(ttf),
                "--name",
                "pix_chicago",
                "--size",
                PIXEL_SIZE,
                "-o",
                str(header),
            ],
            capture_output=True,
            text=True,
            check=False,
        )
        if result.returncode != 0:
            header.unlink(missing_ok=True)
            sys.exit(
                result.stderr.strip() or f"{tool.name} failed ({result.returncode})"
            )
        stamp.write_text(inputs)
    env.Append(  # noqa: F821
        CPPPATH=[str(generated)], CPPDEFINES=["HAVE_PIX_CHICAGO"]
    )
