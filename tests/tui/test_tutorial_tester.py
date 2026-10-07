"""User flows of the tutorial's TSP tester, checked on what the screen shows."""

import re

from tui_driver import DELETE, DOWN, ENTER, F3, F4, F5, RIGHT, UP, Tui

INITIAL_COST = 29  # the initial tour 0-1-2-3-4 of five.tsp


def focus_seed(tui: Tui) -> None:
    """On the Run page, move the focus to the seed field and clear it."""
    tui.press(F5)
    for _ in range(10):
        if "[P Problem parameters]" in tui.text():
            break
        tui.press(DOWN)
    tui.press(DOWN)
    tui.keys(*[DELETE] * 24)


def set_seed(tui: Tui, seed: str) -> None:
    focus_seed(tui)
    tui.type(seed)
    tui.press(ENTER)


def test_startup_shows_the_input_and_the_seed(tui):
    tui.expect("TSP tester")
    tui.expect("five.tsp")
    tui.expect("[seed=2026]")
    tui.expect("COST -")
    tui.expect("Move [disabled]")


def test_initial_solution_passes_the_checks(tui):
    tui.press("I")
    tui.expect("Initial solution selected")
    assert tui.cost() == INITIAL_COST
    tui.expect_absent("[disabled]")

    tui.press(F3, "C")  # I opens the Move page; Check is on the I/O page
    tui.expect(re.compile(r"Check passed: \d+ semantic checks"))


def test_best_move_has_a_consistent_delta_and_applies(tui):
    tui.press("I", F4, "B")
    tui.expect("Selected best move")
    tui.expect("Delta check: OK")
    incremental = int(tui.expect(re.compile(r"Candidate \(incremental\): *(\d+)")).group(1))
    full = int(tui.expect(re.compile(r"Candidate \(full\): *(\d+)")).group(1))
    assert incremental == full < INITIAL_COST

    tui.press("A")
    tui.wait_until(lambda _: tui.cost() == full, what=f"COST {full}")


def test_seed_from_the_interface_is_reproducible(binary):
    def random_cost_with_seed(seed: str) -> int:
        with Tui(binary) as tui:
            tui.press("I")
            set_seed(tui, seed)
            tui.expect(f"[seed={seed}]")
            tui.expect(f"Seed set to {seed}")
            tui.press(F3, "R")
            tui.expect("Random solution selected")
            return tui.cost()

    assert random_cost_with_seed("42") == random_cost_with_seed("42")


def test_seed_reproduces_a_stochastic_run(binary):
    # From a random tour, a short Simulated Annealing run: the same seed (the
    # options' 2026) gives the same start and the same run, as on the command
    # line, in a Session and in REST.
    def annealed() -> str:
        with Tui(binary) as tui:
            tui.press("R")
            tui.expect("Random solution selected")
            tui.press(F5)
            tui.select("sa")
            tui.focus("P Problem parameters")
            tui.press(*[DOWN] * 10, RIGHT)  # the last row, then to evaluations
            tui.type("40")
            tui.focus("G Run selected", key=UP)  # out of the fields
            tui.press("G")
            tui.expect("Parameters of sa")
            tui.press(ENTER)
            result = tui.expect(re.compile(r"Runner completed: (sa: \d+ -> \d+)")).group(1)
            tui.press(F3, "q")
            return result

    assert annealed() == annealed()


def test_an_invalid_seed_is_rejected(tui):
    tui.press("I")
    set_seed(tui, "abc")
    tui.expect("Seed must be a non-negative integer: abc")
    tui.expect("[seed=2026]")


def test_help_toggles_and_q_quits(tui):
    tui.press("?")
    tui.expect("Keyboard help")
    tui.press("?")
    tui.wait_until(lambda s: "Keyboard help" not in s, what="help closed")

    tui.press("q")
    assert tui.wait_exit() == 0


def test_buttons_shortcuts_and_help_name_an_action_alike(tui):
    # The Move page's buttons, its shortcut line and the help: one label.
    tui.press("I")
    tui.expect("B Best  I First improving  F First")
    tui.expect("U Distribution")
    tui.press("?")
    tui.expect("Keyboard help")
    tui.expect("U Distribution")
    tui.expect("X in the progress window stops the run")
    # ? closes the help as Escape does, and cannot be read as the start of an
    # escape sequence on a slow runner, which left the help open and q unread.
    tui.press("?")
    tui.expect_absent("Keyboard help")


def test_a_problem_without_parameters_says_so(tui):
    tui.press("I", F5, "P")
    tui.expect("The problem has no parameters")
