"""PlatformIO pre-build script: writes the patched analogtv.c that
src/hacks/pong_tv.c includes. See tools/analogtv_patch.py for why."""

import sys
from pathlib import Path

Import("env")  # noqa: F821

project = Path(env["PROJECT_DIR"])  # noqa: F821
sys.path.insert(0, str(project.parent / "tools"))
import analogtv_patch

generated = Path(env.subst("$BUILD_DIR")) / "generated"  # noqa: F821
generated.mkdir(parents=True, exist_ok=True)

source = (project / "src" / "hacks" / "pong" / "analogtv.c").read_text()
patched = analogtv_patch.float_ghost_filter(source)
target = generated / "analogtv_patched.c"
if not target.exists() or target.read_text() != patched:
    target.write_text(patched)

# The patched copy lives in the build directory, so it finds analogtv.h by path.
env.Append(CPPPATH=[str(generated), str(project / "src" / "hacks" / "pong")])  # noqa: F821
