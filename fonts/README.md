# fonts

30 **original** fonts for the `examples/fonts.cpp` showcase. The letters are drawn as
strokes on a grid and generated into `.ttf` by `tools/genfonts.py` — run
`python tools/genfonts.py` (needs `fontTools`) to regenerate `elm01.ttf … elm30.ttf`.

The 30 files are 30 rendering styles of the same hand-drawn strokes — weight (hairline …
block), slant (italic/script), caps (round/square/butt), plus serif, slab, dotted, dashed
and marker. Lowercase uses the uppercase shapes. Coverage: Latin, Cyrillic, digits,
punctuation.

The font path is taken from the `ELFONTS` environment variable, or from
`<exe folder>\..\fonts`. The embedding mechanism — `el_font_add(name, path)` and
`el_font_add_memory(name, bytes, size)` — works with any `.ttf`/`.otf`.
