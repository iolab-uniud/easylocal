#!/usr/bin/env python3
"""Regenerate the TextUI screenshots of docs/tutorial/13-tester.md.

Drives the tutorial's interactive tester (examples/tutorial/tui_main.cpp) in a
pseudo-terminal with a fixed size and a fixed key sequence, reconstructs each
screen with a VT100 emulator and writes it as an SVG image.

Requirements: a build with the TUI component, and the `pyte` package, provided
by the uv environment of the repository (pyproject.toml):

    uv run scripts/tui-snapshots.py build/<preset>/examples/tutorial/easylocal_tutorial_tui
    uv run scripts/tui-snapshots.py <binary> --text     # print the screens instead
"""

import html
import pathlib
import sys

import pyte

from tui_driver import DOWN, ENTER, F3, F4, F5, Tui

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUTPUT = ROOT / "docs" / "tutorial" / "images"
COLUMNS, LINES = 100, 30

# (image name, actions before the snapshot); actions accumulate. An action is a
# key sequence, or ("until", key, text): press key until the screen shows text.
SCENARIO = [
    ("tui-check", ["I", F3, "C"]),                      # initial solution, app check
    ("tui-moves", [F4, "B"]),                           # the best move from it
    ("tui-parameters", [F5, ("until", DOWN, "> sa"), "G"]),  # its parameters
    ("tui-run", [ENTER]),                               # Simulated Annealing
]

PALETTE = {
    "black": "#1e1e1e", "red": "#cd3131", "green": "#0dbc79", "brown": "#e5e510",
    "yellow": "#e5e510", "blue": "#2472c8", "magenta": "#bc3fbc", "cyan": "#11a8cd",
    "white": "#e5e5e5", "brightblack": "#666666", "brightred": "#f14c4c",
    "brightgreen": "#23d18b", "brightyellow": "#f5f543", "brightblue": "#3b8eea",
    "brightmagenta": "#d670d6", "brightcyan": "#29b8db", "brightwhite": "#ffffff",
}
FOREGROUND, BACKGROUND = "#d4d4d4", "#1e1e1e"


def color(value: str, default: str) -> str:
    if value == "default":
        return default
    if value in PALETTE:
        return PALETTE[value]
    if len(value) == 6 and all(c in "0123456789abcdefABCDEF" for c in value):
        return "#" + value
    return default


def to_svg(screen: pyte.Screen) -> str:
    cell_w, cell_h, font = 8.4, 17, 14
    width, height = COLUMNS * cell_w + 16, LINES * cell_h + 16
    parts = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width:.0f}" height="{height:.0f}" '
        f'viewBox="0 0 {width:.0f} {height:.0f}">',
        f'<rect width="100%" height="100%" rx="6" fill="{BACKGROUND}"/>',
        f'<g font-family="Menlo, Consolas, \'DejaVu Sans Mono\', monospace" font-size="{font}">',
    ]
    for y in range(LINES):
        row = screen.buffer[y]
        x = 0
        while x < COLUMNS:
            char = row[x]
            fg = color(char.fg, FOREGROUND)
            bg = color(char.bg, BACKGROUND)
            if char.reverse:
                fg, bg = bg, fg
            style = (fg, bg, char.bold)
            run = x
            text = ""
            while run < COLUMNS:
                c = row[run]
                cfg, cbg = color(c.fg, FOREGROUND), color(c.bg, BACKGROUND)
                if c.reverse:
                    cfg, cbg = cbg, cfg
                if (cfg, cbg, c.bold) != style:
                    break
                text += c.data or " "
                run += 1
            px, py = 8 + x * cell_w, 8 + y * cell_h
            if bg != BACKGROUND:
                parts.append(
                    f'<rect x="{px:.1f}" y="{py}" width="{(run - x) * cell_w:.1f}" '
                    f'height="{cell_h}" fill="{bg}"/>')
            if text.strip():
                # One x per character: every glyph sits in its cell whatever
                # the font's advance, so boxes line up in any renderer.
                weight = ' font-weight="bold"' if style[2] else ""
                xs = " ".join(f"{px + i * cell_w:.1f}" for i in range(len(text)))
                parts.append(
                    f'<text x="{xs}" y="{py + 13}" fill="{fg}"{weight} '
                    f'xml:space="preserve">{html.escape(text)}</text>')
            x = run
    parts.append("</g></svg>")
    return "\n".join(parts) + "\n"


def main() -> int:
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    binary = sys.argv[1]
    as_text = "--text" in sys.argv[2:]

    OUTPUT.mkdir(parents=True, exist_ok=True)
    with Tui(binary, COLUMNS, LINES) as tui:
        for name, actions in SCENARIO:
            for action in actions:
                if isinstance(action, tuple):
                    _, key, text = action
                    tui.select(text.removeprefix("> "), key)
                else:
                    tui.press(action)
                    tui.settle(quiet=0.6)
            if as_text:
                print(f"=== {name}\n{tui.text()}\n")
            else:
                (OUTPUT / f"{name}.svg").write_text(to_svg(tui.screen_buffer))
                print(f"wrote {(OUTPUT / name).relative_to(ROOT)}.svg")
        tui.press("q")
    return 0


if __name__ == "__main__":
    sys.exit(main())
