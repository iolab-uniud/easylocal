#!/usr/bin/env python3
"""Keep the code snippets of the documentation in sync with the examples.

A fenced block preceded by

    <!-- snippet: tutorial/tsp.hpp:model -->

is the code between the two "// [model]" marker lines of
examples/tutorial/tsp.hpp (dedented, nested marker lines removed); without a
":section" suffix it is the whole file. Running the script rewrites those blocks
from the sources; with --check it only reports blocks that differ and exits
with status 1, which is how the test suite uses it.
"""

import pathlib
import re
import sys
import textwrap

ROOT = pathlib.Path(__file__).resolve().parent.parent
DOCS = ROOT / "docs"
EXAMPLES = ROOT / "examples"
MARKER = re.compile(r"\s*//\s*\[[\w-]+\]")
SNIPPET = re.compile(
    r"(<!-- snippet: (?P<ref>[\w/.-]+(?::[\w-]+)?) -->\n```(?P<lang>\w*)\n)"
    r"(?P<body>.*?)(\n```)",
    re.S,
)


def extract(ref: str) -> str:
    path, _, section = ref.partition(":")
    lines = (EXAMPLES / path).read_text().split("\n")
    if not section:
        return "\n".join(lines).rstrip("\n")
    marks = [i for i, line in enumerate(lines)
             if re.search(r"//\s*\[" + re.escape(section) + r"\]", line)]
    if len(marks) != 2:
        raise SystemExit(f"{ref}: expected two '// [{section}]' markers, found {len(marks)}")
    body = [line for line in lines[marks[0] + 1:marks[1]] if not MARKER.match(line)]
    return textwrap.dedent("\n".join(body)).strip("\n")


def main() -> int:
    check = "--check" in sys.argv[1:]
    stale = []
    for page in sorted(DOCS.rglob("*.md")):
        text = page.read_text()
        synced = SNIPPET.sub(
            lambda m: m.group(1) + extract(m.group("ref")) + m.group(5), text)
        if synced != text:
            stale.append(page.relative_to(ROOT))
            if not check:
                page.write_text(synced)
    if check and stale:
        for page in stale:
            print(f"{page}: snippets differ from examples/; run scripts/sync-doc-snippets.py")
        return 1
    if not check:
        for page in stale:
            print(f"updated {page}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
