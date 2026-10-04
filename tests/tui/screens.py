"""Helpers of the tests that check the tutorial's tester on its screen."""

import os
import re

import pytest

from tui_driver import DOWN, ENTER, Tui

INITIAL_COST = 29  # the initial tour 0-1-2-3-4 of five.tsp
FI_COST = 26  # where First Improvement stops from it


def small(columns: int, lines: int):
    """A terminal size as a test parameter."""
    return pytest.param(columns, lines, id=f"{columns}x{lines}")


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
