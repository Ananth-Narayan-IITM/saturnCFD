#!/usr/bin/env python3
"""Convert Markdown math to Doxygen math.

Markdown pages use `$...$` and `$$ ... $$` so they render on GitHub,
VS Code and Obsidian. Doxygen needs `\\f$...\\f$` and `\\f[...\\f]`.

Usage (Doxygen FILTER_PATTERNS):
    tools/doxygenMathFilter.py path/to/page.md

The filter leaves fenced code blocks and inline code untouched, and converts
`[@key]` citations to `\\cite{key}`.
"""

import re
import sys


def convert(text: str) -> str:
    out = []
    in_fence = False
    in_display = False

    for line in text.splitlines(keepends=True):
        stripped = line.lstrip()

        if not in_display and (stripped.startswith("```") or stripped.startswith("~~~")):
            in_fence = not in_fence
            out.append(line)
            continue

        if in_fence:
            out.append(line)
            continue

        if line.strip() == "$$":
            out.append("\\f[\n" if not in_display else "\\f]\n")
            in_display = not in_display
            continue

        if in_display:
            out.append(line)
            continue

        # Split on single backticks so inline code is not transformed.
        parts = line.split("`")
        for i, part in enumerate(parts):
            if i % 2 == 1:
                continue
            part = re.sub(r"\$([^$\n]+)\$", r"\\f$\1\\f$", part)
            part = re.sub(r"\[@([A-Za-z0-9_:.\-]+)\]", r"\\cite{\1}", part)
            parts[i] = part
        out.append("`".join(parts))

    return "".join(out)


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: doxygenMathFilter.py <markdown-file>", file=sys.stderr)
        return 2
    with open(sys.argv[1], encoding="utf-8") as f:
        sys.stdout.write(convert(f.read()))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
