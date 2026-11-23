# Elements — библиотека окон для Windows (C++)

Нативная библиотека интерфейса в духе WPF, перенос прототипа «библиотеки дизайнов»
из DynastyOS (`<elements.h>`) на настоящую Windows. Рисование — на **GDI+** (без
сторонних зависимостей). Программа строит дерево элементов один раз и запускает
`el_run()`; панели сами раскладывают детей (measure/arrange, как в WPF).

Тот же API, что в DyOS: любой код против `elements.h` собирается и там, и здесь.

## Что где

```
Elements/
  include/elements.h     — единственный заголовок, который подключает программа (API, C-совместимый)
  src/                   — реализация (каждый файл — свой слой):
    el_internal.h          общий внутренний заголовок: структура элемента, контекст, помощники
    el_core.cpp            контекст, создание элементов, свойства, шрифты/текст, цикл el_run/el_frame
    el_theme.cpp           темы: тёмная/светлая, 16 именованных, 14 цветовых слотов
    el_layout.cpp          раскладка: stack/dock/wrap/grid/scroll/canvas/border/group/tabs, align, сетка "Auto,*,2*,120"
    el_controls.cpp        ввод: попадание мышью, наведение, щелчки, текстовый редактор, combo, подсказки
    el_paint.cpp           отрисовка всех элементов на GDI+; примитивы своего рисования el_g_*
    el_menu.cpp            полоса меню, выпадающие и подменю, контекстное меню
    el_style.cpp           оформление строкой (CSS-подобно) el_style и таблица стилей el_stylesheet
    el_anim.cpp            анимации (сдвиг/масштаб/поворот/прозрачность), плавности, таймеры
    el_dialog.cpp          сообщение, вопрос, тост, выбор файла/папки, модальное окно
    el_win32.cpp           окно, цикл сообщений, ввод, буфер кадра, НЕСТАНДАРТНЫЕ окна
  examples/              — примеры (ниже)
  build.cmd             — сборка (Release, статический CRT)
  lib/Elements.lib      — библиотека после сборки
  bin/                  — собранные примеры
```

## Сборка

Нужен Visual Studio с инструментами C++. Из PowerShell в папке проекта:

```powershell
& ".\build.cmd"        # x64: статическая lib + все примеры в bin\
& ".\build_all.ps1"    # все варианты: x64 и x86, статические и динамические (DLL) в dist\
```

`build.cmd` собирает `lib\Elements.lib` и примеры в `bin\`. `build_all.ps1` собирает:

```
dist\x64\static\Elements.lib        dist\x86\static\Elements.lib        (статические)
dist\x64\dll\Elements.dll + .lib    dist\x86\dll\Elements.dll + .lib    (динамические: DLL + import-lib)
```

Подключение в своей программе: добавить `include\` в пути заголовков, слинковать нужную
`Elements.lib` + `gdiplus.lib user32.lib gdi32.lib comdlg32.lib shell32.lib ole32.lib`,
собрать как оконное приложение (`/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup`).
Исходники собирать с `/utf-8` — строки в программе в UTF-8. Для DLL положить `Elements.dll`
рядом с программой.

## Коротко об API

- **Окно и цикл:** `el_window`, `el_run` (или свой `while (el_frame())`), `el_quit`.
- **Панели:** `el_stack`, `el_dock`, `el_wrap`, `el_grid("Auto,*,2*,120", "...")`, `el_scroll`, `el_canvas`, `el_border`, `el_group`, `el_tabs`/`el_tab`.
- **Элементы:** `el_text`, `el_button`, `el_check`, `el_radio`, `el_edit`, `el_memo`, `el_password`, `el_combo`, `el_listbox`, `el_slider`, `el_progress`, `el_number`, `el_image`, `el_separator`, `el_toolbar`/`el_tool`.
- **Меню:** `el_menubar`, `el_menu` ("Пункт\tCtrl+N"), `el_menu_check`, `el_context_menu`.
- **Свойства:** `el_size/min/max`, `el_margin`, `el_padding`, `el_align`, `el_cell`, `el_dock_side`, `el_gap`, `el_set_text`, `el_text_bind`, `el_tooltip`, `el_font`, `el_colors`, `el_frame_style`.
- **Тема:** `el_theme(dark)`, `el_accent`, `el_theme_color(slot, rgb)`, `el_corner_radius`, `el_theme_named("Ночь"…)`.
- **Оформление строкой (как CSS):** `el_style(e, "gradient:#1a2a6c #b21f1f diagonal; radius:12; background-alpha:160; shadow:#000 18 0 6 120; hover-background:#fff; font-weight:bold; text-align:center")`; таблица — `el_stylesheet`.
- **События:** `el_on_click`, `el_on_change`, `el_clicked`, `el_changed`.
- **Анимации:** `el_animate(e,"opacity",0,255,300,EL_EASE_OUT,0)`, `el_appear(e,"pop",400)`, `el_transform`, таймеры `el_every`.
- **Готовые окна:** `el_message`, `el_ask`, `el_toast`, `el_open_file`, `el_save_file`, `el_pick_folder`, `el_dialog`.
- **Своё рисование:** `el_paint` (поверхность) + примитивы `el_g_fill/round/frame/line/disc/ring/shade/text/poly/poly_frame/spline/blob/arc`.
- **Эффекты окна:** `el_effect("chromatic"|"scanlines"|"noise"|"vignette"|"glitch"|"bloom"|"pixelate"|"grayscale"|"sepia"|"invert", сила)`, `el_effects_clear()` — постобработка всего кадра, можно несколько сразу.
- **Встраиваемые шрифты:** `el_font_add(name, "файл.ttf")` / `el_font_add_memory(name, bytes, size)`, затем `el_font_face(e, name)` или стиль `font-family:name`.
- **Размер окна:** обычные окна тянутся за края с живой перекладкой; `el_window_resizable(on)` — для безрамочных и фигурных тоже.

## Нестандартные окна

Окно может быть любой формы, размера и пропорций, и рисовать себя само:

- `el_frameless(1)` — убрать системную рамку: своя «шапка», кнопки, фон.
- `el_window_round(r)` / `el_window_shape("round:24" | "ellipse" | "pixels")` — форма окна.
  Режим **`"pixels"`** берёт форму из **непрозрачности** того, что нарисовано, — так
  получаются гладкие нестандартные контуры (треугольник, «капля», угловатая панель).
- `el_window_region(xy, n)` — произвольный многоугольный контур.
- `el_window_aspect(16, 10)` — держать пропорции при изменении размера.
- `el_window_minmax`, `el_window_opacity`, `el_drag_region(e)` (тянуть окно за свой элемент),
  `el_window_canvas(fn, arg)` — рисовать фон окна целиком через `el_g_*`.

Произвольные формы (в том числе кривые) рисуются примитивами:
`el_g_poly` (многоугольник), `el_g_blob` (гладкая замкнутая кривая — любая «кривая» форма),
`el_g_spline` (гладкая кривая-штрих), `el_g_arc` (дуга).

## Примеры (`bin\`)

| Пример | Что показывает |
|---|---|
| `hello` | минимум: поле, кнопка, подпись |
| `form` | меню, вкладки, сетка формы, список, многострочное поле, подсказки |
| `design` | оформление строкой: градиенты, стекло, тени, плавное наведение |
| `custom_window` | безрамочное окно: свои углы, шапка, фон, пропорции 16:10 |
| `shaped` | окно-«пилюля»: форма по непрозрачности рисунка |
| `triangle` | **треугольное окно с треугольными кнопками** |
| `curves` | окно-«капля» со скруглёнными кривыми краями и круглыми кнопками, синусоида и дуга |
| `cyberpunk` | угловатая панель в духе Cyberpunk 2077: эффекты дисперсии, развёртки, свечения; неон жёлтый/голубой/розовый |
| `effects` | витрина эффектов окна: 10 эффектов переключателями |
| `fonts` | 30 встраиваемых шрифтов (из `fonts\`, через `el_font_add`) |

## Что реализовано и что нет

Готово: всё перечисленное выше — раскладка, элементы, темы, оформление строкой и
таблицей, меню, анимации, диалоги, своё рисование, нестандартные окна и формы,
эффекты окна, встраиваемые шрифты, изменение размера с живой перекладкой, HiDPI,
экономный простой (окно спит, пока нет событий). Полная документация — `ДОКУМЕНТАЦИЯ.md`.

Пока упрощено/не реализовано (объявления в `elements.h` есть, кроме отмеченных):
загрузка окна из XML-разметки, `TreeView`/`DataGrid`, редактор кода с подсветкой,
панель «Найти/Заменить», выбор цвета/даты, `background-image` в стиле (парсится, не
рисуется). Их можно добавлять тем же способом — отдельным слоем в `src/`.
