"""User flows of the tutorial's TSP tester, checked on what the screen shows."""

import re

from tui_driver import BACKSPACE, DELETE, DOWN, ENTER, ESCAPE, F3, F4, F5, TAB, UP, Tui

INITIAL_COST = 29  # the initial tour 0-1-2-3-4 of five.tsp


def focus_seed(tui: Tui) -> None:
    """On the Run page, move the focus to the seed field and clear it."""
    tui.press(F5)
    for _ in range(10):
        if "[P Problem parameters]" in tui.text():
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
    tui.expect("Parameters of sa")  # confirmed as they are
    tui.press(ENTER)
    tui.expect("Runner completed: sa", timeout=60)
    final = int(tui.expect(re.compile(rf"sa: {INITIAL_COST} -> (\d+)")).group(1))
    assert final <= INITIAL_COST
    assert tui.cost() == final


def test_runner_parameters_are_checked_and_kept(tui):
    tui.press("I", F5)
    tui.select("sa")
    tui.press("G")
    tui.expect("Parameters of sa")

    # The third field is temperature.cooling_rate, 0.95: an invalid value runs
    # nothing.
    tui.press(TAB, TAB, *[BACKSPACE] * 4)
    tui.type("2")
    tui.press(ENTER)
    tui.expect("cooling_rate must be finite and in the open interval (0, 1)")
    tui.expect_absent("Runner executing")

    tui.press(BACKSPACE)
    tui.type("0.9")
    tui.press(ENTER)
    tui.expect("Runner completed: sa", timeout=60)
    tui.expect("runners.sa.temperature.cooling_rate = 0.9 (was 0.95)")

    # The window opens again with the value set, and Esc leaves it unchanged.
    tui.press("G")
    tui.expect("Parameters of sa")
    tui.expect("cooling_rate  *")
    tui.press(ESCAPE)
    tui.wait_until(lambda screen: "Parameters of sa" not in screen, what="the window closed")


def set_target(tui: Tui, target: str) -> None:
    """On the Run page, type the target cost and select fi again."""
    tui.press(F5)
    tui.focus("P Problem parameters")  # through the runner list
    tui.press(*[DOWN] * 6)  # to the last field, past the seed
    tui.type(target)
    tui.select("fi", key=UP)


def test_a_target_cost_stops_the_run(tui):
    # First Improvement would reach 26; the initial tour already meets 29.
    tui.press("I")
    set_target(tui, str(INITIAL_COST))
    tui.expect(f"current cost: {INITIAL_COST}")  # the syntax, by example
    tui.press("G")
    tui.expect("Parameters of fi")
    tui.press(ENTER)
    tui.expect(f"fi: {INITIAL_COST} -> {INITIAL_COST} (target {INITIAL_COST} reached)")


def test_an_invalid_target_runs_nothing(tui):
    tui.press("I")
    set_target(tui, "abc")
    tui.press("G")
    tui.expect("Parameters of fi")
    tui.press(ENTER)
    tui.expect("Target cost: expected a number, found 'abc'")
    tui.expect_absent("Runner executing")


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
