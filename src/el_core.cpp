// Ядро Elements: контекст, создание элементов, свойства, вспомогательные
// функции (шрифты, текст, цвета, DPI), главный цикл el_run/el_frame.
#include "el_internal.h"
#include <cstring>
#include <cstdlib>
#include <map>

using namespace Gdiplus;

namespace eli {

Ctx g;

// ---------------------------------------------------------------- utils ----

std::wstring wide(const char *s) {
    if (!s || !*s) return std::wstring();
    int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, nullptr, 0);
    if (n <= 0) return std::wstring();
    std::wstring w(n - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, s, -1, &w[0], n);
    return w;
}
std::string narrow(const std::wstring &s) {
    if (s.empty()) return std::string();
    int n = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (n <= 0) return std::string();
    std::string o(n - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, &o[0], n, nullptr, nullptr);
    return o;
}

int px(int d) { return (int)(d * g.dpi + 0.5); }
int dip(int p) { return (int)(p / g.dpi + 0.5); }

u32 mix(u32 a, u32 b, double t) {
    if (t < 0) t = 0; if (t > 1) t = 1;
    int ar = (a >> 16) & 255, ag = (a >> 8) & 255, ab = a & 255;
    int br = (b >> 16) & 255, bg2 = (b >> 8) & 255, bb = b & 255;
    int r = (int)(ar + (br - ar) * t + 0.5);
    int gn = (int)(ag + (bg2 - ag) * t + 0.5);
    int bl = (int)(ab + (bb - ab) * t + 0.5);
    return (r << 16) | (gn << 8) | bl;
}

// --- шрифты (кэш) ---
// «лицо» (face): 0 — Segoe UI, 1 — Consolas (mono), 2+ — встроенные через el_font_add
struct FontKey { int size, bold, face; bool operator<(const FontKey &o) const {
    if (size != o.size) return size < o.size;
    if (bold != o.bold) return bold < o.bold;
    return face < o.face; } };
static std::map<FontKey, Font *> s_fonts;
static std::map<int, FontFamily *> s_fam;        // 0 Segoe UI, 1 Consolas, 2+ встроенные
static std::map<std::string, int> s_faceName;    // имя -> индекс лица
// по коллекции на каждый встроенный шрифт: в ней ровно одно семейство. (В общей коллекции GDI+
// отдаёт семейства по алфавиту, а не по порядку добавления — имя привязывалось к чужому шрифту.)
static std::vector<PrivateFontCollection *> s_pfcs;
static int s_nextFace = 2;

Font *get_font(int size, int bold, int face) {
    if (size <= 0) size = g.def_font;
    if (face < 0) face = 0;
    FontKey k{ size, bold ? 1 : 0, face };
    auto it = s_fonts.find(k);
    if (it != s_fonts.end()) return it->second;
    if (!s_fam.count(face)) {
        if (face == 1) { FontFamily *f = new FontFamily(L"Consolas"); if (!f->IsAvailable()) { delete f; f = new FontFamily(L"Courier New"); } s_fam[1] = f; }
        else { FontFamily *f = new FontFamily(L"Segoe UI"); if (!f->IsAvailable()) { delete f; f = new FontFamily(L"Tahoma"); } s_fam[0] = f; face = 0; k.face = 0; }
    }
    // размер шрифта задаём в пикселях (мы сами в HiDPI-пикселях рисуем)
    Font *f = new Font(s_fam[face], (REAL)px(size), bold ? FontStyleBold : FontStyleRegular, UnitPixel);
    s_fonts[k] = f;
    return f;
}

int font_face_by_name(const char *name) {
    if (!name || !*name) return 0;
    std::string s = name;
    if (s == "default") return 0;
    if (s == "mono") return 1;
    auto it = s_faceName.find(s);
    return it != s_faceName.end() ? it->second : 0;
}

static int font_take(PrivateFontCollection *pfc, const char *alias) {
    int cnt = pfc->GetFamilyCount();
    if (cnt <= 0) { delete pfc; return 0; }
    std::vector<FontFamily> fams(cnt); int found = 0;
    pfc->GetFamilies(cnt, fams.data(), &found);
    if (found <= 0) { delete pfc; return 0; }
    WCHAR fam[LF_FACESIZE] = { 0 }; fams[0].GetFamilyName(fam);
    FontFamily *ff = new FontFamily(fam, pfc);
    if (!ff->IsAvailable()) { delete ff; delete pfc; return 0; }
    s_pfcs.push_back(pfc);
    int idx = s_nextFace++;
    s_fam[idx] = ff; s_faceName[alias] = idx;
    request_redraw();
    return 1;
}

int el_font_add(const char *name, const char *path) {
    if (!name || !path) return 0;
    PrivateFontCollection *pfc = new PrivateFontCollection();
    if (pfc->AddFontFile(wide(path).c_str()) != Ok) { delete pfc; return 0; }
    return font_take(pfc, name);
}
int el_font_add_memory(const char *name, const void *data, int size) {
    if (!name || !data || size <= 0) return 0;
    PrivateFontCollection *pfc = new PrivateFontCollection();
    if (pfc->AddMemoryFont(data, size) != Ok) { delete pfc; return 0; }
    return font_take(pfc, name);
}
void el_font_face(el e, const char *family) { if (!valid(e)) return; g.el[e].mono = font_face_by_name(family); request_redraw(); }

int text_w(const std::wstring &s, int size, int bold, int mono) {
    int w, h; text_size(s, size, bold, mono, false, 0, &w, &h); return w;
}
void text_size(const std::wstring &s, int size, int bold, int mono, bool wrapW, int maxw, int *w, int *h) {
    Graphics *gr = g.measure_g;
    Font *f = get_font(size, bold, mono);
    StringFormat sf(StringFormat::GenericTypographic());
    sf.SetFormatFlags(sf.GetFormatFlags() | StringFormatFlagsMeasureTrailingSpaces);
    if (!wrapW) sf.SetFormatFlags(sf.GetFormatFlags() | StringFormatFlagsNoWrap);
    REAL lim = wrapW ? (REAL)px(maxw) : 100000.0f;
    RectF layout(0, 0, lim, 100000.0f), bound;
    std::wstring t = s.empty() ? std::wstring(L" ") : s;
    gr->MeasureString(t.c_str(), -1, f, layout, &sf, &bound);
    // возвращаем в ПИКСЕЛЯХ (вся внутренняя геометрия — в пикселях)
    if (w) *w = (int)(bound.Width + 1.5f);
    if (h) *h = (int)(bound.Height + 1.5f);
    if (s.empty() && w) *w = 0;
}

// ---------------------------------------------------------------- tree -----

int el_alloc(int kind) {
    int h = -1;
    for (int i = 1; i < (int)g.el.size(); i++) if (!g.el[i].alive) { h = i; break; }
    if (h < 0) { g.el.push_back(Element()); h = (int)g.el.size() - 1; }
    Element &e = g.el[h];
    e = Element();
    e.alive = true;
    e.kind = kind;
    e.w = e.h = e.minw = e.minh = e.maxw = e.maxh = -1;
    e.ha = e.va = A_STRETCH;
    e.rspan = e.cspan = 1;
    e.bw = -1; e.radius = -1; e.fsize = 0; e.bold = 0;
    e.bd = 0; e.fg = 0; e.bg = 0;
    e.gap = -1;
    e.tx = 0; e.ty = 0; e.sc = 100; e.rot = 0; e.op = 255;
    e.styled_gen = -1;
    e.live = nullptr; e.iv = nullptr; e.buf = nullptr; e.items = nullptr;
    e.caret = e.anchor = 0;
    // отступы по умолчанию (в точках интерфейса), чтобы общая раскладка давала вид
    switch (kind) {
        case K_BUTTON:   e.pl = e.pr = 16; e.pt = e.pb = 7; break;
        case K_TOOL:     e.pl = e.pr = 8;  e.pt = e.pb = 6; break;
        case K_EDIT: case K_PASSWORD: e.pl = e.pr = 8; e.pt = e.pb = 6; break;
        case K_MEMO:     e.pl = e.pr = 6;  e.pt = e.pb = 6; break;
        case K_COMBO:    e.pl = 10; e.pr = 28; e.pt = e.pb = 6; break;
        case K_NUMBER:   e.pl = 8;  e.pr = 30; e.pt = e.pb = 5; break;
        case K_LIST:     e.pl = e.pr = 1; e.pt = e.pb = 4; break;
        case K_GROUP:    e.pl = e.pr = 12; e.pt = 10; e.pb = 12; break;
        case K_TAB:      e.pl = e.pr = 14; e.pt = e.pb = 12; break;
        case K_MENU:     e.pl = e.pr = 12; e.pt = e.pb = 6; break;
        default: break;
    }
    resolve_default(e);
    return h;
}

void el_add_child(int parent, int child) {
    if (!valid(parent)) return;
    Element &p = g.el[parent];
    g.el[child].parent = parent;
    g.el[child].next = 0;
    if (p.last) g.el[p.last].next = child; else p.first = child;
    p.last = child;
}

static int make(int parent, int kind) {
    if (parent == 0) parent = g.window;
    int h = el_alloc(kind);
    el_add_child(parent, h);
    request_redraw();
    return h;
}

static void free_subtree(int h) {
    if (!valid(h)) return;
    int c = g.el[h].first;
    while (c) { int n = g.el[c].next; free_subtree(c); c = n; }
    Element &e = g.el[h];
    if (e.grid) { delete e.grid; e.grid = nullptr; }
    if (e.img) { delete e.img; e.img = nullptr; }
    if (e.icon_img) { delete e.icon_img; e.icon_img = nullptr; }
    e.alive = false;
    e.kind = K_FREE;
    e.first = e.last = e.next = e.parent = 0;
}

// ---------------------------------------------------------------- окно -----

el el_window(const char *title, int w, int h) {
    if (!g.inited) {
        g.el.clear();
        g.el.push_back(Element());          // индекс 0 — «нет»
        g.dpi = 1.0;
        g.def_font = 14;
        g.corner = 5;
        g.dark = 1;
        g.accent = 0x4c8bf5;
        g.win_opacity = 255;
        g.win_minw = g.win_minh = g.win_maxw = g.win_maxh = -1;
        g.aspect_w = g.aspect_h = 0;
        g.win_shape = 0; g.layered = false; g.frameless = false;
        g.resizable = true;
        theme_init();
        gdip_init();
        g.inited = true;
    }
    int wh = el_alloc(K_WINDOW);
    g.window = wh;
    Element &e = g.el[wh];
    e.text = title ? title : "";
    e.parent = 0;
    win_create(title, w, h);
    // перецепить осиротевшие элементы (сделанные с parent 0 до окна) — не нужно: до окна их нет
    return wh;
}

void el_title(const char *title) {
    if (valid(g.window)) g.el[g.window].text = title ? title : "";
    win_set_title(wide(title));
}

// ---------------------------------------------------------- панели/контролы

el el_stack(el parent, int horizontal) { int h = make(parent, K_STACK); g.el[h].horiz = horizontal != 0; return h; }
el el_dock(el parent) { return make(parent, K_DOCK); }
el el_wrap(el parent) { int h = make(parent, K_WRAP); g.el[h].wrap = true; return h; }
el el_canvas(el parent) { return make(parent, K_CANVAS); }
el el_scroll(el parent) { return make(parent, K_SCROLL); }
el el_border(el parent) { return make(parent, K_BORDER); }

el el_group(el parent, const char *header) { int h = make(parent, K_GROUP); g.el[h].text = header ? header : ""; return h; }

el el_grid(el parent, const char *cols, const char *rows) {
    int h = make(parent, K_GRID);
    GridDef *gd = new GridDef(); memset(gd, 0, sizeof(*gd));
    auto parse = [](const char *spec, u8 *types, int *vals) -> int {
        int n = 0; if (!spec || !*spec) { types[0] = 2; vals[0] = 100; return 1; }
        const char *p = spec;
        while (*p && n < GMAX) {
            while (*p == ' ' || *p == ',') p++;
            if (!*p) break;
            if (*p == '*' || ((p[0] >= '0' && p[0] <= '9') && strchr(p, '*') && (strchr(p, '*') < strchr(p, ',') || !strchr(p, ',')))) {
                // "*", "2*" — доля
            }
            // определить тип токена
            char tok[32]; int ti = 0;
            while (*p && *p != ',' && ti < 31) tok[ti++] = *p++;
            tok[ti] = 0;
            // убрать хвостовые пробелы
            while (ti > 0 && tok[ti-1] == ' ') tok[--ti] = 0;
            if (strstr(tok, "Auto") || strstr(tok, "auto") || tok[0] == 'A' || tok[0] == 'a') { types[n] = 0; vals[n] = 0; }
            else if (strchr(tok, '*')) { types[n] = 2; int wgt = atoi(tok); vals[n] = (wgt <= 0 ? 1 : wgt) * 100; }
            else { types[n] = 1; vals[n] = atoi(tok); }
            n++;
        }
        if (n == 0) { types[0] = 2; vals[0] = 100; n = 1; }
        return n;
    };
    gd->nc = parse(cols, gd->ct, gd->cv);
    gd->nr = parse(rows, gd->rt, gd->rv);
    g.el[h].grid = gd;
    return h;
}

el el_tabs(el parent) { int h = make(parent, K_TABS); g.el[h].ival = 0; return h; }
el el_tab(el tabs, const char *header) {
    if (!valid(tabs)) return 0;
    int h = el_alloc(K_TAB); g.el[h].text = header ? header : "";
    el_add_child(tabs, h); request_redraw(); return h;
}

el el_text(el parent, const char *text) { int h = make(parent, K_TEXT); g.el[h].text = text ? text : ""; return h; }
el el_button(el parent, const char *text) { int h = make(parent, K_BUTTON); g.el[h].text = text ? text : ""; return h; }
el el_check(el parent, const char *text, int *value) { int h = make(parent, K_CHECK); g.el[h].text = text ? text : ""; g.el[h].iv = value; return h; }
el el_radio(el parent, const char *text, int *group, int value) { int h = make(parent, K_RADIO); g.el[h].text = text ? text : ""; g.el[h].iv = group; g.el[h].ival = value; return h; }

el el_edit(el parent, char *buf, int cap) { int h = make(parent, K_EDIT); g.el[h].buf = buf; g.el[h].cap = cap; return h; }
el el_memo(el parent, char *buf, int cap) { int h = make(parent, K_MEMO); g.el[h].buf = buf; g.el[h].cap = cap; return h; }
el el_password(el parent, char *buf, int cap) { int h = make(parent, K_PASSWORD); g.el[h].buf = buf; g.el[h].cap = cap; return h; }

el el_combo(el parent, const char *const *items, int count, int *sel) { int h = make(parent, K_COMBO); g.el[h].items = items; g.el[h].count = count; g.el[h].iv = sel; return h; }
el el_listbox(el parent, const char *const *items, int count, int *sel) { int h = make(parent, K_LIST); g.el[h].items = items; g.el[h].count = count; g.el[h].iv = sel; return h; }
void el_items(el e, const char *const *items, int count) { if (!valid(e)) return; g.el[e].items = items; g.el[e].count = count; request_redraw(); }

el el_slider(el parent, int min, int max, int *value) { int h = make(parent, K_SLIDER); g.el[h].vmin = min; g.el[h].vmax = max; g.el[h].iv = value; return h; }
el el_progress(el parent, int max, int *value) { int h = make(parent, K_PROGRESS); g.el[h].vmin = 0; g.el[h].vmax = max; g.el[h].iv = value; return h; }
el el_separator(el parent) { return make(parent, K_SEP); }

el el_number(el parent, int min, int max, int step, int *value) {
    int h = make(parent, K_NUMBER); g.el[h].vmin = min; g.el[h].vmax = max; g.el[h].step = step <= 0 ? 1 : step; g.el[h].iv = value;
    if (value) { if (*value < min) *value = min; if (*value > max) *value = max; }
    return h;
}

el el_image(el parent, const char *path) {
    int h = make(parent, K_IMAGE);
    g.el[h].text = path ? path : "";
    if (path && *path) { Bitmap *b = new Bitmap(wide(path).c_str()); if (b->GetLastStatus() == Ok) g.el[h].img = b; else delete b; }
    return h;
}

el el_paint(el parent, void (*fn)(el, int, int, void *), void *arg) {
    int h = make(parent, K_PAINT); g.el[h].paint_fn = fn; g.el[h].paint_arg = arg; g.el[h].needs_redraw = true; return h;
}
void el_redraw(el e) { if (valid(e)) g.el[e].needs_redraw = true; request_redraw(); }

el el_toolbar(el parent) { int h = make(parent, K_TOOLBAR); g.el[h].horiz = true; g.el[h].flat = true; return h; }
el el_tool(el toolbar, const char *icon, const char *text, const char *tip) {
    int h = make(toolbar, K_TOOL); g.el[h].text = text ? text : ""; g.el[h].tip = tip ? tip : "";
    g.el[h].flat = true;
    if (icon && *icon) { Bitmap *b = new Bitmap(wide(icon).c_str()); if (b->GetLastStatus() == Ok) g.el[h].icon_img = b; else delete b; g.el[h].icon_size = 20; }
    return h;
}
void el_icon(el button, const char *path, int size) {
    if (!valid(button)) return;
    if (g.el[button].icon_img) { delete g.el[button].icon_img; g.el[button].icon_img = nullptr; }
    if (path && *path) { Bitmap *b = new Bitmap(wide(path).c_str()); if (b->GetLastStatus() == Ok) g.el[button].icon_img = b; else delete b; }
    g.el[button].icon_size = size <= 0 ? 20 : size;
    request_redraw();
}

// ---------------------------------------------------------- свойства -------

void el_size(el e, int w, int h) { if (!valid(e)) return; g.el[e].w = w; g.el[e].h = h; request_redraw(); }
void el_min_size(el e, int w, int h) { if (!valid(e)) return; g.el[e].minw = w; g.el[e].minh = h; request_redraw(); }
void el_max_size(el e, int w, int h) { if (!valid(e)) return; g.el[e].maxw = w; g.el[e].maxh = h; request_redraw(); }
void el_margin(el e, int l, int t, int r, int b) { if (!valid(e)) return; Element &x = g.el[e]; x.ml = l; x.mt = t; x.mr = r; x.mb = b; request_redraw(); }
void el_padding(el e, int l, int t, int r, int b) { if (!valid(e)) return; Element &x = g.el[e]; x.pl = l; x.pt = t; x.pr = r; x.pb = b; request_redraw(); }
void el_align(el e, int h, int v) { if (!valid(e)) return; g.el[e].ha = h; g.el[e].va = v; request_redraw(); }
void el_cell(el e, int row, int col, int rowspan, int colspan) { if (!valid(e)) return; Element &x = g.el[e]; x.row = row; x.col = col; x.rspan = rowspan < 1 ? 1 : rowspan; x.cspan = colspan < 1 ? 1 : colspan; request_redraw(); }
void el_dock_side(el e, int side) { if (!valid(e)) return; g.el[e].dock = side; request_redraw(); }
void el_pos(el e, int x, int y) { if (!valid(e)) return; g.el[e].px = x; g.el[e].py = y; request_redraw(); }
void el_gap(el e, int p) { if (!valid(e)) return; g.el[e].gap = p; request_redraw(); }

void el_set_text(el e, const char *text) { if (!valid(e)) return; g.el[e].text = text ? text : ""; if (g.el[e].kind == K_IMAGE) { if (g.el[e].img) { delete g.el[e].img; g.el[e].img = nullptr; } if (text && *text) { Bitmap *b = new Bitmap(wide(text).c_str()); if (b->GetLastStatus() == Ok) g.el[e].img = b; else delete b; } } request_redraw(); }
const char *el_get_text(el e) { return valid(e) ? g.el[e].text.c_str() : ""; }
void el_text_bind(el e, const char *live) { if (!valid(e)) return; g.el[e].live = live; request_redraw(); }
void el_enable(el e, int on) { if (!valid(e)) return; g.el[e].disabled = !on; request_redraw(); }
void el_show(el e, int on) { if (!valid(e)) return; g.el[e].hidden = !on; request_redraw(); }
void el_tooltip(el e, const char *text) { if (!valid(e)) return; g.el[e].tip = text ? text : ""; }
void el_wrap_text(el e, int on) { if (!valid(e)) return; g.el[e].wrap = on != 0; request_redraw(); }
void el_font(el e, int size, int bold) { if (!valid(e)) return; g.el[e].fsize = size; g.el[e].bold = bold; request_redraw(); }
void el_colors(el e, int fg, int bg) { if (!valid(e)) return; Element &x = g.el[e]; if (fg >= 0) { x.fg = fg; x.has_fg = true; } if (bg >= 0) { x.bg = bg; x.has_bg = true; } request_redraw(); }
void el_frame_style(el e, int color, int width, int radius) { if (!valid(e)) return; Element &x = g.el[e]; if (color >= 0) { x.bd = color; x.has_bd = true; } x.bw = width; x.radius = radius; request_redraw(); }

void el_name(el e, const char *name) { if (!valid(e)) return; g.el[e].name = name ? name : ""; }
el el_find(const char *name) {
    if (!name) return 0;
    for (int i = 1; i < (int)g.el.size(); i++) if (g.el[i].alive && g.el[i].name == name) return i;
    return 0;
}
void el_class(el e, const char *classes) { if (!valid(e)) return; g.el[e].cls = classes ? classes : ""; g.el[e].styled_gen = -1; restyle_all(); request_redraw(); }

void el_select_tab(el tabs, int index) { if (!valid(tabs)) return; g.el[tabs].ival = index; request_redraw(); }
int  el_selected_tab(el tabs) { return valid(tabs) ? g.el[tabs].ival : 0; }

void el_scroll_to(el e, int pos) { if (!valid(e)) return; g.el[e].scroll = pos; request_redraw(); }
void el_focus(el e) { set_focus(valid(e) ? e : 0); request_redraw(); }
el el_focused(void) { return g.focus; }

void el_clear(el e) {
    if (!valid(e)) return;
    int c = g.el[e].first;
    while (c) { int n = g.el[c].next; free_subtree(c); c = n; }
    g.el[e].first = g.el[e].last = 0;
    request_redraw();
}
void el_remove(el e) {
    if (!valid(e) || e == g.window) return;
    int p = g.el[e].parent;
    if (valid(p)) {
        int prev = 0, c = g.el[p].first;
        while (c && c != e) { prev = c; c = g.el[c].next; }
        if (c == e) { if (prev) g.el[prev].next = g.el[e].next; else g.el[p].first = g.el[e].next; if (g.el[p].last == e) g.el[p].last = prev; }
    }
    free_subtree(e);
    request_redraw();
}

void el_data(el e, void *p) { if (valid(e)) g.el[e].data = p; }
void *el_get_data(el e) { return valid(e) ? g.el[e].data : nullptr; }

// ---------------------------------------------------------- события --------

void el_on_click(el e, el_fn fn, void *arg) { if (!valid(e)) return; g.el[e].on_click = fn; g.el[e].click_arg = arg; }
void el_on_change(el e, el_fn fn, void *arg) { if (!valid(e)) return; g.el[e].on_change = fn; g.el[e].change_arg = arg; }
int  el_clicked(el e) { return valid(e) && g.el[e].fl_click; }
int  el_changed(el e) { return valid(e) && g.el[e].fl_change; }
void fire_click(int h) { if (!valid(h)) return; g.el[h].fl_click = true; if (g.el[h].on_click) g.el[h].on_click(h, g.el[h].click_arg); request_redraw(); }
void fire_change(int h) { if (!valid(h)) return; g.el[h].fl_change = true; if (g.el[h].on_change) g.el[h].on_change(h, g.el[h].change_arg); request_redraw(); }

void el_on_close(el_fn fn, void *arg) { g.on_close = fn; g.on_close_arg = arg; g.close_guard = (fn != nullptr); }
void el_quit(void) { g.quit = true; }

// ---------------------------------------------------------- главный цикл ---

static void clear_frame_flags() {
    for (int i = 1; i < (int)g.el.size(); i++) if (g.el[i].alive) { g.el[i].fl_click = false; g.el[i].fl_change = false; }
}

int el_frame(void) {
    if (!g.inited || g.quit) return 0;
    win_pump();
    if (g.quit) return 0;
    if (g.close_req) {
        g.close_req = false;
        if (g.close_guard && g.on_close) g.on_close(0, g.on_close_arg);
        else return 0;
    }

    // время
    LARGE_INTEGER freq, now; QueryPerformanceFrequency(&freq); QueryPerformanceCounter(&now);
    g.last_ms = g.now_ms;
    g.now_ms = (double)now.QuadPart * 1000.0 / (double)freq.QuadPart;
    if (g.last_ms == 0) g.last_ms = g.now_ms;

    clear_frame_flags();

    // раскладка
    layout_root();

    // ввод: оверлеи/меню/списки перехватывают
    if (g.msg_kind) overlay_frame_input();
    else if (g.combo_open) combo_frame_input();
    else if (!g.popups.empty()) menu_frame_input();
    else input_frame();                 // при g.modal ввод ограничен поддеревом окна-диалога

    // анимации/таймеры
    anim_frame();

    // раскладка ещё раз — обработчики могли поменять дерево/значения
    layout_root();

    // рисование
    draw_tree();
    if (g.combo_open) combo_draw();
    if (!g.popups.empty()) menu_draw();
    if (g.msg_kind || g.modal || g.toast_until > g.now_ms) overlay_draw();
    draw_tooltip();
    apply_effects();                    // постобработка кадра (дисперсия, глитч, свечение…)
    present();

    // сброс колеса за кадр
    g.wheel = 0; g.wheel_consumed_by = 0;
    g.pbuttons = g.buttons;
    g.pmx = g.mx; g.pmy = g.my;
    g.keys.clear();

    // экономия ЦП: спим тем дольше, чем меньше происходит
    if (anim_busy() || effects_animated()) win_wait(12);
    else if (g.dirty) win_wait(30);
    else win_wait(1000);
    g.dirty = false;
    return g.quit ? 0 : 1;
}

void el_run(void) { while (el_frame()) {} }

void request_redraw() { g.dirty = true; }

} // namespace eli
