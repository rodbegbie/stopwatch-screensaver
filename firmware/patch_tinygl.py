"""PlatformIO pre-build script: writes the patched TinyGL tree, and the one
translation unit that includes it, into the build directory. The tree in
src/tinygl/ stays byte-identical to upstream; see tools/tinygl_patch.py."""

import sys
from pathlib import Path

Import("env")  # noqa: F821

project = Path(env["PROJECT_DIR"])  # noqa: F821
sys.path.insert(0, str(project.parent / "tools"))
import tinygl_patch  # noqa: E402

source = project / "src" / "tinygl"
sources = {
    str(path.relative_to(source)): path.read_text(errors="surrogateescape")
    for sub in ("src", "include")
    for path in sorted((source / sub).rglob("*"))
    if path.is_file()
}
patched = tinygl_patch.patch_tree(sources)

generated = Path(env.subst("$BUILD_DIR")) / "generated"  # noqa: F821


def write_if_changed(path: Path, text: str) -> None:
    """Rewrites only a changed file, so the build does not recompile it."""
    path.parent.mkdir(parents=True, exist_ok=True)
    data = text.encode(errors="surrogateescape")
    if not path.exists() or path.read_bytes() != data:
        path.write_bytes(data)


for relative, text in patched.items():
    write_if_changed(generated / "tinygl" / relative, text)
write_if_changed(generated / "tinygl_unity.c", tinygl_patch.unity_source(patched))

# `GL/gl.h` and `zbuffer.h` resolve to the patched copy, and
# src/glshim/tinygl_build.c finds the unity file.
env.Append(  # noqa: F821
    CPPPATH=[str(generated / "tinygl" / "include"), str(generated)]
)
