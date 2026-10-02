# EasyLocal branding

The symbol shows a current solution (the filled centre node), its neighbourhood (the hollow nodes) and the move that was chosen (in orange).

## Files

| File | Use |
|---|---|
| `easylocal-logo.svg` / `-dark.svg` / `-mono.svg` | Horizontal logo (symbol + wordmark): README, docs header, slides |
| `easylocal-logo-vertical.svg` / `-dark.svg` / `-mono.svg` | Stacked logo: title slides, posters |
| `easylocal-symbol.svg` / `-dark.svg` / `-mono.svg` | Symbol only, for 32 px and up |
| `easylocal-symbol-small.svg` | Simplified symbol, for 32 px and below |
| `favicon.svg` | Browser favicon, switches automatically in dark mode |
| `favicon.ico` | Fallback favicon (16, 32, 48 px) |
| `apple-touch-icon.png` | 180 px home-screen icon |
| `easylocal-avatar-512.png` | GitHub organisation / repo avatar |
| `easylocal-social-preview.png` / `.svg` | GitHub social preview (1280×640) |

The `-dark` files are for dark backgrounds. The `-mono` files use `currentColor`, so they take the colour of the surrounding text.

## Colours

| Role | Hex |
|---|---|
| Ink | `#1C1C22` |
| Paper (ink on dark) | `#F4F1EA` |
| Orange (the chosen move) | `#E0622B` |

Orange is used only for the chosen move in the symbol. The wordmark always uses ink or paper.

## Typography

The wordmark is set in Space Grotesk Medium (500), with tracking −0.025 em. It is stored as outlines, so the font does not need to be installed. The name is always written **EasyLocal**: no `++`, no spaces.

## Rules

- Leave clear space around the logo at least as wide as one hollow node.
- Don't recolour, stretch or rotate the symbol.
- Don't add effects such as shadows or gradients.
- Below 32 px, use `easylocal-symbol-small.svg`.

## README snippet

```html
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="docs/assets/branding/easylocal-logo-dark.svg">
  <img alt="EasyLocal" src="docs/assets/branding/easylocal-logo.svg" height="64">
</picture>
```

## Regenerating

Run `python3 generate.py`. It needs `fonttools`, `uharfbuzz`, `cairosvg`, `Pillow` and the Space Grotesk 500 TTF.
