"""PlatformIO pre-build script: writes the patched polyominoes.c that
src/hacks/polyominoes_iterative.c includes. See tools/polyominoes_patch.py for
why."""

import sys
from pathlib import Path

Import("env")  # noqa: F821

project = Path(env["PROJECT_DIR"])  # noqa: F821
sys.path.insert(0, str(project.parent / "tools"))
import polyominoes_patch

generated = Path(env.subst("$BUILD_DIR")) / "generated"  # noqa: F821
generated.mkdir(parents=True, exist_ok=True)

source = (project / "src" / "hacks" / "polyominoes" / "polyominoes.c").read_text()
patched = polyominoes_patch.make_the_flood_fill_iterative(source)
target = generated / "polyominoes_patched.c"
if not target.exists() or target.read_text() != patched:
    target.write_text(patched)

env.Append(CPPPATH=[str(generated)])  # noqa: F821
