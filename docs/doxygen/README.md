# Doxygen helpers

`doxygenMathFilter.py` is the Doxygen input filter for Markdown. Markdown pages
use `$...$` and `$$ ... $$` so they render on GitHub, VS Code and Obsidian;
Doxygen needs `\f$...\f$` and `\f[...\f]`. The filter also converts `[@key]`
citations to `\cite{key}` and leaves fenced/inline code untouched.

It is wired in `docs/Doxyfile`:

```text
FILTER_PATTERNS = *.md=./docs/doxygen/doxygenMathFilter.py
```

Run it directly only when debugging the docs pipeline:

```bash
python3 docs/doxygen/doxygenMathFilter.py docs/theory/heat.md | head
```

The regular documentation build is:

```bash
cmake --build buildCPU --target docHtml
```
