"""Say whether a pull request can change the firmware host tests' result.

Usage: `firmware_changed.py <base-ref>`. Prints `firmware=true` or
`firmware=false` for CI to append to `$GITHUB_OUTPUT`.

Anything under `tools/` counts except `tools/tests/`: the firmware build runs
the patch and sprite scripts there, and `xs_vendor.py` pins the xscreensaver
version the tests unpack. `uv.lock` counts only when it moves a package the
build uses, so a bump of ruff or ty alone does not.
"""

import subprocess
import sys
import tomllib

FIRMWARE_PREFIXES = ("firmware/", "tools/")
IGNORED_PREFIXES = ("tools/tests/",)
FIRMWARE_FILES = frozenset({".depot/workflows/ci.yml"})
LOCK_FILE = "uv.lock"

# PlatformIO runs the tests; Pillow and machfs are imported by the scripts the
# build runs (make_badge_font.py, after_dark_sprites.py).
BUILD_PACKAGES = frozenset({"platformio", "pillow", "machfs"})


def affects_firmware(paths: list[str]) -> bool:
    return any(
        path in FIRMWARE_FILES
        or (
            path.startswith(FIRMWARE_PREFIXES) and not path.startswith(IGNORED_PREFIXES)
        )
        for path in paths
    )


def build_package_versions(lock_text: str | None) -> dict[str, str] | None:
    if lock_text is None:
        return None
    packages = tomllib.loads(lock_text).get("package", [])
    return {p["name"]: p["version"] for p in packages if p["name"] in BUILD_PACKAGES}


def build_packages_changed(base_lock: str | None, head_lock: str | None) -> bool:
    return build_package_versions(base_lock) != build_package_versions(head_lock)


def git(*args: str) -> str:
    return subprocess.run(
        ["git", *args], check=True, capture_output=True, text=True
    ).stdout


def lock_at(ref: str) -> str | None:
    try:
        return git("show", f"{ref}:{LOCK_FILE}")
    except subprocess.CalledProcessError:
        return None


def firmware_changed(base_ref: str) -> bool:
    fork_point = git("merge-base", base_ref, "HEAD").strip()
    # --no-renames lists a moved file's old path too, so moving firmware/ code
    # out of the tree still counts.
    paths = git("diff", "--name-only", "--no-renames", fork_point, "HEAD").split()
    if affects_firmware(paths):
        return True
    return LOCK_FILE in paths and build_packages_changed(
        lock_at(fork_point), lock_at("HEAD")
    )


def main() -> None:
    print(f"firmware={str(firmware_changed(sys.argv[1])).lower()}")


if __name__ == "__main__":
    main()
