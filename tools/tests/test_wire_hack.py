import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import wire_hack as wh

REGISTRY = """\
extern const HackEntry pyro_hack;
XLOCKMORE_HACK(sphere, "Sphere");
extern const HackEntry old_hack;
extern const HackEntry fireball_hack;

const HackEntry *const g_hacks[] = {
    &pyro_hack,
    &old_hack};
"""

TESTS = """\
void test_registry_lists_hacks_in_order(void) {
  static const char *const expected[] = {"Pyro",
                                         "Old"};
  const int n = sizeof(expected) / sizeof(expected[0]);
}

static const uint64_t kBaseline[] = {
    0x1ull, /* Old: taken after looking at the frames */
};

void test_frames_of_every_hack_but_maze_match_main(void) {
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_gears_stack_use_on_the_host_has_not_run_away);
}
"""

NOTICES = """\
| File | Copyright | Licence |
| `firmware/src/hacks/old/old.c` | Someone | jwz permission notice (above) |

## Next section
"""


@pytest.fixture
def repo(tmp_path):
    (tmp_path / wh.UPSTREAM / "hacks/glx").mkdir(parents=True)
    (tmp_path / wh.UPSTREAM / "hacks/newhack.c").write_text("/* plain */\n")
    (tmp_path / wh.UPSTREAM / "hacks/glx/glhack.c").write_text("/* gl */\n")
    hacks = tmp_path / "firmware/src/hacks"
    hacks.mkdir(parents=True)
    (hacks / "registry.c").write_text(REGISTRY)
    tdir = tmp_path / "firmware/test/test_hacks"
    tdir.mkdir(parents=True)
    (tdir / "test_hacks.c").write_text(TESTS)
    (tmp_path / "THIRD_PARTY_NOTICES.md").write_text(NOTICES)
    return tmp_path


def read(repo, rel):
    return (repo / rel).read_text()


def test_the_source_is_copied_byte_for_byte(repo):
    wh.wire(repo, "newhack", "NewHack", "Copyright (c) 2001 A. Person")
    assert read(repo, "firmware/src/hacks/newhack/newhack.c") == "/* plain */\n"


def test_a_plain_hack_gets_an_extern_and_goes_last_in_the_button_order(repo):
    wh.wire(repo, "newhack", "NewHack", "c")
    reg = read(repo, "firmware/src/hacks/registry.c")
    assert reg.index("extern const HackEntry newhack_hack;") < reg.index("g_hacks[]")
    assert "    &old_hack,\n    &newhack_hack};" in reg


def test_an_xlockmore_hack_is_registered_by_the_macro(repo):
    wh.wire(repo, "newhack", "NewHack", "c", xlockmore=True)
    reg = read(repo, "firmware/src/hacks/registry.c")
    assert 'XLOCKMORE_HACK(newhack, "NewHack");' in reg
    assert "extern const HackEntry newhack_hack;" not in reg
    assert 'check_restarts_and_stops_do_not_leak("NewHack", 1);' in read(
        repo, "firmware/test/test_hacks/test_hacks.c"
    )


def test_the_tests_get_a_name_an_unpinned_hash_and_a_leak_test(repo):
    wh.wire(repo, "newhack", "NewHack", "c")
    t = read(repo, "firmware/test/test_hacks/test_hacks.c")
    assert '"Old",\n                                         "NewHack"};' in t
    assert "    0, /* NewHack: not yet pinned */\n};" in t
    assert 'check_restarts_and_stops_do_not_leak("NewHack", 0);' in t
    assert t.index("RUN_TEST(test_newhack_stops_do_not_leak)") < t.index(
        "test_gears_stack"
    )


def test_the_notices_row_follows_the_last_hack_row(repo):
    wh.wire(repo, "newhack", "NewHack", "Copyright (c) 2001 A. Person", notice="MIT")
    n = read(repo, "THIRD_PARTY_NOTICES.md")
    row = "| `firmware/src/hacks/newhack/newhack.c` | Copyright (c) 2001 A. Person | MIT |\n"
    assert n.index("old.c") < n.index(row) < n.index("## Next section")


def test_a_glx_hack_is_read_from_the_glx_directory(repo):
    wh.wire(repo, "glhack", "GlHack", "c", xlockmore=True, glx=True)
    assert read(repo, "firmware/src/hacks/glhack/glhack.c") == "/* gl */\n"


def test_it_refuses_a_hack_that_is_already_registered_and_changes_nothing(repo):
    (repo / wh.UPSTREAM / "hacks/old.c").write_text("/* old */\n")
    before = read(repo, "firmware/src/hacks/registry.c")
    with pytest.raises(ValueError, match="already"):
        wh.wire(repo, "old", "Old", "c")
    assert read(repo, "firmware/src/hacks/registry.c") == before


def test_a_name_inside_another_hacks_name_is_not_taken_for_a_duplicate(repo):
    (repo / wh.UPSTREAM / "hacks/ball.c").write_text("/* ball */\n")
    wh.wire(repo, "ball", "Ball", "c")
    assert "extern const HackEntry ball_hack;" in read(
        repo, "firmware/src/hacks/registry.c"
    )


def test_it_refuses_a_missing_source_and_a_changed_layout_without_writing(repo):
    with pytest.raises(FileNotFoundError):
        wh.wire(repo, "nope", "Nope", "c")
    (repo / "firmware/test/test_hacks/test_hacks.c").write_text("int main(void) {}\n")
    with pytest.raises(ValueError):
        wh.wire(repo, "newhack", "NewHack", "c")
    assert not (repo / "firmware/src/hacks/newhack").exists()
    assert "newhack" not in read(repo, "firmware/src/hacks/registry.c")


GUARDED_REGISTRY = """\
extern const HackEntry old_hack;

const HackEntry *const g_hacks[] = {
    &old_hack,
#ifdef HAVE_AFTER_DARK_TOASTERS
    &ad_flyingtoasters_hack,
#endif
    HYPNOWHEEL_PROBE_ENTRY
};
"""

GUARDED_TESTS = """\
void test_registry_lists_hacks_in_order(void) {
  static const char *const expected[] = {"Old",
#ifdef HAVE_AFTER_DARK_TOASTERS
                                         "ad-flyingtoasters",
#endif
  };
  const int n = sizeof(expected) / sizeof(expected[0]);
}

static const uint64_t kBaseline[] = {
    0x1ull, /* Old: taken after looking at the frames */
#ifdef HAVE_AFTER_DARK_TOASTERS
    0x2ull, /* ad-flyingtoasters */
#endif
};

void test_frames_of_every_hack_but_maze_match_main(void) {
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_gears_stack_use_on_the_host_has_not_run_away);
}
"""


def test_a_new_hack_goes_before_the_toasters_which_stay_last(repo):
    (repo / "firmware/src/hacks/registry.c").write_text(GUARDED_REGISTRY)
    (repo / "firmware/test/test_hacks/test_hacks.c").write_text(GUARDED_TESTS)
    wh.wire(repo, "newhack", "NewHack", "c")
    reg = read(repo, "firmware/src/hacks/registry.c")
    assert (
        "    &old_hack,\n    &newhack_hack,\n#ifdef HAVE_AFTER_DARK_TOASTERS\n" in reg
    )
    t = read(repo, "firmware/test/test_hacks/test_hacks.c")
    assert (
        '{"Old",\n                                         "NewHack",\n#ifdef HAVE_AFTER_DARK_TOASTERS\n'
        in t
    )
    assert (
        "    0x1ull, /* Old: taken after looking at the frames */\n"
        "    0, /* NewHack: not yet pinned */\n#ifdef HAVE_AFTER_DARK_TOASTERS\n" in t
    )
