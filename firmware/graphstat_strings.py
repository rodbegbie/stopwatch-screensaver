"""PlatformIO pre-build script: writes graphstat.h, Graphstat's word list as a C
string, from vendor/'s graphstat.txt into the build directory, as upstream's
Makefile does with utils/bin2c. The list is never committed."""

import sys
from pathlib import Path

Import("env")  # noqa: F821

repo = Path(env["PROJECT_DIR"]).parent  # noqa: F821
sys.path.insert(0, str(repo / "tools"))
import text_to_c  # noqa: E402

source = repo / "vendor" / "xscreensaver-6.16" / "hacks" / "glx" / "graphstat.txt"
generated = Path(env.subst("$BUILD_DIR")) / "generated"  # noqa: F821
header = generated / "graphstat.h"

if not source.is_file():
    sys.exit(f"{source} is missing: run `uv run tools/fetch_xscreensaver.py`")
text = text_to_c.c_string(source.read_bytes(), "graphstat")
if not header.exists() or header.read_text() != text:
    generated.mkdir(parents=True, exist_ok=True)
    header.write_text(text)

env.Append(CPPPATH=[str(generated)])  # noqa: F821
