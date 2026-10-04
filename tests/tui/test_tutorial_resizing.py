"""The tutorial's tester in small terminals, and in terminals resized while it
runs: each page shows its content, or scrolls to it, the text windows wrap
their text again, a run goes on, and the original size gives back the original
screen."""

import os
import re

import pytest

from tui_driver import BACKSPACE, DOWN, ENTER, ESCAPE, F1, F2, F3, F4, F5, TAB, UP, Tui

INITIAL_COST = 29  # the initial tour 0-1-2-3-4 of five.tsp
FI_COST = 26  # where First Improvement stops from it

def small(columns: int, lines: int, *, cut: str | None = None):
    """A terminal size as a test parameter; `cut` marks a size known to hide
    part of the page, with the reason."""
    marks = [pytest.mark.xfail(strict=True, reason=cut)] if cut else []
    return pytest.param(columns, lines, id=f"{columns}x{lines}", marks=marks)


@pytest.fixture
def terminal(binary):
    """Start the tester in a terminal of the size a test gives; each one must
    quit with q at the end, as in conftest.py."""
    drivers: list[Tui] = []

    def start(columns: int, lines: int) -> Tui:
        driver = Tui(binary, columns=columns, lines=lines).start()
        drivers.append(driver)
        return driver

    yield start
    for driver in drivers:
        driver.close()
    for driver in drivers:
        assert driver.exit_status == 0, "the tester did not quit with q"


def assert_framed(tui: Tui) -> None:
    """The screen is a whole frame at the current size: the outer border is
    drawn on every line, with nothing past it or left from an older size."""
    rows = tui.text().split("\n")
    problems = []
    if len(rows) != tui.lines:
        problems.append(f"{len(rows)} lines instead of {tui.lines}")
    for number, row in enumerate(rows):
        first, last = ("╭", "╮") if number == 0 else ("╰", "╯") if number == len(rows) - 1 \
            else ("│├", "│┤")
        if len(row) != tui.columns or row[:1] not in first or row[-1:] not in last:
            problems.append(f"line {number}: {row!r}")
    assert not problems, "the frame is broken:\n" + "\n".join(problems) + "\n" + tui.text()


def words(screen: str) -> str:
    """The text of the screen without borders, list markers and line breaks:
    a wrapped sentence reads as one line again."""
    return " ".join(word for word in re.sub(r"[│├┤╭╮╰╯─]", " ", screen).split()
                    if word != ">")


def send(tui: Tui, key: str) -> None:
    """Press a key without waiting for the screen to settle, which it does not
    while a run redraws its progress."""
    os.write(tui.fd, key.encode())


def evaluations(tui: Tui) -> int:
    """The evaluations of the run in progress, as the progress window shows them."""
    match = re.search(r"Running sa \[eval=(\d+)", tui.text())
    if match is None:
        raise AssertionError(f"no progress of sa on screen\n{tui.text()}")
    return int(match.group(1))


def shown_while_going_down(tui: Tui, contents: tuple[str, ...], presses: int = 15) -> None:
    """Each content is on screen at some point while Down moves the focus
    through the page, which scrolls to the focused control."""
    missing = set(contents)
    for _ in range(presses):
        missing = {content for content in missing if content not in tui.text()}
        if not missing:
            return
        tui.press(DOWN)
    assert not missing, f"never shown: {sorted(missing)}\n{tui.text()}"


def run_selected(tui: Tui) -> None:
    """Run the selected runner with its parameters as they are."""
    tui.press("G")
    tui.expect("Parameters of")
    tui.press(ENTER)


# -- small terminals ----------------------------------------------------------


@pytest.mark.parametrize("columns, lines", [small(80, 24), small(60, 20)])
def test_the_input_output_page_shows_its_controls(terminal, columns, lines):
    tui = terminal(columns, lines)
    tui.press("I", F3)
    tui.expect("Setup complete")
    assert_framed(tui)
    shown_while_going_down(tui, ("> five.tsp", "L Load selected", "I Initial  R Random",
                                 "Shift-L Load  W Save", "C Check"))


@pytest.mark.parametrize("columns, lines", [
    small(80, 40),
    small(80, 24),
    small(60, 20, cut="the page gets 8 lines: the actions and the diagnostics "
                      "leave none to the move window"),
])
def test_the_move_page_fits(terminal, columns, lines):
    tui = terminal(columns, lines)
    tui.press("I", F4, "B")
    tui.expect("Selected best move")
    assert_framed(tui)
    for content in ("B Best", "A Apply", "P List", "Move - 2-opt", "Delta check: OK"):
        tui.expect(content, timeout=1)


@pytest.mark.parametrize("columns, lines", [small(80, 24), small(60, 20)])
def test_the_run_page_shows_its_controls(terminal, columns, lines):
    tui = terminal(columns, lines)
    tui.press("I", F5)
    tui.expect("G Run selected")
    assert_framed(tui)
    tui.expect("> fi", timeout=1)
    tui.expect("  sa", timeout=1)  # the runner list keeps its lines
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
    tui.press(TAB, TAB, *[BACKSPACE] * 4)  # temperature.cooling_rate, 0.95
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
    tui.expect(f"current cost: {INITIAL_COST}", timeout=1)
    tui.type(str(INITIAL_COST))
    tui.select("fi", key=UP)
    run_selected(tui)
    assert_framed(tui)
    tui.expect(f"fi: {INITIAL_COST} -> {INITIAL_COST} (target {INITIAL_COST} reached)")


# -- resizing the text windows ------------------------------------------------

# The tutorial's input has no operator<<: the Input window shows this line.
NOT_PRINTABLE = "<not printable; add describe() or operator<<>"


def row_with(tui: Tui, text: str) -> str:
    rows = [row for row in tui.text().split("\n") if text in row]
    assert rows, f"{text!r} is not on screen\n{tui.text()}"
    return rows[0]


def test_the_input_window_wraps_again_when_resized(tui):
    tui.press(F1)
    tui.expect("Input  [F1]")
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
    tui.expect("Solution  [F2/S]")
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
    # A slower cooling makes the run last a few seconds (about 9 on a laptop).
    tui.press(TAB, TAB, *[BACKSPACE] * 4)
    tui.type("0.9999")
    send(tui, ENTER)
    tui.expect("Running sa [eval=")

    seen = evaluations(tui)
    for columns, lines in ((80, 24), (60, 20), (120, 40), (100, 30)):
        tui.resize(columns, lines, limit=0.5)
        if "Runner completed: sa" in tui.text():
            pytest.fail(f"the run ended before {columns}x{lines}: make it longer")
        assert_framed(tui)
        tui.expect(" Progress ", timeout=1)
        tui.wait_until(lambda _: evaluations(tui) > seen, timeout=5,
                       what=f"the progress to move on in {columns}x{lines}")
        seen = evaluations(tui)

    tui.expect("Runner completed: sa", timeout=120)
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
