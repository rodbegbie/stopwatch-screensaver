"""Lists what an xscreensaver release changes for this firmware.

    uv run tools/vendor_diff.py vendor/xscreensaver-<old> vendor/xscreensaver-<new>

Prints four lists: files in `firmware/src/` copied byte for byte from the old
release that the new one changes or removes, data the build reads from
`vendor/` (a non-C file under `hacks/` that a script in `firmware/` or `tools/`
names) that changed or went, and hacks new in this release (a new
`hacks/config/<name>.xml`). See `.claude/skills/update-xscreensaver/`.
"""

import argparse
import hashlib
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


@dataclass
class Report:
    changed: list[tuple[str, str]] = field(default_factory=list)
    removed: list[tuple[str, str]] = field(default_factory=list)
    build_inputs: list[str] = field(default_factory=list)
    new_hacks: list[str] = field(default_factory=list)


def _sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _files(tree: Path) -> list[Path]:
    return sorted(p for p in tree.rglob("*") if p.is_file())


def _copied(root: Path, old: Path, new: Path, report: Report) -> None:
    by_hash: dict[str, list[Path]] = {}
    for p in _files(old):
        by_hash.setdefault(_sha(p), []).append(p.relative_to(old))
    for ours in _files(root / "firmware" / "src"):
        for rel in by_hash.get(_sha(ours), []):
            pair = (str(ours.relative_to(root)), str(rel))
            theirs = new / rel
            if not theirs.exists():
                report.removed.append(pair)
            elif _sha(theirs) != _sha(ours):
                report.changed.append(pair)


def _build_inputs(root: Path, old: Path, new: Path, report: Report) -> None:
    scripts = "\n".join(
        p.read_text() for p in [*root.glob("firmware/*.py"), *root.glob("tools/*.py")]
    )
    for p in _files(old / "hacks"):
        if p.suffix == ".c" or p.parent.name == "config":
            continue
        named = re.search(
            rf"""["']({re.escape(p.name)}|{re.escape(p.stem)})["']""", scripts
        )
        if not named:
            continue
        rel = p.relative_to(old)
        theirs = new / rel
        if not theirs.exists():
            report.build_inputs.append(f"{rel} (removed)")
        elif _sha(theirs) != _sha(p):
            report.build_inputs.append(f"{rel} (changed)")


def compare(root: Path, old: Path, new: Path) -> Report:
    report = Report()
    _copied(root, old, new, report)
    _build_inputs(root, old, new, report)
    report.new_hacks = sorted(_configs(new) - _configs(old))
    return report


def _configs(tree: Path) -> set[str]:
    return {p.stem for p in (tree / "hacks" / "config").glob("*.xml")}


def _section(title: str, lines: list[str]) -> None:
    print(f"{title}: {len(lines)}")
    for line in lines:
        print(f"  {line}")


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("old", help="the release the firmware was copied from")
    parser.add_argument("new", help="the release to move to")
    parser.add_argument("--root", default=str(ROOT), help="the repository")
    args = parser.parse_args(argv)
    for tree in (args.old, args.new):
        if not _configs(Path(tree)):
            print(f"error: {tree} is not an xscreensaver source tree", file=sys.stderr)
            return 1
    report = compare(Path(args.root), Path(args.old), Path(args.new))
    _section(
        "copied files changed upstream", [f"{a} <- {b}" for a, b in report.changed]
    )
    _section(
        "copied files removed upstream", [f"{a} <- {b}" for a, b in report.removed]
    )
    _section("build inputs changed or removed", report.build_inputs)
    _section("new hacks", report.new_hacks)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
