"""The tutorial's tester in small terminals: each page shows its content, or
scrolls to it, and the result of a run stays in view."""

import re

import pytest

from screens import FI_COST, INITIAL_COST, assert_framed, run_selected, \
    shown_while_going_down, small
from tui_driver import BACKSPACE, DOWN, ENTER, F3, F4, F5, TAB, UP


@pytest.mark.parametrize("columns, lines", [small(80, 24), small(60, 20)])
def test_the_input_output_page_shows_its_controls(terminal, columns, lines):
    tui = terminal(columns, lines)
    tui.press("I", F3)
    tui.expect("Setup complete")
    assert_framed(tui)
    shown_while_going_down(tui, ("> five.tsp", "L Load selected", "I Initial  R Random",
                                 "Shift-L Load  W Save", "C Check"))


@pytest.mark.parametrize("columns, lines", [small(80, 40), small(80, 24)])
def test_the_move_page_fits(terminal, columns, lines):
    tui = terminal(columns, lines)
    tui.press("I", F4, "B")
    tui.expect("Selected best move")
    assert_framed(tui)
    for content in ("B Best", "A Apply", "P List", "Move - 2-opt", "Delta check: OK"):
        tui.expect(content)


@pytest.mark.parametrize("columns, lines", [small(80, 24), small(60, 20)])
def test_the_run_page_shows_its_controls(terminal, columns, lines):
    tui = terminal(columns, lines)
    tui.press("I", F5)
    tui.expect("G Run selected")
    assert_framed(tui)
    tui.expect("> fi")
    tui.expect("  sa")  # the runner list keeps its lines
    shown_while_going_down(tui, ("P Problem parameters", "Apply seed", "Target cost",
                                 f"current cost: {INITIAL_COST}", "Stop after",
                                 "evaluations"))
    tui.select("fi", key=UP)  # out of the fields, where q is typed, not quit


@pytest.mark.parametrize("columns, lines", [small(100, 30), small(80, 24), small(60, 20)])
def test_the_result_of_a_run_is_shown(terminal, columns, lines):
    tui = terminal(columns, lines)
    tui.press("I", F5)
    run_selected(tui)
    # In the status line, always visible, and below the controls.
    tui.expect(f"Runner completed: fi: {INITIAL_COST} -> {FI_COST}")
    assert_framed(tui)
    rows = [row for row in tui.text().split("\n") if f"fi: {INITIAL_COST} -> {FI_COST}" in row]
    assert len(rows) == 2, tui.text()


def test_the_result_of_a_run_is_shown_after_changed_parameters(tui):
    tui.press("I", F5)
    tui.select("sa")
    tui.press("G")
    tui.expect("Parameters of sa")
    tui.press(TAB, TAB).keys(*[BACKSPACE] * 4)  # temperature.cooling_rate, 0.95
    tui.type("0.9")
    tui.press(ENTER)
    tui.expect("Runner completed: sa", timeout=60)
    tui.expect("runners.sa.temperature.cooling_rate = 0.9 (was 0.95)")
    rows = [row for row in tui.text().split("\n")
            if re.search(rf"sa: {INITIAL_COST} -> \d+", row)]
    assert len(rows) == 2, tui.text()  # the Last run box and the status line


def test_the_focus_scrolls_to_the_target_field(terminal):
    # In 80x24 the target field is below the last line until the focus
    # reaches it.
    tui = terminal(80, 24)
    tui.press("I", F5)
    tui.expect_absent("current cost")
    tui.focus("P Problem parameters")  # through the runner list
    tui.press(*[DOWN] * 8, UP)  # to the limits, then the target above them
    tui.expect(f"current cost: {INITIAL_COST}")
    tui.type(str(INITIAL_COST))
    tui.select("fi", key=UP)
    run_selected(tui)
    assert_framed(tui)
    tui.expect(f"fi: {INITIAL_COST} -> {INITIAL_COST} (target reached, 1 evaluation)")
