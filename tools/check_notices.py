"""Check that every copied third-party source has a licence notice and is
listed in THIRD_PARTY_NOTICES.md.

Run with: uv run tools/check_notices.py
"""

import argparse
import sys
from pathlib import Path

# Copied third-party code: one directory per hack under hacks/ (top-level
# files in hacks/ are our own, e.g. the registry), plus xs_support/.
SUFFIXES = {".c", ".h", ".cpp"}
HEADER_LINES = 40
NOTICE_PHRASES = (
    "permission to use, copy, modify, distribute, and sell this software",
    "permission is hereby granted, free of charge",
)


def has_notice(text: str) -> bool:
    head = "\n".join(text.splitlines()[:HEADER_LINES]).lower()
    return "copyright" in head and any(p in head for p in NOTICE_PHRASES)


def check(src_dir: Path, notices_md: Path) -> list[str]:
    notices = notices_md.read_text() if notices_md.exists() else ""
    problems = []
    candidates = sorted((src_dir / "hacks").glob("*/**/*")) + sorted(
        (src_dir / "xs_support").rglob("*")
    )
    for path in candidates:
        if path.suffix not in SUFFIXES or not path.is_file():
            continue
        rel = path.relative_to(src_dir)
        if not has_notice(path.read_text(errors="replace")):
            problems.append(f"{rel}: no licence notice in first {HEADER_LINES} lines")
        if path.name not in notices and path.stem not in notices:
            problems.append(f"{rel}: not listed in {notices_md.name}")
    return problems


def main(argv: list[str]) -> int:
    root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--src", default=str(root / "firmware" / "src"))
    parser.add_argument("--notices", default=str(root / "THIRD_PARTY_NOTICES.md"))
    args = parser.parse_args(argv)
    problems = check(Path(args.src), Path(args.notices))
    for problem in problems:
        print(problem)
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
