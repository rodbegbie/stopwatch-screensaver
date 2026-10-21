"""PlatformIO pre-build script: writes the patched analogtv.c that
src/hacks/analogtv_tv.c includes (see tools/analogtv_patch.py for why), and the
patched filmleader.c that src/hacks/filmleader_tv.c includes (see
tools/filmleader_patch.py)."""

import sys
from pathlib import Path

Import("env")  # noqa: F821

project = Path(env["PROJECT_DIR"])  # noqa: F821
sys.path.insert(0, str(project.parent / "tools"))
import analogtv_patch
import filmleader_patch

generated = Path(env.subst("$BUILD_DIR")) / "generated"  # noqa: F821
generated.mkdir(parents=True, exist_ok=True)

source = (project / "src" / "hacks" / "analogtv" / "analogtv.c").read_text()
patched = analogtv_patch.load_ximage_without_row_buffers(
    analogtv_patch.step_the_line_loop(analogtv_patch.float_ghost_filter(source))
)
target = generated / "analogtv_patched.c"
if not target.exists() or target.read_text() != patched:
    target.write_text(patched)

filmleader = (project / "src" / "hacks" / "filmleader" / "filmleader.c").read_text()
patched = filmleader_patch.measure_the_string_it_was_given(
    filmleader_patch.square_canvas(filmleader)
)
target = generated / "filmleader_patched.c"
if not target.exists() or target.read_text() != patched:
    target.write_text(patched)

# The patched copy lives in the build directory, so it finds analogtv.h by path.
env.Append(CPPPATH=[str(generated), str(project / "src" / "hacks" / "analogtv")])  # noqa: F821
