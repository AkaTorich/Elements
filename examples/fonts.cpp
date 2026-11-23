// 30 встраиваемых шрифтов: каждый загружается из .ttf (el_font_add) и может быть
// встроен в программу. Здесь — витрина: имя и образец строки в каждом шрифте.
// Папка со шрифтами: переменная ELFONTS или <папка_exe>\..\fonts.
#include <elements.h>
#include <windows.h>
#include <stdio.h>
#include <string>

struct FontDef { const char *file; const char *name; };
FontDef FONTS[] = {
    { "arial.ttf", "Arial" }, { "times.ttf", "Times New Roman" }, { "cour.ttf", "Courier New" },
    { "verdana.ttf", "Verdana" }, { "georgia.ttf", "Georgia" }, { "tahoma.ttf", "Tahoma" },
    { "trebuc.ttf", "Trebuchet MS" }, { "comic.ttf", "Comic Sans MS" }, { "impact.ttf", "Impact" },
    { "calibri.ttf", "Calibri" }, { "candara.ttf", "Candara" }, { "consola.ttf", "Consolas" },
    { "constan.ttf", "Constantia" }, { "corbel.ttf", "Corbel" }, { "framd.ttf", "Franklin Gothic" },
    { "gabriola.ttf", "Gabriola" }, { "segoeui.ttf", "Segoe UI" }, { "segoepr.ttf", "Segoe Print" },
    { "segoesc.ttf", "Segoe Script" }, { "lucon.ttf", "Lucida Console" }, { "pala.ttf", "Palatino Linotype" },
    { "sylfaen.ttf", "Sylfaen" }, { "ebrima.ttf", "Ebrima" }, { "gadugi.ttf", "Gadugi" },
    { "Inkfree.ttf", "Ink Free" }, { "bahnschrift.ttf", "Bahnschrift" }, { "micross.ttf", "Microsoft Sans Serif" },
    { "mvboli.ttf", "Mv Boli" }, { "ariblk.ttf", "Arial Black" }, { "seguisb.ttf", "Segoe UI Semibold" },
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
    el sub = el_text(page, "Каждый загружен из .ttf через el_font_add и может встраиваться в программу.");
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
