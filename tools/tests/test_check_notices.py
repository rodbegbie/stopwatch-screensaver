import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import check_notices as cn  # noqa: E402

JWZ = """/* xscreensaver, Copyright (c) 1992-2008 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee.
 */
int x;
"""

MIT = """/* Copyright (C) Someone 1998
Permission is hereby granted, free of charge, to any person obtaining
a copy of this software
*/
int x;
"""


def setup(tmp_path, files: dict[str, str], notices: str):
    hacks = tmp_path / "firmware" / "src" / "hacks"
    for rel, text in files.items():
        f = hacks / rel
        f.parent.mkdir(parents=True, exist_ok=True)
        f.write_text(text)
    md = tmp_path / "THIRD_PARTY_NOTICES.md"
    md.write_text(notices)
    return tmp_path / "firmware" / "src", md


def test_file_with_jwz_notice_and_listing_passes(tmp_path):
    src, md = setup(tmp_path, {"pyro/pyro.c": JWZ}, "| pyro.c | x | y |")
    assert cn.check(src, md) == []


def test_mit_style_notice_passes(tmp_path):
    src, md = setup(tmp_path, {"ifs/ifs.c": MIT}, "| ifs.c | x | y |")
    assert cn.check(src, md) == []


def test_notice_wrapped_across_comment_lines_passes(tmp_path):
    wrapped = """/*
 *  Copyright (c) 1994, by Carnegie Mellon University.  Permission to use,
 *  copy, modify, distribute, and sell this software and its documentation
 *  for any purpose is hereby granted without fee.
 */
int x;
"""
    src, md = setup(tmp_path, {"pedal/pedal.c": wrapped}, "| pedal.c | x | y |")
    assert cn.check(src, md) == []


def test_naughton_xlock_notice_passes(tmp_path):
    naughton = """/*-
 * Copyright (c) 1991 by Patrick J. Naughton.
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies.
 */
int x;
"""
    src, md = setup(tmp_path, {"hopalong/hopalong.c": naughton}, "| hopalong.c | x | y |")
    assert cn.check(src, md) == []


def test_file_without_notice_is_reported(tmp_path):
    src, md = setup(tmp_path, {"pyro/pyro.c": "int x;\n"}, "pyro")
    problems = cn.check(src, md)
    assert len(problems) == 1 and "pyro.c" in problems[0]


def test_file_not_listed_in_notices_is_reported(tmp_path):
    src, md = setup(tmp_path, {"pyro/pyro.c": JWZ}, "nothing relevant here")
    problems = cn.check(src, md)
    assert len(problems) == 1 and "not listed" in problems[0]


def test_notice_text_past_line_40_is_not_accepted(tmp_path):
    text = "\n" * 45 + JWZ
    src, md = setup(tmp_path, {"pyro/pyro.c": text}, "pyro.c")
    assert len(cn.check(src, md)) == 1


def test_non_c_files_ignored(tmp_path):
    src, md = setup(tmp_path, {"pyro/NOTES.md": "no notice"}, "")
    assert cn.check(src, md) == []


def test_xs_support_dir_is_scanned(tmp_path):
    src, md = setup(tmp_path, {}, "hsv.c")
    support = src / "xs_support"
    support.mkdir(parents=True)
    (support / "hsv.c").write_text("int x;\n")
    assert len(cn.check(src, md)) == 1


def test_main_exit_codes(tmp_path, capsys):
    src, md = setup(tmp_path, {"pyro/pyro.c": JWZ}, "pyro.c")
    assert cn.main(["--src", str(src), "--notices", str(md)]) == 0
    md.write_text("")
    assert cn.main(["--src", str(src), "--notices", str(md)]) == 1
    assert "not listed" in capsys.readouterr().out


def test_top_level_files_in_hacks_are_ours_and_ignored(tmp_path):
    src, md = setup(tmp_path, {"registry.c": "int x;\n"}, "")
    assert cn.check(src, md) == []
