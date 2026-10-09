#!/usr/bin/env python3
"""Render each section of the documentation site as a PDF, into build/pdf.

A section is a top-level entry of the nav of mkdocs.yml — Quick start,
Tutorial, Coming from EasyLocal 3, Reference, Guides, Benchmarks — and becomes
one PDF with a cover page and a table of contents, the chapters of a section in
the order of the nav:

    uv run scripts/docs-pdf.py                 # every section
    uv run scripts/docs-pdf.py --only tutorial # one of them, by its slug
    uv run scripts/docs-pdf.py --list          # the slugs and their pages

For each section the script builds the site again, with the print-site plugin
on and every other page excluded, and prints the single page it generates with
a headless browser, so that the diagrams and the syntax highlighting are the
ones the site shows. The home page is not a section: it is a page of links.

It prints with an installed Chrome or Chromium — $CHROME, the PATH, or the
usual places — and needs nothing else: the page size and the margins are in
the print stylesheet of the site (docs/assets/css/extra.css), and the browser
is given the time to draw the Mermaid diagrams before it prints.

The PDFs are not part of `mkdocs build`: the documentation workflow runs this
script and publishes them under pdf/ next to the site.
"""

import argparse
import contextlib
import functools
import http.server
import os
import shutil
import subprocess
import sys
import tempfile
import threading
import urllib.parse
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CONFIG = ROOT / "mkdocs.yml"
OUTPUT = ROOT / "build" / "pdf"

# The page of the site that is a landing page, not a section to print.
HOME = "index.md"

# Milliseconds of page time the browser is given before it prints: the
# Mermaid diagrams are drawn in the browser, and a long section has a few.
DRAWING_TIME = 20_000


class Section:
    """A top-level entry of the nav: its title, its file name and its pages."""

    def __init__(self, title, pages):
        self.title = title
        self.pages = pages
        self.slug = slugify(title)

    def __repr__(self):
        return f"{self.slug} ({len(self.pages)} pages)"


def slugify(title):
    kept = [c.lower() if c.isalnum() else "-" for c in title]
    slug = "".join(kept)
    while "--" in slug:
        slug = slug.replace("--", "-")
    return slug.strip("-")


def yaml_scalar(text):
    """A YAML scalar for a title, quoted when it could be read as something else."""
    if any(character in text for character in ":#{}[],&*!|>%@`\"'"):
        return '"' + text.replace('"', '\\"') + '"'
    return text


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


def overlay_config(section, every_page, site_dir, directory):
    """A configuration that inherits mkdocs.yml and prints this section only.

    The paths are absolute, since a relative one would be read from the
    directory of this configuration file rather than from the repository.
    """
    excluded = [HOME] + [page for page in every_page if page not in section.pages]
    lines = [
        f"INHERIT: {CONFIG}",
        f"site_name: EasyLocal — {section.title}",
        # The nav of this section alone: the table of contents of the print
        # page follows it, and would otherwise name the empty sections too.
        "nav:",
        f"  - {yaml_scalar(section.title)}:",
    ]
    lines += [f"      - {page}" for page in section.pages]
    lines += [
        f"docs_dir: {ROOT / 'docs'}",
        f"site_dir: {site_dir}",
        "hooks:",
        f"  - {ROOT / 'scripts' / 'mkdocs_downloads.py'}",
        "plugins:",
        "  - search",
        "  - print-site:",
        "      enabled: true",
        "      add_to_navigation: false",
        "      add_cover_page: true",
        "      add_table_of_contents: true",
        "      toc_depth: 3",
        # The titles of the tutorial are numbered already.
        "      enumerate_headings: false",
        "      add_print_site_banner: false",
        "      exclude:",
    ]
    lines += [f"        - {page}" for page in excluded]
    path = directory / f"mkdocs-{section.slug}.yml"
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return path


def build(config_path):
    subprocess.run(
        [sys.executable, "-m", "mkdocs", "build", "--quiet", "-f", str(config_path)],
        check=True,
        cwd=ROOT,
    )


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
    for path in usual:
        if Path(path).exists():
            return path
    return None


def print_page(browser, url, pdf):
    """Prints the page at the URL, once its scripts have had their time."""
    finished = subprocess.run(
        [
            browser,
            "--headless=new",
            "--disable-gpu",
            "--no-pdf-header-footer",
            # The page draws its Mermaid diagrams when it is loaded: this gives
            # the scripts their time, and the browser prints as soon as they
            # are done, not after the whole budget.
            f"--virtual-time-budget={DRAWING_TIME}",
            "--run-all-compositor-stages-before-draw",
            f"--print-to-pdf={pdf}",
            url,
        ],
        capture_output=True,
        text=True,
    )
    if finished.returncode != 0 or not pdf.exists():
        raise SystemExit(
            f"error: the browser could not print {url}\n{finished.stderr.strip()}"
        )


def site_prefix(config):
    """The path of site_url, which the pages use for some of their assets."""
    path = urllib.parse.urlparse(config["site_url"] or "/").path
    return path if path.endswith("/") else path + "/"


class QuietHandler(http.server.SimpleHTTPRequestHandler):
    """The pages of one section, served without a line per request."""

    def log_message(self, format, *args):
        pass


@contextlib.contextmanager
def served(root):
    """The directory, served on a port of its own: the pages ask for absolute
    paths under site_url, which a browser reading them from disk cannot find."""
    handler = functools.partial(QuietHandler, directory=str(root))
    server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), handler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    try:
        yield f"http://127.0.0.1:{server.server_address[1]}"
    finally:
        server.shutdown()
        server.server_close()


def render(section, every_page, directory, prefix):
    root = directory / section.slug
    site_dir = root / prefix.strip("/") if prefix != "/" else root
    config_path = overlay_config(section, every_page, site_dir, directory)
    build(config_path)
    printable = site_dir / "print_page" / "index.html"
    if not printable.exists():
        raise SystemExit(f"error: {section.slug}: the print page was not generated")
    OUTPUT.mkdir(parents=True, exist_ok=True)
    pdf = OUTPUT / f"easylocal-{section.slug}.pdf"
    browser = find_browser()
    if browser is None:
        raise SystemExit(
            "error: no browser to print with: install Chrome or Chromium, or\n"
            "       set $CHROME to one."
        )
    with served(root) as origin:
        print_page(browser, f"{origin}{prefix}print_page/", pdf)
    return pdf


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

    config = read_config()
    sections = sections_of(config["nav"])
    every_page = [page for section in sections for page in section.pages]
    if arguments.only:
        sections = [section for section in sections if section.slug == arguments.only]
        if not sections:
            raise SystemExit(f"error: no section is named {arguments.only}")

    if arguments.list_sections:
        for section in sections:
            print(f"{section.slug}: {', '.join(section.pages)}")
        return 0

    prefix = site_prefix(config)
    with tempfile.TemporaryDirectory(prefix="easylocal-pdf-") as temporary:
        for section in sections:
            pdf = render(section, every_page, Path(temporary), prefix)
            size = pdf.stat().st_size / 1024
            print(f"{pdf.relative_to(ROOT)} ({size:.0f} KiB)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
