"""PlatformIO pre-build script: writes the patched maze.c that
src/hacks/maze_small.c includes. See tools/maze_patch.py for why."""

import sys
from pathlib import Path

Import("env")  # noqa: F821

project = Path(env["PROJECT_DIR"])  # noqa: F821
sys.path.insert(0, str(project.parent / "tools"))
import maze_patch  # noqa: E402

generated = Path(env.subst("$BUILD_DIR")) / "generated"  # noqa: F821
generated.mkdir(parents=True, exist_ok=True)

patched = maze_patch.patch_maze(
    (project / "src" / "hacks" / "maze" / "maze.c").read_text(),
    maze_patch.MAZE_LIMIT,
)
target = generated / "maze_patched.c"
if not target.exists() or target.read_text() != patched:
    target.write_text(patched)

env.Append(CPPPATH=[str(generated)])  # noqa: F821
