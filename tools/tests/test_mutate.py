import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import mutate as mu  # noqa: E402


@pytest.fixture
def project(tmp_path):
    (tmp_path / "thing.py").write_text("VALUE = 1\nNOTE = 'x'\n")
    (tmp_path / "check.py").write_text(
        "ns = {}\nexec(open('thing.py').read(), ns)\n"
        "assert ns['VALUE'] == 1, 'value changed'\n"
    )
    return tmp_path


def run(project, mutations):
    return mu.run_mutations(
        project / "thing.py", mutations, [sys.executable, "check.py"], project
    )


def test_a_mutation_the_tests_notice_is_caught(project):
    ((old, status, detail),) = run(project, [("VALUE = 1", "VALUE = 2")])
    assert status == "CAUGHT"
    assert "value changed" in detail


def test_a_mutation_that_changes_nothing_observable_survives(project):
    ((_, status, _),) = run(project, [("NOTE = 'x'", "NOTE = 'y'")])
    assert status == "SURVIVED"


def test_a_mutation_that_breaks_the_build_is_not_counted_as_caught(project):
    ((_, status, _),) = run(project, [("VALUE = 1", "VALUE = (")])
    assert status == "BUILD ERROR"


def test_each_mutation_starts_from_the_original_and_the_file_is_restored(project):
    results = run(project, [("VALUE = 1", "VALUE = 2"), ("NOTE = 'x'", "NOTE = 'y'")])
    assert [s for _, s, _ in results] == ["CAUGHT", "SURVIVED"]
    assert (project / "thing.py").read_text() == "VALUE = 1\nNOTE = 'x'\n"


def test_a_pattern_that_is_not_in_the_file_is_an_error_and_the_file_is_untouched(
    project,
):
    with pytest.raises(ValueError, match="not in"):
        run(project, [("VALUE = 1", "VALUE = 2"), ("MISSING", "x")])
    assert (project / "thing.py").read_text() == "VALUE = 1\nNOTE = 'x'\n"


def test_only_the_first_occurrence_is_replaced(project):
    (project / "thing.py").write_text("VALUE = 1\nVALUE = 1\n")
    run(project, [("VALUE = 1", "VALUE = 9")])
    assert (project / "thing.py").read_text() == "VALUE = 1\nVALUE = 1\n"
