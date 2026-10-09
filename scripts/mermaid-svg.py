#!/usr/bin/env python3
"""Draw the Mermaid diagrams of docs/ as SVG files, into docs/assets/diagrams.

The site draws its ```mermaid blocks in the browser; the PDFs of the sections
(scripts/docs-pdf.py) are rendered from the Markdown, where a diagram has to be
a picture. This script draws each block once, with the same library the theme
uses, and writes it next to the others:

    uv run scripts/mermaid-svg.py            # draw them all, report what changed
    uv run scripts/mermaid-svg.py --check    # only report, exit 1 when one is stale

Run it when a diagram is added or changed, and commit the SVG with the page, as
the TextUI screenshots are committed with the TextUI. It needs an installed
Chrome or Chromium ($CHROME, the PATH, or the usual places) and the network:
Mermaid is a script, loaded from the CDN the theme loads it from.
"""

import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DOCS = ROOT / "docs"
DIAGRAMS = DOCS / "assets" / "diagrams"

# The version Material for MkDocs loads, so that the site and the PDFs draw the
# same diagrams.
MERMAID = "https://unpkg.com/mermaid@11/dist/mermaid.min.js"

BLOCK = re.compile(r"^```mermaid\n(.*?)^```", re.MULTILINE | re.DOTALL)

PAGE_TEMPLATE = """<!doctype html>
<html>
  <head>
    <meta charset="utf-8">
    <style>body {{ margin: 0; background: #fff; font-family: sans-serif; }}</style>
    <script src="{library}"></script>
  </head>
  <body>
{containers}
    <script>
      mermaid.initialize({{
        startOnLoad: true,
        theme: "neutral",
        deterministicIds: true,
        // Labels as SVG text, not as HTML in a foreignObject, which the
        // renderers that read the file alone do not support.
        htmlLabels: false,
        flowchart: {{ htmlLabels: false }},
      }});
    </script>
  </body>
</html>
"""


def diagrams_of(page):
    """The Mermaid sources of a page, with the file name of each."""
    text = page.read_text(encoding="utf-8")
    stem = page.relative_to(DOCS).with_suffix("").as_posix().replace("/", "-")
    return [
        (f"{stem}-{number}.svg", block.group(1))
        for number, block in enumerate(BLOCK.finditer(text), start=1)
    ]


def every_diagram():
    """Every Mermaid diagram of the documentation, in the order of the pages."""
    found = []
    for page in sorted(DOCS.rglob("*.md")):
        found += diagrams_of(page)
    return found


def find_browser():
    """An installed Chrome or Chromium: $CHROME, the PATH, or the usual places."""
    named = os.environ.get("CHROME")
    if named:
        return named
    for name in ("chromium", "chromium-browser", "google-chrome", "google-chrome-stable"):
        found = shutil.which(name)
        if found:
            return found
    usual = [
        "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome",
        "/Applications/Chromium.app/Contents/MacOS/Chromium",
    ]
    return next((path for path in usual if Path(path).exists()), None)


def draw(diagrams, directory):
    """The SVG of each diagram, drawn by the browser in one page."""
    containers = "\n".join(
        f'    <div class="mermaid" id="d{number}">{source}</div>'
        for number, (_, source) in enumerate(diagrams)
    )
    page = directory / "diagrams.html"
    page.write_text(
        PAGE_TEMPLATE.format(library=MERMAID, containers=containers), encoding="utf-8"
    )
    browser = find_browser()
    if browser is None:
        raise SystemExit(
            "error: no browser to draw with: install Chrome or Chromium, or\n"
            "       set $CHROME to one."
        )
    finished = subprocess.run(
        [
            browser,
            "--headless=new",
            "--disable-gpu",
            "--virtual-time-budget=30000",
            "--dump-dom",
            page.as_uri(),
        ],
        capture_output=True,
        text=True,
    )
    if finished.returncode != 0:
        raise SystemExit(f"error: the browser failed\n{finished.stderr.strip()}")
    dom = finished.stdout
    drawn = {}
    for number, (name, _) in enumerate(diagrams):
        # The drawing holds elements of its own, so it ends at its own </svg>,
        # not at the first closing tag after the container.
        container = dom.find(f'id="d{number}"')
        opening = dom.find("<svg", container) if container != -1 else -1
        closing = dom.find("</svg>", opening) if opening != -1 else -1
        if closing == -1:
            raise SystemExit(f"error: {name} was not drawn; is the network reachable?")
        drawn[name] = standalone(dom[opening : closing + len("</svg>")]) + "\n"
    return drawn


def standalone(svg):
    """The drawing as a file of its own: a size in place of the page's width.

    In the browser a diagram is as wide as its column, which a renderer that
    reads the file alone cannot know; the view box gives it its own size.
    """
    box = re.search(r'viewBox="0 0 ([\d.]+) ([\d.]+)"', svg)
    if box is None:
        return svg
    width, height = (round(float(value)) for value in box.groups())
    svg = re.sub(r'\swidth="100%"', f' width="{width}" height="{height}"', svg, count=1)
    return re.sub(r'\sstyle="max-width:[^"]*"', "", svg, count=1)


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    parser.add_argument(
        "--check",
        action="store_true",
        help="report the diagrams that are missing or out of date, write nothing",
    )
    arguments = parser.parse_args()

    diagrams = every_diagram()
    if not diagrams:
        print("no Mermaid diagram in docs/")
        return 0

    DIAGRAMS.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="easylocal-mermaid-") as temporary:
        drawn = draw(diagrams, Path(temporary))

    stale = []
    for name, svg in drawn.items():
        path = DIAGRAMS / name
        current = path.read_text(encoding="utf-8") if path.exists() else None
        if current == svg:
            continue
        stale.append(name)
        if not arguments.check:
            path.write_text(svg, encoding="utf-8")

    unused = [
        path.name for path in sorted(DIAGRAMS.glob("*.svg")) if path.name not in drawn
    ]
    for name in unused:
        print(f"unused: {name}")
        if not arguments.check:
            (DIAGRAMS / name).unlink()

    if arguments.check:
        for name in stale:
            print(f"out of date: {name}")
        if stale or unused:
            print("run: uv run scripts/mermaid-svg.py")
            return 1
        print(f"{len(drawn)} diagrams are up to date")
        return 0

    for name in stale:
        print(f"drawn: {name}")
    print(f"{len(drawn)} diagrams, {len(stale)} written")
    return 0


if __name__ == "__main__":
    sys.exit(main())
