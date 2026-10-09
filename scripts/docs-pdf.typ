// The look of the section PDFs (scripts/docs-pdf.py), added to the document
// pandoc writes: the body text, the code blocks and the pictures of the site,
// set for paper.

// Code blocks, which hold whole classes: smaller than the text, on the grey of
// the site, and not broken across pages when they fit on one.
#show raw.where(block: true): set text(size: 7.6pt)
#show raw.where(block: true): it => block(
  fill: luma(246),
  inset: 8pt,
  radius: 2pt,
  width: 100%,
  breakable: true,
  it,
)
#show raw.where(block: false): set text(size: 0.92em)

// Links, in the purple of the site, so that a reader sees what to follow.
#show link: set text(fill: rgb("#5a3fa0"))

// Headings: a page of its own for a chapter, space around the others.
#show heading.where(level: 1): it => {
  pagebreak(weak: true)
  block(above: 0pt, below: 14pt, it)
}
#show heading.where(level: 2): set block(above: 18pt, below: 10pt)

// The pictures of a page, the diagrams and the screenshots of the terminal,
// which are drawings and should not be stretched.
#show image: set align(center)

#set par(justify: true, leading: 0.62em)
#set text(hyphenate: true)
