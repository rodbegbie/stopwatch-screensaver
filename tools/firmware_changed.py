"""Say whether a pull request can change the firmware host tests' result.

Reads changed file paths on stdin, one per line, and prints `firmware=true` or
`firmware=false` for CI to append to `$GITHUB_OUTPUT`.

Anything under `tools/` counts except `tools/tests/`: the firmware build runs
the patch and sprite scripts there, and `xs_vendor.py` pins the xscreensaver
version the tests unpack.
"""

import sys

FIRMWARE_PREFIXES = ("firmware/", "tools/")
IGNORED_PREFIXES = ("tools/tests/",)
FIRMWARE_FILES = frozenset({".depot/workflows/ci.yml"})


def affects_firmware(paths: list[str]) -> bool:
    return any(
        path in FIRMWARE_FILES
        or (
            path.startswith(FIRMWARE_PREFIXES) and not path.startswith(IGNORED_PREFIXES)
        )
        for path in paths
    )


def main() -> None:
    paths = [line.strip() for line in sys.stdin if line.strip()]
    print(f"firmware={str(affects_firmware(paths)).lower()}")


if __name__ == "__main__":
    main()
