#!/usr/bin/env python3
"""Keep the code snippets of the documentation in sync with the examples.

A fenced block preceded by

    <!-- snippet: tutorial/tsp.hpp:model -->

is the code between the two "// [model]" marker lines of
examples/tutorial/tsp.hpp (dedented, nested marker lines removed); without a
":section" suffix it is the whole file, marker lines removed. A section may
leave out nested sections, "tutorial/tsp.hpp:solution-manager!random-solution",
so that a chapter can show a class without a member a later chapter adds.
Running the script rewrites those blocks from the sources; with --check it only
reports blocks that differ and exits with status 1, which is how the test suite
uses it.

Standard library only: `uv run scripts/sync-doc-snippets.py` or `python3`.
"""

import pathlib
import re
import sys
import textwrap

ROOT = pathlib.Path(__file__).resolve().parent.parent
DOCS = ROOT / "docs"
EXAMPLES = ROOT / "examples"
MARKER = re.compile(r"\s*//\s*\[[\w-]+\]")
# The block body runs up to the first line that starts with ``` (an empty block
# included).
SNIPPET = re.compile(
    r"(<!-- snippet: (?P<ref>[\w/.-]+(?::[\w-]+(?:![\w-]+)*)?) -->\n```(?P<lang>\w*)\n)"
    r"(?P<body>(?:(?!```).*\n)*?)(?P<close>```)",
    re.M,
)


def extract(ref: str) -> str:
    path, _, section = ref.partition(":")
    lines = (EXAMPLES / path).read_text(encoding="utf-8").split("\n")
    if not section:
        return "\n".join(line for line in lines if not MARKER.match(line)).rstrip("\n")
    section, *excluded = section.split("!")
    first, last = markers(lines, ref, section)
    body = lines[first + 1:last]
    for name in excluded:
        start, end = markers(body, ref, name)
        body = body[:start] + body[end + 1:]
    body = [line for line in body if not MARKER.match(line)]
    # Leaving out a section may leave two blank lines in a row.
    body = [line for i, line in enumerate(body)
            if line.strip() or i == 0 or body[i - 1].strip()]
    return textwrap.dedent("\n".join(body)).strip("\n")


def markers(lines: list[str], ref: str, section: str) -> tuple[int, int]:
    marks = [i for i, line in enumerate(lines)
             if re.search(r"//\s*\[" + re.escape(section) + r"\]", line)]
    if len(marks) != 2:
        raise SystemExit(f"{ref}: expected two '// [{section}]' markers, found {len(marks)}")
    return marks[0], marks[1]


def main() -> int:
    check = "--check" in sys.argv[1:]
    stale = []
    for page in sorted(DOCS.rglob("*.md")):
        text = page.read_text(encoding="utf-8")
        synced = SNIPPET.sub(
            lambda m: m.group(1) + extract(m.group("ref")) + "\n" + m.group("close"),
            text)
        if synced != text:
            stale.append(page.relative_to(ROOT))
            if not check:
                page.write_text(synced, encoding="utf-8")
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
