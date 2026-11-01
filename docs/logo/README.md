# saturnCFD logo

The primary mark is a compact, transparent terminal-style lockup:

```text
[ Saturn ]   > saturnCFD
```

`saturnCFD.svg` is the **light/default** version (black Saturn, black
`saturn`, dark-green rings and `CFD`), and `saturnCFD-dark.svg` is the
**terminal/dark** version (black Saturn, green rings, white `saturn`,
bright-green `CFD`). Both SVGs have transparent backgrounds.

## Design

- Terminal palette: black, green and white; the wordmark is preceded by a
  green `>` prompt.
- The wordmark is `saturnCFD` in **JetBrains Mono Bold**, outlined to paths so
  no font installation is required.
- The ring is a dense set of fine green lines that wraps the whole planet:
  back rings are visible behind the planet and the front half crosses it.
- The planet is black in both versions and keeps the simple round silhouette
  with a thin ring-colour rim.
- Compact 800 x 300 canvas with a small emblem-to-text gap.
- No trailing cursor; `>` is the prompt.

### Light version

| element | colour |
|---|---|
| Saturn | black `#111111` |
| rings, `>`, `CFD` | dark terminal green `#0B7A3B` |
| `saturn` | black `#111111` |
| background | transparent (white pages) |

### Dark terminal version

| element | colour |
|---|---|
| Saturn | black |
| rings, `>`, `CFD` | terminal green `#00E676` |
| `saturn` | terminal white `#E6E6E6` |
| background | transparent (black terminal pages) |

`saturn` is white in the dark version only because black would disappear on a
black terminal background. The black planet is defined by the green rim and
the ring lines that cross it.

## Preview

<table>
<tr>
<td><img src="saturnCFD-on-light.png" width="360" alt="saturnCFD on light"></td>
<td><img src="saturnCFD-on-dark.png" width="360" alt="saturnCFD on dark"></td>
</tr>
</table>

## Usage

```html
<!-- light page -->
<img src="docs/logo/saturnCFD.svg" alt="saturnCFD" width="320">

<!-- black/dark page -->
<img src="docs/logo/saturnCFD-dark.svg" alt="saturnCFD" width="320">
```

In LaTeX, use the vector PDF:

```latex
\includegraphics[width=8cm]{saturnCFD.pdf}
```

Keep clear space around the lockup of at least one planet radius. Do not
recolour, stretch or crop the mark.

## Files

```text
saturnCFD.svg             light/default lockup (transparent)
saturnCFD-dark.svg        terminal/dark lockup (transparent)
saturnCFD-light.svg       explicit light copy of the default
saturnCFD.png             light lockup preview (transparent, 1200 x 450)
saturnCFD.pdf             light vector export
saturnCFD-on-light.png    600 x 225 preview on white
saturnCFD-on-dark.png     600 x 225 preview on black
saturnCFD-preview.png     stacked light/dark preview
```
