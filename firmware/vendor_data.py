"""PlatformIO pre-build script: copies data files the GL hacks include from
vendor/ into the build directory, where the wrappers find them. They carry no
licence text of their own, so they are never committed (see
THIRD_PARTY_NOTICES.md)."""

import sys
from pathlib import Path

Import("env")  # noqa: F821

repo = Path(env["PROJECT_DIR"]).parent  # noqa: F821
sys.path.insert(0, str(repo / "tools"))
import xs_vendor

glx = xs_vendor.upstream(repo) / "hacks" / "glx"
generated = Path(env.subst("$BUILD_DIR")) / "generated"  # noqa: F821

for name in ("curlicue.h", "graphstat.h"):
    source = glx / name
    target = generated / name
    if not source.is_file():
        sys.exit(f"{source} is missing: run `uv run tools/fetch_xscreensaver.py`")
    data = source.read_bytes()
    if not target.exists() or target.read_bytes() != data:
        generated.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)

env.Append(CPPPATH=[str(generated)])  # noqa: F821
