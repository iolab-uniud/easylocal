"""User flows of the tutorial's TSP tester, checked on what the screen shows."""

import re

from tui_driver import DELETE, DOWN, ENTER, F3, F4, F5, Tui

INITIAL_COST = 29  # the initial tour 0-1-2-3-4 of five.tsp


def focus_seed(tui: Tui) -> None:
    """On the Run page, move the focus to the seed field and clear it."""
    tui.press(F5)
    for _ in range(10):
        if "[X Stop running]" in tui.text():
            break
        tui.press(DOWN)
    tui.press(DOWN)
    tui.press(*[DELETE] * 24)


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


def test_simulated_annealing_runs_to_completion(tui):
    tui.press("I", F5)
    tui.select("sa")
    tui.press("G")
    tui.expect("Runner completed: sa", timeout=60)
    final = int(tui.expect(re.compile(rf"sa: {INITIAL_COST} -> (\d+)")).group(1))
    assert final <= INITIAL_COST
    assert tui.cost() == final


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
