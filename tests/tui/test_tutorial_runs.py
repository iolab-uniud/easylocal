"""Runs of the tutorial's TSP tester: Simulated Annealing to completion, its
parameters, and the target cost, time limit and evaluation budget that stop a
run."""

import re

from tui_driver import BACKSPACE, DOWN, ENTER, ESCAPE, F5, RIGHT, TAB, UP, Tui

INITIAL_COST = 29  # the initial tour 0-1-2-3-4 of five.tsp


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
    tui.press(TAB, TAB).keys(*[BACKSPACE] * 4)
    tui.type("2")
    tui.press(ENTER)
    tui.expect("cooling_rate: expected a value in (0, 1), got 2")
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
    # To the last row, the limits, then up to the target.
    tui.press(*[DOWN] * 10, UP)
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
    tui.expect(f"fi: {INITIAL_COST} -> {INITIAL_COST} (target reached, 1 evaluation)")


def test_an_invalid_target_runs_nothing(tui):
    tui.press("I")
    set_target(tui, "abc")
    tui.press("G")
    tui.expect("Parameters of fi")
    tui.press(ENTER)
    tui.expect("Target cost: expected a number, found 'abc'")
    tui.expect_absent("Runner executing")


def test_a_time_limit_stops_the_run(tui):
    # No time at all: First Improvement stops at its first check, from the
    # initial tour.
    tui.press("I", F5)
    tui.focus("P Problem parameters")  # through the runner list
    tui.press(*[DOWN] * 10)  # to the last row, the limits: seconds first
    tui.type("0")
    tui.select("fi", key=UP)
    tui.press("G")
    tui.expect("Parameters of fi")
    tui.press(ENTER)
    tui.expect(f"fi: {INITIAL_COST} -> {INITIAL_COST} (time limit reached, ")


def test_an_invalid_time_limit_runs_nothing(tui):
    tui.press("I", F5)
    tui.focus("P Problem parameters")
    tui.press(*[DOWN] * 10)
    tui.type("soon")
    tui.select("fi", key=UP)
    tui.press("G")
    tui.expect("Parameters of fi")
    tui.press(ENTER)
    tui.expect("Time limit: give a non-negative number of seconds, or nothing")
    tui.expect_absent("Runner executing")


def test_an_invalid_evaluation_budget_runs_nothing(tui):
    tui.press("I", F5)
    tui.focus("P Problem parameters")
    tui.press(*[DOWN] * 10, RIGHT)  # the last row, then from seconds to evaluations
    tui.type("many")
    tui.select("fi", key=UP)
    tui.press("G")
    tui.expect("Parameters of fi")
    tui.press(ENTER)
    tui.expect("Evaluations: give a non-negative whole number, or nothing")
    tui.expect_absent("Runner executing")


def test_an_evaluation_budget_stops_the_run(tui):
    # One evaluation, the initial one: First Improvement stops on the initial
    # tour.
    tui.press("I", F5)
    tui.focus("P Problem parameters")
    tui.press(*[DOWN] * 10, RIGHT)  # the last row, then from seconds to evaluations
    tui.type("1")
    tui.select("fi", key=UP)
    tui.press("G")
    tui.expect("Parameters of fi")
    tui.press(ENTER)
    tui.expect(f"fi: {INITIAL_COST} -> {INITIAL_COST} (evaluation budget exhausted, 1 evaluation)")
