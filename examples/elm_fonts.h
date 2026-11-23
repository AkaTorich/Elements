// Список 30 собственных шрифтов Elements (tools/fontgen -> fonts/*.ttf) и их загрузка.
// Папка со шрифтами: переменная ELFONTS или <папка_exe>\..\fonts (пути могут быть с кириллицей).
#pragma once
#include <elements.h>
#include <windows.h>
#include <string>

struct ElmFont { const char *file; const char *name; };

static const ElmFont ELM_FONTS[] = {
    { "ElmGrotesk.ttf", "Elm Grotesk" },             { "ElmGroteskLight.ttf", "Elm Grotesk Light" },
    { "ElmGroteskBold.ttf", "Elm Grotesk Bold" },    { "ElmGroteskBlack.ttf", "Elm Grotesk Black" },
    { "ElmGroteskItalic.ttf", "Elm Grotesk Italic" },{ "ElmGroteskCondensed.ttf", "Elm Grotesk Condensed" },
    { "ElmGroteskExtended.ttf", "Elm Grotesk Extended" }, { "ElmCompressed.ttf", "Elm Compressed" },
    { "ElmGeometric.ttf", "Elm Geometric" },         { "ElmGeometricLight.ttf", "Elm Geometric Light" },
    { "ElmGeometricBold.ttf", "Elm Geometric Bold" },{ "ElmHumanist.ttf", "Elm Humanist" },
    { "ElmHumanistBold.ttf", "Elm Humanist Bold" },  { "ElmHumanistItalic.ttf", "Elm Humanist Italic" },
    { "ElmRounded.ttf", "Elm Rounded" },             { "ElmRoundedBold.ttf", "Elm Rounded Bold" },
    { "ElmTechno.ttf", "Elm Techno" },               { "ElmTechnoBold.ttf", "Elm Techno Bold" },
    { "ElmMono.ttf", "Elm Mono" },                   { "ElmMonoBold.ttf", "Elm Mono Bold" },
    { "ElmTypewriter.ttf", "Elm Typewriter" },       { "ElmSlab.ttf", "Elm Slab" },
    { "ElmSlabBold.ttf", "Elm Slab Bold" },          { "ElmSerif.ttf", "Elm Serif" },
    { "ElmSerifBold.ttf", "Elm Serif Bold" },        { "ElmSerifItalic.ttf", "Elm Serif Italic" },
    { "ElmDidone.ttf", "Elm Didone" },               { "ElmOldstyle.ttf", "Elm Oldstyle" },
    { "ElmHairline.ttf", "Elm Hairline" },           { "ElmWideLight.ttf", "Elm Wide Light" },
};
static const int ELM_FONT_COUNT = (int)(sizeof(ELM_FONTS) / sizeof(ELM_FONTS[0]));

static std::wstring elm_fonts_dir()
{
    const wchar_t *e = _wgetenv(L"ELFONTS");
    if (e && *e) return e;
    wchar_t p[600]; GetModuleFileNameW(NULL, p, 600);
    wchar_t *s = wcsrchr(p, L'\\'); if (s) *s = 0;
    return std::wstring(p) + L"\\..\\fonts";
}

static std::string elm_utf8(const std::wstring &w)
{
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, NULL, 0, NULL, NULL);
    std::string s(n > 0 ? n - 1 : 0, 0);
    if (n > 0) WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &s[0], n, NULL, NULL);
    return s;
}

// загрузить все шрифты; ok[i] — загрузился ли i-й; вернёт сколько загрузилось
static int elm_load_fonts(int *ok)
{
    std::wstring dir = elm_fonts_dir();
    int n = 0;
    for (int i = 0; i < ELM_FONT_COUNT; i++) {
        std::wstring pw = dir + L"\\";
        for (const char *c = ELM_FONTS[i].file; *c; c++) pw += (wchar_t)(unsigned char)*c;
        ok[i] = el_font_add(ELM_FONTS[i].name, elm_utf8(pw).c_str());
        n += ok[i] ? 1 : 0;
    }
    return n;
}
