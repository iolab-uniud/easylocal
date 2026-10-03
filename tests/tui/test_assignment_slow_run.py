"""A run of the assignment example's slow-fi (2000 evaluations, 5 ms each)
watched while it runs and stopped from the progress window."""

import re

from tui_driver import F5, Tui

BUDGET = 2000  # slow-fi's max_evaluations (examples/assignment/application.hpp)


def evaluations(tui: Tui) -> int:
    return int(tui.expect(re.compile(r"Running slow-fi \[eval=(\d+)")).group(1))


def test_a_slow_run_shows_its_progress_and_stops(slow_runner):
    with Tui(slow_runner) as tui:
        tui.press("I", F5)
        tui.select("slow-fi")
        tui.press("G")
        tui.expect("Runner executing: slow-fi")

        first = evaluations(tui)
        tui.wait_until(lambda _: evaluations(tui) > first, what="the progress advancing")
        assert evaluations(tui) < BUDGET

        tui.press("X")
        tui.expect("Runner stopped: slow-fi", timeout=20)
        final = tui.expect(re.compile(r"slow-fi: .* -> (.*) \(stopped\)")).group(1)
        assert f"COST {final}" in tui.text()  # the current solution is where it stopped

        tui.press("q")
        assert tui.wait_exit() == 0
