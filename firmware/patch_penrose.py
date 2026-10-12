"""PlatformIO pre-build script: writes the patched penrose.c that
src/hacks/penrose_short_pause.c includes. See tools/penrose_patch.py for why."""

import sys
from pathlib import Path

Import("env")  # noqa: F821

project = Path(env["PROJECT_DIR"])  # noqa: F821
sys.path.insert(0, str(project.parent / "tools"))
import penrose_patch

generated = Path(env.subst("$BUILD_DIR")) / "generated"  # noqa: F821
generated.mkdir(parents=True, exist_ok=True)

patched = penrose_patch.shorten_pauses(
    (project / "src" / "hacks" / "penrose" / "penrose.c").read_text(),
    penrose_patch.PAUSE_FRAMES,
)
target = generated / "penrose_patched.c"
if not target.exists() or target.read_text() != patched:
    target.write_text(patched)

env.Append(CPPPATH=[str(generated)])  # noqa: F821
