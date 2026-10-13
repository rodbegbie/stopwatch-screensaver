import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import fetch_xscreensaver as fx
import xs_vendor

ROOT = Path(__file__).resolve().parents[2]
PINNED_PATH = re.compile(r"xscreensaver-\d+\.\d+")


def test_fetch_surveys_the_vendored_version():
    assert fx.SURVEYED_VERSION == xs_vendor.VERSION


def test_upstream_dir_names_the_version():
    assert (
        xs_vendor.upstream(ROOT)
        == ROOT / "vendor" / f"xscreensaver-{xs_vendor.VERSION}"
    )


def test_scripts_take_the_vendor_path_from_xs_vendor():
    scripts = [
        *ROOT.glob("firmware/*.py"),
        *ROOT.glob("tools/*.py"),
        *ROOT.glob("tools/tests/test_*_patch.py"),
        ROOT / "tools/tests/test_wire_hack.py",
        ROOT / "docs/pipes-lab/build.py",
    ]
    pinned = [
        str(p.relative_to(ROOT))
        for p in scripts
        if p.name != "xs_vendor.py" and PINNED_PATH.search(p.read_text())
    ]
    assert pinned == []
