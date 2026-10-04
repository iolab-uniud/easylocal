"""The tutorial's tester in terminals resized while it runs: the text windows
wrap their text again, a run goes on, and the original size gives back the
original screen."""

import re

import pytest

from screens import FI_COST, INITIAL_COST, assert_framed, evaluations, run_selected, send, words
from tui_driver import BACKSPACE, ENTER, ESCAPE, F1, F2, F3, F4, F5, TAB, Tui


# -- resizing the text windows ------------------------------------------------

# The tutorial's input has no operator<<: the Input window shows this line.
NOT_PRINTABLE = "<not printable; add describe() or operator<<>"


def row_with(tui: Tui, text: str) -> str:
    rows = [row for row in tui.text().split("\n") if text in row]
    assert rows, f"{text!r} is not on screen\n{tui.text()}"
    return rows[0]


def test_the_input_window_wraps_again_when_resized(tui):
    tui.press(F1)
    tui.expect(NOT_PRINTABLE)  # the whole window, not only its title
    assert NOT_PRINTABLE in row_with(tui, "describe()")

    tui.resize(50, 20)  # narrower than the line
    assert_framed(tui)
    tui.expect("Input  [F1]")
    assert "operator<<>" not in row_with(tui, "describe()")
    assert NOT_PRINTABLE in words(tui.text())  # wrapped, not cut

    tui.resize(120, 35)
    assert_framed(tui)
    assert NOT_PRINTABLE in row_with(tui, "describe()")  # one line again

    tui.press(ESCAPE)
    tui.wait_until(lambda screen: "Input  [F1]" not in screen, what="the window closed")
    tui.expect("Input ready")
    assert_framed(tui)


def test_the_solution_window_wraps_again_when_resized(tui):
    hint = "Up/Down/PgUp/PgDn scroll | Esc/F2/S close"
    tui.press("I", F2)
    tui.expect(f"TourLength: {INITIAL_COST}")  # the whole window, not only its title
    assert "Esc/F2/S close" in row_with(tui, "PgDn scroll")

    tui.resize(40, 16)
    assert_framed(tui)
    assert "Esc/F2/S close" not in row_with(tui, "PgDn scroll")
    screen = words(tui.text())
    for content in (hint, f"TourLength: {INITIAL_COST}", "0 1 2 3 4", "Close"):
        assert content in screen, f"{content!r} is cut\n{tui.text()}"

    tui.resize(100, 30)
    assert_framed(tui)
    assert "Esc/F2/S close" in row_with(tui, "PgDn scroll")

    tui.press(ESCAPE)
    tui.wait_until(lambda screen: "Solution  [F2/S]" not in screen, what="the window closed")
    tui.expect("Select move")
    assert_framed(tui)


# -- resizing during a run ----------------------------------------------------


def test_a_run_goes_on_while_the_terminal_is_resized(tui):
    tui.press("I", F5)
    tui.select("sa")
    tui.press("G")
    tui.expect("Parameters of sa")
    # So slow a cooling that the run lasts until X stops it.
    tui.press(TAB, TAB).keys(*[BACKSPACE] * 4)
    tui.type("0.99999999")
    send(tui, ENTER)
    tui.expect("Running sa [eval=")

    seen = evaluations(tui)
    for columns, lines in ((80, 24), (60, 20), (120, 40), (100, 30)):
        tui.resize(columns, lines, limit=0.5)
        assert_framed(tui)
        tui.expect(" Progress ")
        tui.wait_until(lambda _: evaluations(tui) > seen, timeout=5,
                       what=f"the progress to move on in {columns}x{lines}")
        seen = evaluations(tui)

    send(tui, "X")
    tui.expect("Runner stopped: sa", timeout=10)
    tui.expect_absent(" Progress ")
    assert_framed(tui)
    final = int(tui.expect(re.compile(rf"sa: {INITIAL_COST} -> (\d+)")).group(1))
    assert final <= INITIAL_COST
    assert tui.cost() == final


# -- back to the original size ------------------------------------------------


@pytest.mark.parametrize("page, content", [
    (F3, "Setup complete"),
    (F4, "Delta check: OK"),
    (F5, f"fi: {INITIAL_COST} -> {FI_COST}"),
])
def test_the_original_size_gives_back_the_original_screen(tui, page, content):
    tui.press("I", F5)
    run_selected(tui)
    tui.expect("Runner completed: fi")
    tui.press(F4, "B", page)
    tui.expect(content)
    before = tui.text()

    for columns, lines in ((60, 20), (40, 12), (140, 45), (100, 30)):
        tui.resize(columns, lines)
        assert_framed(tui)

    assert tui.text() == before
