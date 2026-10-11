"""PlatformIO pre-build script: writes the copies of hacks with their decimal
literals made floats that src/hacks/*_single.c include. See
tools/float_literals.py for why."""

import sys
from pathlib import Path

Import("env")  # noqa: F821

project = Path(env["PROJECT_DIR"])  # noqa: F821
sys.path.insert(0, str(project.parent / "tools"))
import celtic_patch  # noqa: E402
import float_literals  # noqa: E402
import fireworkx_patch  # noqa: E402

generated = Path(env.subst("$BUILD_DIR")) / "generated"  # noqa: F821
generated.mkdir(parents=True, exist_ok=True)

for name in ("braid",) + ('euler2d', 'compass', 'fuzzyflakes', 'halftone', 'interaggregate', 'anemone', 'lmorph', 'hexadrop', 'fireworkx', 'starfish', 'goop', 'cwaves', 'hextrail', 'celtic'):
    source = (project / "src" / "hacks" / name / f"{name}.c").read_text()
    if name == "fireworkx":
        source = fireworkx_patch.scale_for_a_small_window(source)
    if name == "celtic":
        source = celtic_patch.compute_spline_position_once(source)
    patched = float_literals.float_literals(source)
    target = generated / f"{name}_floatlit.c"
    if not target.exists() or target.read_text() != patched:
        target.write_text(patched)

env.Append(CPPPATH=[str(generated)])  # noqa: F821
