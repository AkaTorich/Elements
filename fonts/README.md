# fonts

Font files for the `examples/fonts.cpp` showcase. They are **not** bundled in the
repository — system fonts cannot be redistributed.

To run the fonts example, put `.ttf` files here (for example copied from
`C:\Windows\Fonts`) with the names listed at the top of `examples/fonts.cpp`
(`arial.ttf`, `times.ttf`, `cour.ttf`, …), or point the `ELFONTS` environment variable
to a folder of fonts.

The embedding mechanism itself — `el_font_add(name, path)` and
`el_font_add_memory(name, bytes, size)` — works with **any** `.ttf`/`.otf`, so use fonts
you have the rights to distribute.
