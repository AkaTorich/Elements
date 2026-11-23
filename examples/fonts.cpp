// 30 встраиваемых шрифтов: каждый загружается из .ttf (el_font_add) и может быть
// встроен в программу. Здесь — витрина: имя и образец строки в каждом шрифте.
// Папка со шрифтами: переменная ELFONTS или <папка_exe>\..\fonts.
#include <elements.h>
#include <windows.h>
#include <stdio.h>
#include <string>

// 30 собственных шрифтов, нарисованных штрихами (tools/genfonts.py -> fonts/elmNN.ttf)
struct FontDef { const char *file; const char *name; };
FontDef FONTS[] = {
    { "elm01.ttf", "Elm Hairline" }, { "elm02.ttf", "Elm Thin" }, { "elm03.ttf", "Elm Regular" },
    { "elm04.ttf", "Elm Medium" }, { "elm05.ttf", "Elm Bold" }, { "elm06.ttf", "Elm Heavy" },
    { "elm07.ttf", "Elm Square" }, { "elm08.ttf", "Elm Square Bold" }, { "elm09.ttf", "Elm Butt" },
    { "elm10.ttf", "Elm Italic" }, { "elm11.ttf", "Elm Italic Bold" }, { "elm12.ttf", "Elm Thin Italic" },
    { "elm13.ttf", "Elm Condensed" }, { "elm14.ttf", "Elm Cond Bold" }, { "elm15.ttf", "Elm Wide" },
    { "elm16.ttf", "Elm Wide Bold" }, { "elm17.ttf", "Elm Tall" }, { "elm18.ttf", "Elm Short" },
    { "elm19.ttf", "Elm Serif" }, { "elm20.ttf", "Elm Slab" }, { "elm21.ttf", "Elm Dotted" },
    { "elm22.ttf", "Elm Dotted Bold" }, { "elm23.ttf", "Elm Dashed" }, { "elm24.ttf", "Elm Marker" },
    { "elm25.ttf", "Elm Pen" }, { "elm26.ttf", "Elm Mono" }, { "elm27.ttf", "Elm Mono Thin" },
    { "elm28.ttf", "Elm Block" }, { "elm29.ttf", "Elm Round Bold" }, { "elm30.ttf", "Elm Script" },
};
const int NF = (int)(sizeof(FONTS) / sizeof(FONTS[0]));

// путь к папке шрифтов — в юникоде (пути могут быть с кириллицей)
static std::wstring fonts_dir()
{
    const wchar_t *e = _wgetenv(L"ELFONTS");
    if (e && *e) return e;
    wchar_t p[600]; GetModuleFileNameW(NULL, p, 600);
    wchar_t *s = wcsrchr(p, L'\\'); if (s) *s = 0;
    return std::wstring(p) + L"\\..\\fonts";
}
static std::string to_utf8(const std::wstring &w)
{
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, NULL, 0, NULL, NULL);
    std::string s(n > 0 ? n - 1 : 0, 0);
    if (n > 0) WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &s[0], n, NULL, NULL);
    return s;
}

int main(void)
{
    el w = el_window("30 встраиваемых шрифтов", 720, 640);
    el_window_minmax(420, 320, 0, 0);

    std::wstring dir = fonts_dir();

    el page = el_stack(w, 0);
    el_padding(page, 20, 18, 20, 18); el_gap(page, 4);
    el head = el_text(page, "30 встраиваемых шрифтов");
    el_style(head, "font-size:24; font-weight:bold");
    el sub = el_text(page, "Свои шрифты, нарисованные штрихами (tools/genfonts.py). Строчные берут начертания прописных.");
    el_style(sub, "color:#8fa3b8; font-size:13; margin:0 0 8 0");

    el sc = el_scroll(page);
    el list = el_stack(sc, 0); el_gap(list, 2);

    char label[64];
    for (int i = 0; i < NF; i++) {
        std::wstring pw = dir + L"\\";
        for (const char *c = FONTS[i].file; *c; c++) pw += (wchar_t)(unsigned char)*c;
        int ok = el_font_add(FONTS[i].name, to_utf8(pw).c_str());

        el row = el_border(list);
        el_style(row, "background:#161b24; radius:8; padding:10 14; margin:0 0 2 0");
        el g = el_grid(row, "230,*", "Auto"); el_gap(g, 12);
        _snprintf(label, sizeof label, "%2d. %s", i + 1, FONTS[i].name);
        el nm = el_text(g, label);
        el_style(nm, "color:#c9d1d9; font-size:13");
        el smp = el_text(g, ok ? "Съешь ещё этих мягких булок — The quick brown fox 0123" : "(не найден)");
        el_cell(smp, 0, 1, 1, 1);
        if (ok) { el_font(smp, 20, 0); el_font_face(smp, FONTS[i].name); }
        else el_style(smp, "color:#e06c75");
    }

    el_run();
    return 0;
}
