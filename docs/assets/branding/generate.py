import os, io
import uharfbuzz as hb
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.boundsPen import BoundsPen
from fontTools.pens.transformPen import TransformPen
import cairosvg
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
FONT = os.environ.get("SPACE_GROTESK_TTF", os.path.join(HERE, "SpaceGrotesk-Medium.ttf"))
OUT = HERE
os.makedirs(OUT, exist_ok=True)

INK, PAPER, ORANGE = "#1C1C22", "#F4F1EA", "#E0622B"

# ---------- symbol geometry (120x120 design grid) ----------
def symbol(ink, acc):
    return f'''<g stroke="{ink}" stroke-width="3" stroke-linecap="round">
<line x1="68" y1="60" x2="87" y2="60"/><line x1="64" y1="67" x2="73" y2="83"/><line x1="56" y1="67" x2="47" y2="83"/><line x1="52" y1="60" x2="33" y2="60"/><line x1="56" y1="53" x2="47" y2="37"/>
</g>
<line x1="64" y1="53" x2="72" y2="38" stroke="{acc}" stroke-width="5" stroke-linecap="round"/>
<g fill="none" stroke="{ink}" stroke-width="3"><circle cx="94" cy="60" r="7"/><circle cx="77" cy="89.4" r="7"/><circle cx="43" cy="89.4" r="7"/><circle cx="26" cy="60" r="7"/><circle cx="43" cy="30.6" r="7"/></g>
<circle cx="60" cy="60" r="9" fill="{ink}"/>
<circle cx="77" cy="30.6" r="9" fill="{acc}"/>'''

def symbol_small(ink, acc):
    return f'''<g stroke="{ink}" stroke-width="8" stroke-linecap="round"><line x1="73" y1="60" x2="80" y2="60"/><line x1="52" y1="73" x2="48" y2="80"/><line x1="47" y1="60" x2="40" y2="60"/></g>
<line x1="67" y1="48" x2="72" y2="40" stroke="{acc}" stroke-width="10" stroke-linecap="round"/>
<g fill="none" stroke="{ink}" stroke-width="8"><circle cx="97" cy="60" r="11"/><circle cx="41" cy="93" r="11"/><circle cx="23" cy="60" r="11"/></g>
<circle cx="60" cy="60" r="16" fill="{ink}"/>
<circle cx="79" cy="27" r="16" fill="{acc}"/>'''

SYM_VB = (12, 12, 96, 96)      # tight square around the full symbol
SMALL_VB = (4, 6, 112, 112)    # tight square around the small cut

# ---------- wordmark as outlines ----------
tt = TTFont(FONT)
gs = tt.getGlyphSet()
UPM = tt["head"].unitsPerEm
blob = hb.Blob.from_file_path(FONT)
hbfont = hb.Font(hb.Face(blob))

def wordmark(text="EasyLocal", tracking=-0.025):
    buf = hb.Buffer(); buf.add_str(text); buf.guess_segment_properties()
    hb.shape(hbfont, buf, {"kern": True, "liga": True})
    order = tt.getGlyphOrder()
    pen = SVGPathPen(gs); bpen = BoundsPen(gs)
    x = 0
    for info, pos in zip(buf.glyph_infos, buf.glyph_positions):
        name = order[info.codepoint]
        # flip y: font units y-up -> svg y-down, baseline at 0
        t = (1, 0, 0, -1, x + pos.x_offset, -pos.y_offset)
        gs[name].draw(TransformPen(pen, t))
        gs[name].draw(TransformPen(bpen, t))
        x += pos.x_advance + tracking * UPM
    xmin, ymin, xmax, ymax = bpen.bounds
    return pen.getCommands(), (xmin, ymin, xmax, ymax)

WM_PATH, WM_B = wordmark()
cap = tt["OS/2"].sCapHeight  # optical alignment on cap height

def svg(vb, body, title):
    x, y, w, h = vb
    return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="{x:g} {y:g} {w:g} {h:g}" role="img" aria-label="{title}">'
            f'<title>{title}</title>\n{body}\n</svg>\n')

def write(name, content):
    with open(os.path.join(OUT, name), "w") as f: f.write(content)

# symbols
for suf, ink, acc in [("", INK, ORANGE), ("-dark", PAPER, ORANGE), ("-mono", "currentColor", "currentColor")]:
    write(f"easylocal-symbol{suf}.svg", svg(SYM_VB, symbol(ink, acc), "EasyLocal"))
write("easylocal-symbol-small.svg", svg(SMALL_VB, symbol_small(INK, ORANGE), "EasyLocal"))

# favicon: small cut, adapts to browser dark mode
fav = f'''<style>.i{{stroke:{INK};}} .f{{fill:{INK};}} @media (prefers-color-scheme: dark){{.i{{stroke:{PAPER};}} .f{{fill:{PAPER};}}}}</style>
<g class="i" stroke-width="8" stroke-linecap="round"><line x1="73" y1="60" x2="80" y2="60"/><line x1="52" y1="73" x2="48" y2="80"/><line x1="47" y1="60" x2="40" y2="60"/></g>
<line x1="67" y1="48" x2="72" y2="40" stroke="{ORANGE}" stroke-width="10" stroke-linecap="round"/>
<g class="i" fill="none" stroke-width="8"><circle cx="97" cy="60" r="11"/><circle cx="41" cy="93" r="11"/><circle cx="23" cy="60" r="11"/></g>
<circle class="f" cx="60" cy="60" r="16"/><circle cx="79" cy="27" r="16" fill="{ORANGE}"/>'''
write("favicon.svg", svg(SMALL_VB, fav, "EasyLocal"))

# ---------- lockups ----------
def place_symbol(ink, acc, x, y, size):
    s = size / SYM_VB[2]
    return f'<g transform="translate({x - SYM_VB[0]*s:.3f} {y - SYM_VB[1]*s:.3f}) scale({s:.5f})">{symbol(ink, acc)}</g>'

def place_word(fill, x, baseline, cap_px):
    s = cap_px / cap
    return f'<path fill="{fill}" transform="translate({x - WM_B[0]*s:.3f} {baseline:.3f}) scale({s:.5f})" d="{WM_PATH}"/>'

def word_width(cap_px): return (WM_B[2] - WM_B[0]) * cap_px / cap

def horizontal(ink, acc, word):
    S = 100                 # symbol box
    capx = 40               # cap height of wordmark (~ 0.4 of symbol)
    gap = 22
    pad = 0
    W = S + gap + word_width(capx)
    base = S / 2 + capx / 2  # cap-height centred on symbol centre
    body = place_symbol(ink, acc, 0, 0, S) + "\n" + place_word(word, S + gap, base, capx)
    return svg((0, 0, round(W + pad, 2), S), body, "EasyLocal")

def vertical(ink, acc, word):
    capx = 34
    ww = word_width(capx)
    S = 120
    W = max(ww, S)
    gap = 18
    body = place_symbol(ink, acc, (W - S) / 2, 0, S) + "\n" + place_word(word, (W - ww) / 2, S + gap + capx, capx)
    # descender of 'y' below baseline
    desc = (WM_B[3]) * capx / cap
    return svg((0, 0, round(W, 2), round(S + gap + capx + desc, 2)), body, "EasyLocal")

for suf, ink, acc, word in [("", INK, ORANGE, INK), ("-dark", PAPER, ORANGE, PAPER),
                             ("-mono", "currentColor", "currentColor", "currentColor")]:
    write(f"easylocal-logo{suf}.svg", horizontal(ink, acc, word))
    write(f"easylocal-logo-vertical{suf}.svg", vertical(ink, acc, word))

# ---------- rasters ----------
def png(svgtext, size, name=None, w=None, h=None):
    data = cairosvg.svg2png(bytestring=svgtext.encode(), output_width=w or size, output_height=h or size)
    if name:
        with open(os.path.join(OUT, name), "wb") as f: f.write(data)
    return Image.open(io.BytesIO(data))

small_light = svg(SMALL_VB, symbol_small(INK, ORANGE), "EasyLocal")
icons = [png(small_light, s) for s in (16, 32, 48)]
icons[2].save(os.path.join(OUT, "favicon.ico"), sizes=[(16, 16), (32, 32), (48, 48)], append_images=icons[:2])

def on_tile(sym_svg_body, vb, bg, size, frac, radius=0):
    x, y, w, h = vb
    inner = size * frac; off = (size - inner) / 2; s = inner / w
    body = (f'<rect width="{size}" height="{size}" rx="{radius}" fill="{bg}"/>'
            f'<g transform="translate({off - x*s:.3f} {off - y*s:.3f}) scale({s:.5f})">{sym_svg_body}</g>')
    return svg((0, 0, size, size), body, "EasyLocal")

png(on_tile(symbol(PAPER, ORANGE), SYM_VB, INK, 180, 0.74), 180, "apple-touch-icon.png")
png(on_tile(symbol(PAPER, ORANGE), SYM_VB, INK, 512, 0.66), 512, "easylocal-avatar-512.png")

# social preview 1280x640 (GitHub)
capx = 64; S = 170; gap = 36
ww = word_width(capx); total = S + gap + ww
x0 = (1280 - total) / 2; y0 = (640 - S) / 2
body = (f'<rect width="1280" height="640" fill="{INK}"/>' + place_symbol(PAPER, ORANGE, x0, y0, S)
        + place_word(PAPER, x0 + S + gap, y0 + S / 2 + capx / 2, capx))
soc = svg((0, 0, 1280, 640), body, "EasyLocal")
write("easylocal-social-preview.svg", soc)
png(soc, 0, "easylocal-social-preview.png", w=1280, h=640)

print("done")
