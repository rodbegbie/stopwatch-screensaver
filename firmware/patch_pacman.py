"""PlatformIO pre-build script: writes the patched pacman_level.c that
src/hacks/pacman_stdlevel.c includes. See tools/pacman_patch.py for why."""

import sys
from pathlib import Path

Import("env")  # noqa: F821

project = Path(env["PROJECT_DIR"])  # noqa: F821
sys.path.insert(0, str(project.parent / "tools"))
import pacman_patch  # noqa: E402

generated = Path(env.subst("$BUILD_DIR")) / "generated"  # noqa: F821
generated.mkdir(parents=True, exist_ok=True)

patched = pacman_patch.patch_pacman_level(
    (project / "src" / "hacks" / "pacman" / "pacman_level.c").read_text()
)
target = generated / "pacman_level_patched.c"
if not target.exists() or target.read_text() != patched:
    target.write_text(patched)

# The patched copy lives in the build directory, so the hack's own headers
# (pacman.h, pacman_level.h) are no longer beside it.
env.Append(CPPPATH=[str(generated), str(project / "src" / "hacks" / "pacman")])  # noqa: F821
