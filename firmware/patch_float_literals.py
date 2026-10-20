"""PlatformIO pre-build script: writes the copies of hacks with their decimal
literals made floats that src/hacks/*_single.c include. See
tools/float_literals.py for why."""

import sys
from pathlib import Path

Import("env")  # noqa: F821

project = Path(env["PROJECT_DIR"])  # noqa: F821
sys.path.insert(0, str(project.parent / "tools"))
import binaryring_patch
import celtic_patch
import cubenetic_patch
import etruscanvenus_patch
import fireworkx_patch
import float_literals
import glmatrix_patch
import kaleidocycle_patch
import superquadrics_patch
import xspirograph_patch

generated = Path(env.subst("$BUILD_DIR")) / "generated"  # noqa: F821
generated.mkdir(parents=True, exist_ok=True)

for name in ("braid",) + (
    "euler2d",
    "compass",
    "fuzzyflakes",
    "halftone",
    "interaggregate",
    "anemone",
    "lmorph",
    "hexadrop",
    "fireworkx",
    "starfish",
    "goop",
    "cwaves",
    "hextrail",
    "celtic",
    "binaryring",
    "piecewise",
    "etruscanvenus",
    "lisa",
    "rotzoomer",
    "twang",
    "nerverot",
    "hyperball",
    "ccurve",
    "galaxy",
    "xspirograph",
    "vines",
    "hopalong",
    "whirlwindwarp",
    "binaryhorizon",
    "barcode",
    "discrete",
    "drift",
    "flame",
    "substrate",
    "bubble3d",
    "glmatrix",
    "cubenetic",
    "superquadrics",
):
    source = (project / "src" / "hacks" / name / f"{name}.c").read_text()
    if name == "fireworkx":
        source = fireworkx_patch.scale_for_a_small_window(source)
    if name == "celtic":
        source = celtic_patch.compute_spline_position_once(source)
        source = celtic_patch.fit_the_graph_to_the_screen(source)
        source = celtic_patch.scale_the_animation_step(source)
    if name == "etruscanvenus":
        source = etruscanvenus_patch.size_the_grid(source)
    if name == "cubenetic":
        source = cubenetic_patch.size_the_texture(source)
    if name == "glmatrix":
        source = glmatrix_patch.keep_the_wave_index_in_range(source)
        source = glmatrix_patch.shift_the_alpha_unsigned(source)
    if name == "superquadrics":
        source = superquadrics_patch.keep_myrand_in_range(source)
    if name == "bubble3d":
        source = source.replace(
            '#include "bubble3d.h"', '#include "hacks/bubble3d/bubble3d.h"'
        )
    if name == "xspirograph":
        source = xspirograph_patch.reduce_the_angles(source)
    if name in ("binaryring", "binaryhorizon"):
        source = binaryring_patch.float_random_scale(source)
    patched = float_literals.float_literals(source)
    target = generated / f"{name}_floatlit.c"
    if not target.exists() or target.read_text() != patched:
        target.write_text(patched)

# Patched copies that keep their double precision and their literals: only the
# one change.
for name, patch in (("kaleidocycle", kaleidocycle_patch.guard_the_zero_count),):
    patched = patch((project / "src" / "hacks" / name / f"{name}.c").read_text())
    target = generated / f"{name}_patched.c"
    if not target.exists() or target.read_text() != patched:
        target.write_text(patched)

env.Append(CPPPATH=[str(generated)])  # noqa: F821
