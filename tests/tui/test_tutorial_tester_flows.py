"""User flows of the tutorial's TSP tester beyond the quick tour: files,
viewers, every move action and diagnostic, runs that are stopped."""

import re

import pytest

from tui_driver import BACKSPACE, ENTER, ESCAPE, F1, F2, F3, F4, RIGHT, UP, Tui

# five.tsp, the tutorial instance
DISTANCE = [
    [0, 2, 9, 10, 7],
    [2, 0, 6, 4, 3],
    [9, 6, 0, 8, 5],
    [10, 4, 8, 0, 6],
    [7, 3, 5, 6, 0],
]


def tour_length(order: list[int]) -> int:
    return sum(DISTANCE[a][b] for a, b in zip(order, order[1:] + order[:1]))


def counts(tui: Tui, *names: str) -> dict[str, int]:
    """The `Name: value` lines of the diagnostic on screen."""
    return {name: int(tui.expect(re.compile(rf"{name}: *(\d+)")).group(1)) for name in names}


@pytest.fixture
def started(binary, tmp_path):
    """The tester run in an empty directory, closed with q at the end."""
    with Tui(binary, cwd=tmp_path) as driver:
        yield driver
    assert driver.exit_status == 0, "the tester did not quit with q"


def test_actions_ask_for_what_they_need(tui):
    tui.press(F3, "C")
    tui.expect("Check: choose or load a solution first")
    tui.press("W")
    tui.expect("Save solution: choose or load a solution first")
    tui.press(F4)
    tui.expect("Move and Run require a loaded instance and a valid solution")

    tui.press("I", "A")  # I opens the Move page, where no move is selected yet
    tui.expect("Apply move: select a move first")


def test_viewers_show_the_input_and_the_solution(tui):
    tui.press(F2)
    tui.expect("<no solution selected>")
    tui.press(ESCAPE)
    tui.wait_until(lambda s: "<no solution selected>" not in s, what="viewer closed")
    tui.press(F1)
    tui.expect(re.compile(r"File: .*five\.tsp"))
    tui.expect("<not printable")  # the tutorial's Tsp has no describe()
    tui.press(ESCAPE)

    tui.press("I", "S")
    tui.expect("0 1 2 3 4")
    # The cost components: TourLength's value and its describe text, the
    # edges of the tour 0 1 2 3 4 that it adds up.
    tui.expect("TourLength: 29")
    tui.expect("    2 + 6 + 8 + 6 + 7")
    tui.press(ESCAPE)
    tui.wait_until(lambda s: "0 1 2 3 4" not in s, what="viewer closed")


def test_every_move_action_reports_its_move(tui):
    tui.press("I")  # the Move page
    for key, status in [
        ("F", "Selected first move"),
        ("N", "Selected next move"),
        ("R", "Selected random move"),
        ("I", "Selected first improving move"),
    ]:
        tui.press(key)
        tui.expect(status)
        tui.expect("Delta check: OK")

    candidate = int(tui.expect(re.compile(r"Candidate \(full\): *(\d+)")).group(1))
    tui.press("A")
    tui.expect("Move applied")
    tui.wait_until(lambda _: tui.cost() == candidate, what=f"COST {candidate}")


def test_neighborhood_diagnostics(tui):
    tui.press("I", "P")
    listed = int(tui.expect(re.compile(r"Neighbors: (\d+)")).group(1))
    assert len(re.findall(r"2-opt\(\d+, \d+\) => \d+", tui.text())) == listed
    tui.press(ESCAPE)
    tui.expect("Neighborhood listed")

    tui.press("T")
    stats = counts(tui, "Moves", "Improving", "Sideways", "Worsening", "Invalid")
    assert stats["Moves"] == listed
    assert stats["Moves"] == sum(stats[k] for k in ("Improving", "Sideways", "Worsening", "Invalid"))
    tui.press(ESCAPE)
    tui.expect("Neighborhood statistics computed")

    tui.press("C")
    assert counts(tui, "Mismatches", "Invalid") == {"Mismatches": 0, "Invalid": 0}
    tui.press(ESCAPE)
    tui.expect("Neighborhood costs consistent")

    tui.press("D")
    assert counts(tui, "Null moves", "Repeated states") == {"Null moves": 0, "Repeated states": 0}
    tui.press(ESCAPE)
    tui.expect("Move independence check completed")

    tui.press("U")
    assert counts(tui, "Neighborhood size", "Outside neighborhood") == {
        "Neighborhood size": listed, "Outside neighborhood": 0}
    tui.press(ESCAPE)
    tui.expect("Random move distribution sampled")


def test_a_solution_is_saved_and_loaded_back_through_the_browser(started, tmp_path):
    tui = started
    solution = tmp_path / "tour.sol"
    solution.write_text("0 2 4 1 3\n")

    tui.press(F3)
    tui.focus("Browse...")  # the one of the solutions
    tui.press(ENTER)
    tui.expect("tour.sol")
    tui.press(ENTER)
    tui.expect("Selected file: tour.sol")

    tui.press("L")  # Shift-L
    tui.expect("Loaded solution: tour.sol")
    assert tui.cost() == tour_length([0, 2, 4, 1, 3])

    tui.press(F3, "R")
    tui.expect("Random solution selected")
    random_cost = tui.cost()
    tui.press(F3, "W")
    tui.expect("Saved solution: tour.sol")
    saved = [int(city) for city in solution.read_text().split()]
    assert sorted(saved) == list(range(5))
    assert tour_length(saved) == random_cost


def browsing(tui: Tui, directory: str) -> None:
    """Wait for the file browser to show `directory`, displayed relative to
    the working directory (the repository, or a build directory under CTest)."""
    tui.expect(re.compile(rf"│(?:\.\./)*{re.escape(directory)} +│"))


def test_an_input_is_browsed_and_loaded(tui):
    tui.focus("L Load input")
    tui.focus("Browse...", RIGHT)
    tui.press(ENTER)
    browsing(tui, "examples/tutorial")  # the instance's directory

    tui.press(BACKSPACE)  # to the parent
    browsing(tui, "examples")
    tui.select("[dir] tutorial/")
    tui.press(ENTER)
    browsing(tui, "examples/tutorial")

    tui.focus("Open")  # below the list, then the Up button beside it
    tui.focus("Up", RIGHT)
    tui.press(ENTER)
    browsing(tui, "examples")
    tui.press(UP)  # back to the list
    tui.select("[dir] tutorial/")
    tui.press(ENTER)

    tui.select("      five.tsp")
    tui.press(ENTER)
    tui.expect(re.compile(r"Selected file: .*five\.tsp"))

    tui.press("l")
    tui.expect(re.compile(r"Loaded input: .*five\.tsp; choose a solution"))
    tui.expect("Move [disabled]")


def test_the_loaded_input_is_named_until_another_is_loaded(tui):
    # Another file selected, not loaded: the header and the Input viewer still
    # name five.tsp, the Input loaded.
    tui.focus("L Load input")
    tui.focus("Browse...", RIGHT)
    tui.press(ENTER)
    browsing(tui, "examples/tutorial")
    tui.select("      annealing.toml")
    tui.press(ENTER)
    tui.expect(re.compile(r"Selected file: .*annealing\.toml"))
    tui.expect(re.compile(r"\|  five\.tsp  \[seed="))
    tui.expect_absent("annealing.toml  [seed=")
    tui.press(F1)
    tui.expect(re.compile(r"File: .*five\.tsp"))
    tui.press(ESCAPE)


def test_a_broken_solution_file_is_reported(started, tmp_path):
    tui = started
    (tmp_path / "solutions").mkdir()
    (tmp_path / "solutions" / "broken.sol").write_text("0 1 x\n")

    tui.focus("Browse...")
    tui.press(ENTER)
    tui.expect("[dir] solutions/")
    tui.press(ENTER)  # into the directory
    tui.expect("broken.sol")
    tui.press(ENTER)
    tui.expect("Selected file: solutions/broken.sol")

    tui.press("L")
    tui.expect("Load solution: invalid tour")
    tui.expect("COST -")
