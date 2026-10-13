"""PlatformIO pre-build script: writes generated/teapot/, an unchanged copy of
xs_support/glx/teapot.c beside a deflated teapot2.h, for glshim/glx_teapot.c
to include. See tools/teapot_table.py."""

import sys
from pathlib import Path

Import("env")  # noqa: F821

project = Path(env["PROJECT_DIR"])  # noqa: F821
sys.path.insert(0, str(project.parent / "tools"))
import teapot_table

glx = project / "src" / "xs_support" / "glx"
generated = Path(env.subst("$BUILD_DIR")) / "generated"  # noqa: F821
teapot_table.main(
    [str(glx / "teapot.c"), str(glx / "teapot2.h"), str(generated / "teapot")]
)

env.Append(CPPPATH=[str(generated)])  # noqa: F821
