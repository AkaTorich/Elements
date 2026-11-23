# fonts

30 `.ttf` fonts used by the `examples/fonts.cpp` showcase. The example loads each one with
`el_font_add(name, path)` and shows a sample line in it.

The font path is taken from the `ELFONTS` environment variable, or from `<exe folder>\..\fonts`.

The embedding mechanism — `el_font_add(name, path)` and `el_font_add_memory(name, bytes, size)`
— works with any `.ttf`/`.otf`.
