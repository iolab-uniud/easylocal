#!/usr/bin/env python3
"""Render each section of the documentation as a PDF, into build/pdf.

A section is a top-level entry of the nav of mkdocs.yml — Quick start,
Tutorial, Coming from EasyLocal 3, Reference, Guides, Benchmarks — and becomes
one PDF, its pages in the order of the nav, with a title page and a table of
contents:

    uv run scripts/docs-pdf.py                 # every section
    uv run scripts/docs-pdf.py --only tutorial # one of them, by its slug
    uv run scripts/docs-pdf.py --list          # the slugs and their pages

The PDFs are rendered from the Markdown, not from the site: pandoc reads the
pages and Typst sets them. Neither reads the extensions of the site's Markdown,
so the pages are prepared first (see `prepare`): the Mermaid blocks become the
SVG files `scripts/mermaid-svg.py` draws, the captions of the code blocks
become lines of their own, and the links between pages become links to the
published site, which a reader of a PDF can follow.

It needs pandoc 3.x and the Typst compiler, either installed:

    brew install pandoc typst          # or the packages of your distribution

or as the optional `pdf` dependency group, which carries both:

    uv run --group pdf scripts/docs-pdf.py

The PDFs are not part of `mkdocs build`: the documentation workflow runs this
script and publishes them under pdf/ next to the site.
"""

import argparse
import importlib.util
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CONFIG = ROOT / "mkdocs.yml"
DOCS = ROOT / "docs"
DIAGRAMS = DOCS / "assets" / "diagrams"
STYLE = Path(__file__).resolve().parent / "docs-pdf.typ"
OUTPUT = ROOT / "build" / "pdf"

# The page of the site that is a landing page, not a section to print.
HOME = "index.md"

# Where a link between pages of the site points, for a reader of a PDF.
SITE = "https://iolab-uniud.github.io/easylocal"


class Section:
    """A top-level entry of the nav: its title, its file name and its pages."""

    def __init__(self, title, pages):
        self.title = title
        self.pages = pages
        self.slug = slugify(title)

    def __repr__(self):
        return f"{self.slug} ({len(self.pages)} pages)"


def slugify(title):
    slug = "".join(c.lower() if c.isalnum() else "-" for c in title)
    while "--" in slug:
        slug = slug.replace("--", "-")
    return slug.strip("-")


def pages_of(entry):
    """The Markdown pages of a nav entry, in order, external links left out."""
    if isinstance(entry, str):
        return [entry] if entry.endswith(".md") else []
    if isinstance(entry, dict):
        return [page for value in entry.values() for page in pages_of(value)]
    if isinstance(entry, list):
        return [page for item in entry for page in pages_of(item)]
    return []


def sections_of(nav):
    """The sections of the nav, the home page and link-only entries left out."""
    sections = []
    for entry in nav:
        if not isinstance(entry, dict):
            continue
        for title, value in entry.items():
            pages = [page for page in pages_of(value) if page != HOME]
            if pages:
                sections.append(Section(title, pages))
    return sections


def read_config():
    """mkdocs.yml, read as MkDocs itself reads it."""
    from mkdocs.config.base import load_config

    return load_config(str(CONFIG))


def site_link(page, target):
    """Where a relative link of a page points on the published site."""
    path, _, anchor = target.partition("#")
    resolved = (Path(page).parent / path).as_posix()
    while resolved.startswith("../"):
        resolved = resolved[3:]
    resolved = re.sub(r"(^|/)README\.md$", r"\1", resolved)
    resolved = re.sub(r"\.md$", "/", resolved)
    link = f"{SITE}/{resolved}"
    return f"{link}#{anchor}" if anchor else link


def prepare(page, text, figures):
    """A page of the site as pandoc reads it.

    The Markdown of the site uses extensions of its own, and refers to its
    pages by their files; a PDF has neither, so the blocks that only a browser
    can draw become pictures and the links become links to the site.
    """
    # The line that offers the PDF of the section, which is the PDF itself: it
    # carries the class the site hides it with when a page is printed.
    text = re.sub(r"^[^\n]*\{ \.pdf-of-this-section \}\n\n", "", text, flags=re.M)
    # The markers the snippets of the examples are checked against.
    text = re.sub(r"^<!-- snippet:[^\n]*-->\n", "", text, flags=re.M)
    # A diagram: the SVG that scripts/mermaid-svg.py drew from this block.
    stem = Path(page).with_suffix("").as_posix().replace("/", "-")
    counter = iter(range(1, 100))
    text = BLOCK_MERMAID.sub(
        lambda _: f"![]({DIAGRAMS / f'{stem}-{next(counter)}.svg'})\n", text
    )
    # A figure drawn by hand, as an SVG in the page: a picture of its own.
    text = FIGURE.sub(lambda match: inline_figure(match, stem, figures), text)
    # The caption of a code block, which the site shows above it.
    text = re.sub(
        r"^```(\w+) title=\"([^\"]+)\"\n", r"*\2*\n\n```\1\n", text, flags=re.M
    )
    # An admonition, which has no counterpart: its title and its text, quoted.
    text = ADMONITION.sub(quoted_admonition, text)
    # The attributes of a link, which say how the site serves a download.
    text = re.sub(r"\)\{:[^}]*\}", ")", text)
    # A link to another page, which the reader of a PDF follows to the site;
    # a link inside the page stays where it is.
    text = re.sub(
        r"\]\((?!https?:|#)([^)\s]+\.md(?:#[^)\s]*)?)\)",
        lambda match: f"]({site_link(page, match.group(1))})",
        text,
    )
    # A picture of the page, by its path in docs/.
    text = re.sub(
        r"\]\((?!https?:|/)([^)\s]+\.(?:svg|png|jpg))\)",
        lambda match: f"]({(DOCS / page).parent / match.group(1)})",
        text,
    )
    return text


BLOCK_MERMAID = re.compile(r"^```mermaid\n.*?^```\n", re.MULTILINE | re.DOTALL)
FIGURE = re.compile(
    r"^<figure>\n(?P<svg><svg.*?</svg>)\n*(?:<figcaption>(?P<caption>.*?)</figcaption>)?"
    r"\n*</figure>\n",
    re.MULTILINE | re.DOTALL,
)
ADMONITION = re.compile(
    r'^(?:!!!|\?\?\?)\+? +(\w+)(?: +"([^"]*)")?\n((?: {4}[^\n]*\n|\n(?= {4}))+)', re.M
)


def quoted_admonition(match):
    """An admonition of the site as a quotation: its title, then its text."""
    kind, title, body = match.groups()
    heading = title if title else kind.capitalize()
    quoted = re.sub(r"^ {4}", "", body, flags=re.M).strip("\n")
    quoted = "\n".join(f"> {line}" if line else ">" for line in quoted.split("\n"))
    return f"> **{heading}**\n>\n{quoted}\n"


def inline_figure(match, stem, figures):
    """An SVG written in the page, saved next to the others as a picture."""
    number = len(figures) + 1
    path = figures[0] / f"{stem}-figure-{number}.svg"
    svg = match.group("svg")
    if "xmlns=" not in svg:
        svg = svg.replace("<svg", '<svg xmlns="http://www.w3.org/2000/svg"', 1)
    path.write_text(svg, encoding="utf-8")
    figures.append(path)
    caption = re.sub(r"<[^>]+>", "", match.group("caption") or "").strip()
    return f"![{caption}]({path})\n"


def document(section, directory):
    """The Markdown of a whole section, prepared, as one file."""
    figures = [directory]
    parts = []
    for page in section.pages:
        text = (DOCS / page).read_text(encoding="utf-8")
        parts.append(prepare(page, text, figures))
    path = directory / f"{section.slug}.md"
    path.write_text("\n\n".join(parts), encoding="utf-8")
    return path


def render(section, directory, found):
    """The PDF of a section: pandoc writes the Typst source, Typst sets it."""
    source = document(section, directory)
    typst_source = directory / f"{section.slug}.typ"
    finished = subprocess.run(
        [
            found["pandoc"],
            str(source),
            "--from=markdown-raw_html",
            "--to=typst",
            "--standalone",
            "--toc",
            "--toc-depth=2",
            f"--include-in-header={STYLE}",
            "--metadata",
            f"title=EasyLocal — {section.title}",
            "--metadata",
            "subtitle=A C++23 framework for local search and metaheuristics",
            "--variable=papersize:a4",
            "--variable=margin-x:22mm",
            "--variable=margin-y:24mm",
            "--variable=fontsize:10pt",
            f"--output={typst_source}",
        ],
        capture_output=True,
        text=True,
        cwd=ROOT,
    )
    if finished.returncode != 0:
        raise SystemExit(f"error: {section.slug}: pandoc\n{finished.stderr.strip()}")

    OUTPUT.mkdir(parents=True, exist_ok=True)
    pdf = OUTPUT / f"easylocal-{section.slug}.pdf"
    compile_typst(typst_source, pdf, found)
    return pdf


def compile_typst(source, pdf, found):
    """Sets the Typst document, with the compiler or with its Python package.

    The pictures are named by their path on disk, so the root the compiler
    resolves them from is the root of the filesystem.
    """
    if found["typst"]:
        finished = subprocess.run(
            [found["typst"], "compile", "--root", "/", str(source), str(pdf)],
            capture_output=True,
            text=True,
        )
        if finished.returncode != 0:
            raise SystemExit(f"error: typst\n{finished.stderr.strip()}")
        return
    import typst

    typst.compile(input=str(source), output=str(pdf), root="/")


def tools():
    """Where pandoc and the Typst compiler are, or what is missing.

    Either an installation on the PATH or, when the optional `pdf` dependency
    group is there, the one inside its packages.
    """
    found = {tool: shutil.which(tool) for tool in ("pandoc", "typst")}
    if found["pandoc"] is None:
        try:
            import pypandoc

            found["pandoc"] = pypandoc.get_pandoc_path()
        except (ImportError, OSError):
            pass
    if found["typst"] is None:
        # The package has no command of its own: its Python API sets the
        # document instead, which compile_typst falls back to.
        found["typst"] = "" if importlib.util.find_spec("typst") else None
    return found


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    parser.add_argument("--only", metavar="SLUG", help="render this section alone")
    parser.add_argument(
        "--list",
        action="store_true",
        dest="list_sections",
        help="list the sections and their pages, render nothing",
    )
    arguments = parser.parse_args()

    sections = sections_of(read_config()["nav"])
    if arguments.only:
        sections = [section for section in sections if section.slug == arguments.only]
        if not sections:
            raise SystemExit(f"error: no section is named {arguments.only}")

    if arguments.list_sections:
        for section in sections:
            print(f"{section.slug}: {', '.join(section.pages)}")
        return 0

    found = tools()
    missing = [tool for tool, path in found.items() if path is None]
    if missing:
        raise SystemExit(
            f"error: {' and '.join(missing)} not found: the PDFs are rendered with\n"
            "       pandoc and typst. Install them (brew install pandoc typst, or\n"
            "       the packages of your distribution), or run this script as\n"
            "       uv run --group pdf scripts/docs-pdf.py."
        )

    with tempfile.TemporaryDirectory(prefix="easylocal-pdf-") as temporary:
        for section in sections:
            pdf = render(section, Path(temporary), found)
            size = pdf.stat().st_size / 1024
            print(f"{pdf.relative_to(ROOT)} ({size:.0f} KiB)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
