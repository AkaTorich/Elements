// Оформление строкой (как CSS): el_style для одного элемента и таблица стилей
// el_stylesheet для тегов/классов/имён. Свои настройки элемента сильнее таблицы.
#include "el_internal.h"
#include <string>
#include <vector>
#include <cstring>
#include <cstdio>

namespace eli {

static std::string trim(const std::string &s) {
    size_t a = 0, b = s.size();
    while (a < b && (unsigned char)s[a] <= ' ') a++;
    while (b > a && (unsigned char)s[b - 1] <= ' ') b--;
    return s.substr(a, b - a);
}
static std::vector<std::string> tokens(const std::string &s) {
    std::vector<std::string> out; std::string cur;
    for (char c : s) { if (c == ' ' || c == '\t') { if (!cur.empty()) { out.push_back(cur); cur.clear(); } } else cur += c; }
    if (!cur.empty()) out.push_back(cur);
    return out;
}
static bool parse_color(const std::string &t, u32 *out) {
    std::string s = t; if (!s.empty() && s[0] == '#') s = s.substr(1);
    if (s.size() == 3) { int r = (int)strtol(s.substr(0, 1).c_str(), 0, 16); int gg = (int)strtol(s.substr(1, 1).c_str(), 0, 16); int b = (int)strtol(s.substr(2, 1).c_str(), 0, 16); *out = ((r * 17) << 16) | ((gg * 17) << 8) | (b * 17); return true; }
    if (s.size() == 6) { *out = (u32)strtol(s.c_str(), 0, 16); return true; }
    return false;
}

void apply_style_string(int h, const char *css) {
    if (!valid(h) || !css) return;
    Element &e = g.el[h];
    std::string s = css;
    size_t i = 0;
    while (i < s.size()) {
        size_t semi = s.find(';', i);
        std::string decl = s.substr(i, semi == std::string::npos ? std::string::npos : semi - i);
        i = (semi == std::string::npos) ? s.size() : semi + 1;
        size_t col = decl.find(':');
        if (col == std::string::npos) continue;
        std::string key = trim(decl.substr(0, col));
        std::string val = trim(decl.substr(col + 1));
        std::vector<std::string> tk = tokens(val);
        u32 c;
        if (key == "background" && !tk.empty()) { if (parse_color(tk[0], &c)) { e.bg = c; e.has_bg = true; e.grad = 0; } }
        else if (key == "color" && !tk.empty()) { if (parse_color(tk[0], &c)) { e.fg = c; e.has_fg = true; } }
        else if (key == "border" && !tk.empty()) { if (parse_color(tk[0], &c)) { e.bd = c; e.has_bd = true; if (e.bw < 0) e.bw = 1; } }
        else if (key == "border-width" && !tk.empty()) e.bw = atoi(tk[0].c_str());
        else if (key == "radius") e.radius = tk.empty() ? 0 : atoi(tk[0].c_str());
        else if (key == "width") e.w = tk.empty() ? -1 : atoi(tk[0].c_str());
        else if (key == "height") e.h = tk.empty() ? -1 : atoi(tk[0].c_str());
        else if (key == "padding") {
            int n = (int)tk.size();
            if (n == 1) { int v = atoi(tk[0].c_str()); e.pl = e.pt = e.pr = e.pb = v; }
            else if (n == 2) { int v = atoi(tk[0].c_str()), hh = atoi(tk[1].c_str()); e.pt = e.pb = v; e.pl = e.pr = hh; }
            else if (n == 3) { e.pt = atoi(tk[0].c_str()); e.pl = e.pr = atoi(tk[1].c_str()); e.pb = atoi(tk[2].c_str()); }
            else if (n >= 4) { e.pt = atoi(tk[0].c_str()); e.pr = atoi(tk[1].c_str()); e.pb = atoi(tk[2].c_str()); e.pl = atoi(tk[3].c_str()); }
        }
        else if (key == "margin") {
            int n = (int)tk.size();
            if (n == 1) { int v = atoi(tk[0].c_str()); e.ml = e.mt = e.mr = e.mb = v; }
            else if (n == 2) { int v = atoi(tk[0].c_str()), hh = atoi(tk[1].c_str()); e.mt = e.mb = v; e.ml = e.mr = hh; }
            else if (n == 3) { e.mt = atoi(tk[0].c_str()); e.ml = e.mr = atoi(tk[1].c_str()); e.mb = atoi(tk[2].c_str()); }
            else if (n >= 4) { e.mt = atoi(tk[0].c_str()); e.mr = atoi(tk[1].c_str()); e.mb = atoi(tk[2].c_str()); e.ml = atoi(tk[3].c_str()); }
        }
        else if (key == "gradient" && tk.size() >= 2) {
            u32 a, b; if (parse_color(tk[0], &a) && parse_color(tk[1], &b)) { e.bg = a; e.bg2 = b; e.has_bg = true; e.grad = 1;
                if (tk.size() >= 3) { if (tk[2] == "horizontal") e.grad = 2; else if (tk[2] == "diagonal") e.grad = 3; else e.grad = 1; } }
        }
        else if (key == "background-alpha") e.bg_a = tk.empty() ? 0 : atoi(tk[0].c_str());
        else if (key == "shadow") {
            if (!tk.empty() && tk[0] == "none") { e.sh_a = 0; }
            else if (tk.size() >= 1 && parse_color(tk[0], &c)) { e.sh_col = c; e.sh_blur = tk.size() > 1 ? atoi(tk[1].c_str()) : 12; e.sh_dx = tk.size() > 2 ? atoi(tk[2].c_str()) : 0; e.sh_dy = tk.size() > 3 ? atoi(tk[3].c_str()) : 4; e.sh_a = tk.size() > 4 ? atoi(tk[4].c_str()) : 120; }
        }
        else if (key == "hover-background" && !tk.empty()) { if (parse_color(tk[0], &c)) { e.hbg = c; e.has_hbg = true; } }
        else if (key == "press-background" && !tk.empty()) { if (parse_color(tk[0], &c)) { e.pbg = c; e.has_pbg = true; } }
        else if (key == "hover-color" && !tk.empty()) { if (parse_color(tk[0], &c)) { e.hfg = c; e.has_hfg = true; } }
        else if (key == "hover-border" && !tk.empty()) { if (parse_color(tk[0], &c)) { e.hbd = c; e.has_hbd = true; } }
        else if (key == "font-size") e.fsize = tk.empty() ? 0 : atoi(tk[0].c_str());
        else if (key == "font-weight") e.bold = (!tk.empty() && (tk[0] == "bold" || atoi(tk[0].c_str()) >= 600)) ? 1 : 0;
        else if (key == "font-family") e.mono = tk.empty() ? 0 : font_face_by_name(tk[0].c_str());
        else if (key == "text-align") { e.talign = tk.empty() ? 0 : (tk[0] == "center" ? 1 : (tk[0] == "right" ? 2 : 0)); }
    }
    request_redraw();
}

void el_style(el e, const char *css) {
    if (!valid(e) || !css) return;
    g.el[e].inl = css;
    apply_style_string(e, css);
}

// ----------------------------------------------------------- таблица -------

void el_stylesheet(const char *css) {
    if (!css) return;
    std::string s = css;
    size_t i = 0;
    while (i < s.size()) {
        size_t open = s.find('{', i);
        if (open == std::string::npos) break;
        size_t close = s.find('}', open);
        if (close == std::string::npos) break;
        std::string sel = trim(s.substr(i, open - i));
        std::string body = trim(s.substr(open + 1, close - open - 1));
        i = close + 1;
        // убрать комментарии /* */ грубо
        // несколько селекторов через запятую
        size_t p = 0;
        while (p < sel.size()) {
            size_t comma = sel.find(',', p);
            std::string one = trim(sel.substr(p, comma == std::string::npos ? std::string::npos : comma - p));
            p = (comma == std::string::npos) ? sel.size() : comma + 1;
            if (one.empty()) continue;
            int state = 0;
            size_t h = one.find(":hover"); size_t pr = one.find(":pressed");
            if (h != std::string::npos) { state = 1; one = one.substr(0, h); }
            else if (pr != std::string::npos) { state = 2; one = one.substr(0, pr); }
            Rule r; r.sel = trim(one); r.state = state; r.css = body;
            g.rules.push_back(r);
        }
    }
    g.rules_gen++;
    restyle_all();
}

int el_stylesheet_file(const char *path) {
    if (!path) return 0;
    FILE *f = _wfopen(wide(path).c_str(), L"rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    std::string buf(n, 0); fread(&buf[0], 1, n, f); fclose(f);
    el_stylesheet(buf.c_str());
    return 1;
}

static const char *kind_tag(int k) {
    switch (k) {
    case K_WINDOW: return "window"; case K_STACK: return "stack"; case K_DOCK: return "dock"; case K_WRAP: return "wrap";
    case K_GRID: return "grid"; case K_SCROLL: return "scroll"; case K_CANVAS: return "canvas"; case K_BORDER: return "border";
    case K_GROUP: return "group"; case K_TABS: return "tabs"; case K_TAB: return "tab"; case K_TEXT: return "text";
    case K_BUTTON: return "button"; case K_CHECK: return "check"; case K_RADIO: return "radio"; case K_EDIT: return "edit";
    case K_MEMO: return "memo"; case K_PASSWORD: return "password"; case K_COMBO: return "combo"; case K_LIST: return "list";
    case K_SLIDER: return "slider"; case K_PROGRESS: return "progress"; case K_IMAGE: return "image"; case K_NUMBER: return "number";
    case K_TOOLBAR: return "toolbar"; case K_TOOL: return "tool"; case K_PAINT: return "paint"; case K_DIALOG: return "dialog";
    default: return "";
    }
}
static bool has_class(const Element &e, const std::string &cls) {
    std::string s = " " + e.cls + " ";
    return s.find(" " + cls + " ") != std::string::npos;
}
static bool sel_match(const Element &e, const std::string &sel) {
    // tag  .class  #name  tag.class
    std::string tag, cls, id;
    size_t i = 0;
    while (i < sel.size()) {
        if (sel[i] == '.') { size_t j = i + 1; std::string c; while (j < sel.size() && sel[j] != '.' && sel[j] != '#') c += sel[j++]; cls = c; i = j; }
        else if (sel[i] == '#') { size_t j = i + 1; std::string c; while (j < sel.size() && sel[j] != '.' && sel[j] != '#') c += sel[j++]; id = c; i = j; }
        else { std::string t; while (i < sel.size() && sel[i] != '.' && sel[i] != '#') t += sel[i++]; tag = t; }
    }
    if (!tag.empty() && tag != "*" && tag != kind_tag(e.kind)) return false;
    if (!cls.empty() && !has_class(e, cls)) return false;
    if (!id.empty() && e.name != id) return false;
    return true;
}

void style_apply_sheet(int root) {
    (void)root;
    if (g.rules.empty()) return;
    for (int i = 1; i < (int)g.el.size(); i++) {
        Element &e = g.el[i];
        if (!e.alive) continue;
        if (e.styled_gen == g.rules_gen) continue;
        for (const Rule &r : g.rules) {
            if (r.state != 0) continue;              // :hover/:pressed — пока не применяем из таблицы
            if (sel_match(e, r.sel)) apply_style_string(i, r.css.c_str());
        }
        if (!e.inl.empty()) apply_style_string(i, e.inl.c_str());  // свой стиль поверх
        e.styled_gen = g.rules_gen;
    }
}

void restyle_all() {
    for (int i = 1; i < (int)g.el.size(); i++) if (g.el[i].alive) g.el[i].styled_gen = -1;
    request_redraw();
}

} // namespace eli
