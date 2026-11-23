// Ввод и взаимодействие Elements: попадание мышью, наведение, щелчки,
// текстовый редактор (edit/memo/password), выпадающий список, подсказки.
#include "el_internal.h"
#include <algorithm>
#include <cmath>
#include <cstring>

using namespace Gdiplus;

namespace eli {

static inline int P(int d) { return px(d); }
static int eff_font(const Element &e) { return e.fsize > 0 ? e.fsize : g.def_font; }
static void draw_text_in_tip(Gdiplus::Graphics *gr, int x, int y, const std::wstring &s, u32 color, int size);

bool is_container(int kind) {
    return kind == K_WINDOW || kind == K_STACK || kind == K_DOCK || kind == K_WRAP || kind == K_GRID ||
           kind == K_SCROLL || kind == K_CANVAS || kind == K_BORDER || kind == K_GROUP || kind == K_TABS ||
           kind == K_TAB || kind == K_TOOLBAR;
}
bool is_focusable(int kind) { return kind == K_EDIT || kind == K_MEMO || kind == K_PASSWORD; }

static bool hover_kind(int k) {
    return k == K_BUTTON || k == K_TOOL || k == K_CHECK || k == K_RADIO || k == K_LIST || k == K_TAB ||
           k == K_MENU || k == K_COMBO || k == K_NUMBER || k == K_SLIDER;
}

void set_focus(int h) {
    if (g.focus == h) return;
    g.focus = h;
    if (valid(h) && is_focusable(g.el[h].kind)) {
        // курсор в конец при фокусе щелчком устанавливается отдельно
    }
    request_redraw();
}

// --- кто получает действие по щелчку (вверх по дереву) ---
static int actionable(int h) {
    while (valid(h)) {
        Element &e = g.el[h];
        if (hover_kind(e.kind) || is_focusable(e.kind) || e.kind == K_IMAGE || e.kind == K_PAINT || e.on_click) return h;
        if (h == g.window) break;
        h = e.parent;
    }
    return 0;
}

// --- попадание ---
static int hit_rec(int h, int x, int y) {
    Element &e = g.el[h];
    if (!e.alive || e.hidden) return 0;
    bool inside = (h == g.window) || (x >= e.ax && x < e.ax + e.aw && y >= e.ay && y < e.ay + e.ah);
    if (!inside) return 0;
    if (e.kind == K_TABS) {
        int c = e.first;
        while (c) { if (g.el[c].kind == K_TAB) { Element &tb = g.el[c]; if (x >= tb.hdr_x && x < tb.hdr_x + tb.hdr_w && y >= e.ay && y < e.ay + e.strip_h) return c; } c = g.el[c].next; }
        int idx = 0, sel = e.ival, found = 0; c = e.first;
        while (c) { if (g.el[c].kind == K_TAB) { if (idx == sel) { int r = hit_rec(c, x, y); if (r) found = r; } idx++; } c = g.el[c].next; }
        return found ? found : h;
    }
    if (e.kind == K_MENUBAR) {
        int c = e.first;
        while (c) { if (g.el[c].kind == K_MENU) { Element &mi = g.el[c]; if (x >= mi.ax && x < mi.ax + mi.aw && y >= mi.ay && y < mi.ay + mi.ah) return c; } c = g.el[c].next; }
        return h;
    }
    int found = 0, c = e.first;
    while (c) { if (g.el[c].alive && g.el[c].kind != K_MENU && g.el[c].kind != K_TAB) { int r = hit_rec(c, x, y); if (r) found = r; } c = g.el[c].next; }
    return found ? found : h;
}
int hit_test(int root, int x, int y) { return hit_rec(root, x, y); }

// --- текст: строки ---
static void split_lines(const std::wstring &w, std::vector<std::wstring> &out, std::vector<int> &starts) {
    out.clear(); starts.clear();
    size_t i = 0; starts.push_back(0);
    std::wstring cur;
    for (; i < w.size(); i++) { if (w[i] == L'\n') { out.push_back(cur); cur.clear(); starts.push_back((int)i + 1); } else cur += w[i]; }
    out.push_back(cur);
}

int caret_from_x(int h, int mx, int my) {
    Element &e = g.el[h];
    std::wstring w = e.buf ? wide(e.buf) : std::wstring();
    int fs = eff_font(e), mono = e.mono;
    if (e.kind == K_MEMO) {
        std::vector<std::wstring> lines; std::vector<int> starts; split_lines(w, lines, starts);
        int lh; { int tw; text_size(L"Ag", fs, 0, mono, false, 0, &tw, &lh); }
        int top = e.ay + P(e.pt) - e.scroll;
        int li = (my - top) / std::max(1, lh);
        if (li < 0) li = 0; if (li >= (int)lines.size()) li = (int)lines.size() - 1;
        int x0 = e.ax + P(e.pl);
        const std::wstring &ln = lines[li];
        int col = (int)ln.size();
        for (int i = 0; i <= (int)ln.size(); i++) { int wpx = text_w(ln.substr(0, i), fs, 0, mono); if (x0 + wpx >= mx) { col = i; break; } }
        return starts[li] + col;
    } else {
        std::wstring disp = (e.kind == K_PASSWORD) ? std::wstring(w.size(), L'\x2022') : w;
        int x0 = e.ax + P(e.pl) - e.hscroll;
        for (int i = 0; i <= (int)disp.size(); i++) { int wpx = text_w(disp.substr(0, i), fs, 0, mono); if (x0 + wpx >= mx) return i; }
        return (int)disp.size();
    }
}

// --- буфер обмена ---
static std::wstring clip_get() {
    std::wstring r;
    if (!OpenClipboard(g.hwnd)) return r;
    HANDLE h = GetClipboardData(CF_UNICODETEXT);
    if (h) { wchar_t *p = (wchar_t *)GlobalLock(h); if (p) { r = p; GlobalUnlock(h); } }
    CloseClipboard();
    return r;
}
static void clip_set(const std::wstring &s) {
    if (!OpenClipboard(g.hwnd)) return;
    EmptyClipboard();
    size_t bytes = (s.size() + 1) * sizeof(wchar_t);
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (h) { void *p = GlobalLock(h); memcpy(p, s.c_str(), bytes); GlobalUnlock(h); SetClipboardData(CF_UNICODETEXT, h); }
    CloseClipboard();
}

static void buf_set(Element &e, const std::wstring &w) {
    std::string u = narrow(w);
    if ((int)u.size() >= e.cap) u.resize(e.cap - 1);
    // не рвать UTF-8 на конце
    while (!u.empty() && (u.back() & 0xC0) == 0x80) u.pop_back();
    memcpy(e.buf, u.c_str(), u.size()); e.buf[u.size()] = 0;
}

// --- редактирование клавишами ---
void edit_input(int h, const std::vector<KeyEv> &keys) {
    Element &e = g.el[h];
    std::wstring w = e.buf ? wide(e.buf) : std::wstring();
    int len = (int)w.size();
    if (e.caret > len) e.caret = len; if (e.anchor > len) e.anchor = len;
    bool changed = false;
    auto has_sel = [&]() { return e.caret != e.anchor; };
    auto sel_lo = [&]() { return std::min(e.caret, e.anchor); };
    auto sel_hi = [&]() { return std::max(e.caret, e.anchor); };
    auto del_sel = [&]() { int a = sel_lo(), b = sel_hi(); w.erase(a, b - a); e.caret = e.anchor = a; };

    for (const KeyEv &k : keys) {
        bool shift = (k.mods & EL_SHIFT) != 0, ctrl = (k.mods & EL_CTRL) != 0;
        if (k.vk) {
            switch (k.vk) {
            case VK_LEFT: if (has_sel() && !shift) e.caret = sel_lo(); else if (e.caret > 0) e.caret--; if (!shift) e.anchor = e.caret; break;
            case VK_RIGHT: if (has_sel() && !shift) e.caret = sel_hi(); else if (e.caret < (int)w.size()) e.caret++; if (!shift) e.anchor = e.caret; break;
            case VK_HOME: { int ls = e.caret; while (ls > 0 && w[ls - 1] != L'\n') ls--; e.caret = ls; if (!shift) e.anchor = e.caret; break; }
            case VK_END: { int le = e.caret; while (le < (int)w.size() && w[le] != L'\n') le++; e.caret = le; if (!shift) e.anchor = e.caret; break; }
            case VK_UP: case VK_DOWN: if (e.kind == K_MEMO) { int line_s = e.caret; while (line_s > 0 && w[line_s - 1] != L'\n') line_s--; int col = e.caret - line_s;
                if (k.vk == VK_UP) { if (line_s > 0) { int ps = line_s - 1; while (ps > 0 && w[ps - 1] != L'\n') ps--; int pe = line_s - 1; e.caret = std::min(ps + col, pe); } }
                else { int ne = e.caret; while (ne < (int)w.size() && w[ne] != L'\n') ne++; if (ne < (int)w.size()) { int ns = ne + 1; int nend = ns; while (nend < (int)w.size() && w[nend] != L'\n') nend++; e.caret = std::min(ns + col, nend); } }
                if (!shift) e.anchor = e.caret; } break;
            case VK_BACK: if (has_sel()) { del_sel(); changed = true; } else if (e.caret > 0) { w.erase(e.caret - 1, 1); e.caret--; e.anchor = e.caret; changed = true; } break;
            case VK_DELETE: if (has_sel()) { del_sel(); changed = true; } else if (e.caret < (int)w.size()) { w.erase(e.caret, 1); changed = true; } break;
            case VK_RETURN: if (e.kind == K_MEMO) { if (has_sel()) del_sel(); w.insert(e.caret, 1, L'\n'); e.caret++; e.anchor = e.caret; changed = true; } else { buf_set(e, w); fire_click(h); fire_change(h); return; } break;
            case 'A': if (ctrl) { e.anchor = 0; e.caret = (int)w.size(); } break;
            case 'C': if (ctrl && has_sel()) clip_set(w.substr(sel_lo(), sel_hi() - sel_lo())); break;
            case 'X': if (ctrl && has_sel()) { clip_set(w.substr(sel_lo(), sel_hi() - sel_lo())); del_sel(); changed = true; } break;
            case 'V': if (ctrl) { if (has_sel()) del_sel(); std::wstring c = clip_get(); if (e.kind != K_MEMO) { size_t nl = c.find(L'\n'); if (nl != std::wstring::npos) c = c.substr(0, nl); } w.insert(e.caret, c); e.caret += (int)c.size(); e.anchor = e.caret; changed = true; } break;
            case 'Z': if (ctrl) { /* отмена — упрощённо не ведём историю */ } break;
            case VK_ESCAPE: set_focus(0); return;
            default: break;
            }
        } else if (k.ch >= 32 && k.ch != 127) {
            if (has_sel()) del_sel();
            w.insert(e.caret, 1, k.ch); e.caret++; e.anchor = e.caret; changed = true;
        }
    }
    if (e.caret < 0) e.caret = 0; if (e.caret > (int)w.size()) e.caret = (int)w.size();
    buf_set(e, w);
    if (changed) fire_change(h);
    request_redraw();
}

// --- рисование поля ---
void edit_draw(int h) {
    Element &e = g.el[h];
    Graphics *gr = g.g;
    int fs = eff_font(e), mono = e.mono;
    RectF r((REAL)e.ax, (REAL)e.ay, (REAL)e.aw, (REAL)e.ah);
    u32 bg = e.has_bg ? e.bg : th(11);
    GraphicsPath p; round_rect_path(p, r, (REAL)((e.radius >= 0) ? P(e.radius) : P(g.corner)));
    SolidBrush bb(gc(bg)); gr->FillPath(&bb, &p);
    bool focused = (g.focus == h);
    Pen pen(gc(focused ? th(9) : th(5)), focused ? 1.6f : 1.0f); gr->DrawPath(&pen, &p);

    std::wstring w = e.buf ? wide(e.buf) : std::wstring();
    Font *f = get_font(fs, e.bold, mono);
    SolidBrush tb(gc(e.has_fg ? e.fg : th(6)));
    SolidBrush selb(gc(th(12)));
    Region old; gr->GetClip(&old); gr->SetClip(Rect(e.ax + P(2), e.ay, e.aw - P(4), e.ah));
    bool caret_on = focused && ((int)(g.now_ms / 530) % 2 == 0);
    int selLo = std::min(e.caret, e.anchor), selHi = std::max(e.caret, e.anchor);

    if (e.kind == K_MEMO) {
        std::vector<std::wstring> lines; std::vector<int> starts; split_lines(w, lines, starts);
        int lh; { int tw; text_size(L"Ag", fs, 0, mono, false, 0, &tw, &lh); }
        int y = e.ay + P(e.pt) - e.scroll;
        int x0 = e.ax + P(e.pl);
        // автопрокрутка к курсору
        int cl = 0; for (int i = 0; i < (int)lines.size(); i++) if (e.caret >= starts[i]) cl = i;
        int caretY = cl * lh;
        int viewH = e.ah - P(e.pt) - P(e.pb);
        if (caretY - e.scroll < 0) e.scroll = caretY;
        if (caretY + lh - e.scroll > viewH) e.scroll = caretY + lh - viewH;
        y = e.ay + P(e.pt) - e.scroll;
        StringFormat sf(StringFormat::GenericTypographic()); sf.SetFormatFlags(sf.GetFormatFlags() | StringFormatFlagsMeasureTrailingSpaces | StringFormatFlagsNoWrap);
        for (int i = 0; i < (int)lines.size(); i++) {
            int ls = starts[i], le = ls + (int)lines[i].size();
            if (y + lh > e.ay && y < e.ay + e.ah) {
                if (selHi > selLo && selHi > ls && selLo < le + 1) {
                    int a = std::max(selLo, ls) - ls, b = std::min(selHi, le) - ls;
                    if (b >= a) { int xa = x0 + text_w(lines[i].substr(0, a), fs, 0, mono); int xb = x0 + text_w(lines[i].substr(0, b), fs, 0, mono); if (selHi > le) xb += P(5); gr->FillRectangle(&selb, xa, y, std::max(2, xb - xa), lh); }
                }
                gr->DrawString(lines[i].c_str(), -1, f, PointF((REAL)x0, (REAL)y), &sf, &tb);
                if (caret_on && e.caret >= ls && e.caret <= le) { int cx = x0 + text_w(lines[i].substr(0, e.caret - ls), fs, 0, mono); Pen cp(gc(th(7)), 1.0f); gr->DrawLine(&cp, cx, y + 1, cx, y + lh - 1); }
            }
            y += lh;
        }
    } else {
        std::wstring disp = (e.kind == K_PASSWORD) ? std::wstring(w.size(), L'\x2022') : w;
        int tw, thh; text_size(disp, fs, e.bold, mono, false, 0, &tw, &thh);
        int fieldw = e.aw - P(e.pl) - P(e.pr);
        int caretx = text_w(disp.substr(0, std::min((int)disp.size(), e.caret)), fs, e.bold, mono);
        if (caretx - e.hscroll > fieldw) e.hscroll = caretx - fieldw;
        if (caretx - e.hscroll < 0) e.hscroll = caretx;
        if (e.hscroll < 0) e.hscroll = 0;
        int x0 = e.ax + P(e.pl) - e.hscroll;
        int y = e.ay + (e.ah - thh) / 2;
        StringFormat sf(StringFormat::GenericTypographic()); sf.SetFormatFlags(sf.GetFormatFlags() | StringFormatFlagsMeasureTrailingSpaces | StringFormatFlagsNoWrap);
        if (selHi > selLo) { int xa = x0 + text_w(disp.substr(0, selLo), fs, e.bold, mono); int xb = x0 + text_w(disp.substr(0, selHi), fs, e.bold, mono); gr->FillRectangle(&selb, xa, e.ay + P(3), std::max(2, xb - xa), e.ah - P(6)); }
        gr->DrawString(disp.c_str(), -1, f, PointF((REAL)x0, (REAL)y), &sf, &tb);
        if (caret_on) { Pen cp(gc(th(7)), 1.0f); gr->DrawLine(&cp, x0 + caretx, e.ay + P(4), x0 + caretx, e.ay + e.ah - P(4)); }
    }
    gr->SetClip(&old);
    if (focused) request_redraw();   // мигание курсора
}

// --- выпадающий список combo ---
static void combo_rect(int h, int *x, int *y, int *w, int *hh, int *rowh) {
    Element &e = g.el[h];
    int rh; { int tw; text_size(L"Ag", eff_font(e), 0, 0, false, 0, &tw, &rh); } rh += P(8);
    int n = e.count; int maxv = 10;
    int vis = std::min(n, maxv);
    *x = e.ax; *w = e.aw; *rowh = rh;
    int below = g.el[g.window].ay + g.el[g.window].ah - (e.ay + e.ah);
    int ht = vis * rh + P(8);
    if (ht <= below) *y = e.ay + e.ah + P(2);
    else *y = e.ay - ht - P(2);
    *hh = ht;
}

void combo_frame_input() {
    int h = g.combo_open; if (!valid(h)) { g.combo_open = 0; return; }
    Element &e = g.el[h];
    int x, y, w, ht, rh; combo_rect(h, &x, &y, &w, &ht, &rh);
    bool down = (g.buttons & 1) && !(g.pbuttons & 1);
    bool inside = g.mx >= x && g.mx < x + w && g.my >= y && g.my < y + ht;
    int maxv = 10; int top = g.combo_scroll;
    g.combo_hover = -1;
    if (inside) { int idx = top + (g.my - (y + P(4))) / rh; if (idx >= 0 && idx < e.count) g.combo_hover = idx; }
    if (g.wheel) { g.combo_scroll -= g.wheel; int maxs = std::max(0, e.count - maxv); if (g.combo_scroll < 0) g.combo_scroll = 0; if (g.combo_scroll > maxs) g.combo_scroll = maxs; g.wheel = 0; request_redraw(); }
    if (down) {
        if (inside && g.combo_hover >= 0) { if (e.iv) *e.iv = g.combo_hover; fire_change(h); g.combo_open = 0; }
        else g.combo_open = 0;
        request_redraw();
    }
    for (const KeyEv &k : g.keys) if (k.vk == VK_ESCAPE) g.combo_open = 0;
    request_redraw();
}

void combo_draw() {
    int h = g.combo_open; if (!valid(h)) return;
    Element &e = g.el[h];
    Graphics *gr = g.g;
    int x, y, w, ht, rh; combo_rect(h, &x, &y, &w, &ht, &rh);
    RectF r((REAL)x, (REAL)y, (REAL)w, (REAL)ht);
    GraphicsPath p; round_rect_path(p, r, (REAL)P(6));
    SolidBrush bg(gc(th(13))); gr->FillPath(&bg, &p);
    Pen pen(gc(th(5)), 1.0f); gr->DrawPath(&pen, &p);
    int fs = eff_font(e);
    int top = g.combo_scroll, maxv = 10, vis = std::min(e.count - top, maxv);
    int yy = y + P(4);
    for (int i = 0; i < vis; i++) {
        int idx = top + i;
        if (idx == g.combo_hover) { SolidBrush hb(gc(th(3))); gr->FillRectangle(&hb, x + P(3), yy, w - P(6), rh); }
        if (e.iv && idx == *e.iv) { SolidBrush ab(gca(th(9), 60)); gr->FillRectangle(&ab, x + P(3), yy, w - P(6), rh); }
        RectF tr((REAL)(x + P(10)), (REAL)yy, (REAL)(w - P(16)), (REAL)rh);
        Font *f = get_font(fs, 0, 0); SolidBrush tb(gc(th(6)));
        StringFormat sf(StringFormat::GenericTypographic()); sf.SetLineAlignment(StringAlignmentCenter); sf.SetFormatFlags(sf.GetFormatFlags() | StringFormatFlagsNoWrap); sf.SetTrimming(StringTrimmingEllipsisCharacter);
        gr->DrawString(wide(e.items[idx]).c_str(), -1, f, tr, &sf, &tb);
        yy += rh;
    }
}

// --- подсказка ---
void draw_tooltip() {
    if (!g.tip_show || !valid(g.tip_el)) return;
    Element &e = g.el[g.tip_el];
    if (e.tip.empty()) return;
    Graphics *gr = g.g;
    std::wstring t = wide(e.tip.c_str());
    int tw, thh; text_size(t, g.def_font - 1, 0, 0, false, 0, &tw, &thh);
    int pad = P(7);
    int bw = tw + pad * 2, bh = thh + pad;
    int x = g.mx + P(14), y = g.my + P(18);
    int cw = g.el[g.window].aw, ch = g.el[g.window].ah;
    if (x + bw > cw) x = cw - bw - P(4); if (y + bh > ch) y = g.my - bh - P(6);
    RectF r((REAL)x, (REAL)y, (REAL)bw, (REAL)bh);
    GraphicsPath p; round_rect_path(p, r, (REAL)P(5));
    SolidBrush bg(gc(g.dark ? 0x3a3a3d : 0x2b2b2b)); gr->FillPath(&bg, &p);
    Pen pen(gc(th(5)), 1.0f); gr->DrawPath(&pen, &p);
    draw_text_in_tip(gr, x + pad, y + pad / 2, t, 0xf0f0f0, g.def_font - 1);
}

// небольшой помощник (во избежание зависимости от draw_text_in)
static void draw_text_in_tip(Gdiplus::Graphics *gr, int x, int y, const std::wstring &s, u32 color, int size) {
    Font *f = get_font(size, 0, 0); SolidBrush br(gc(color));
    StringFormat sf(StringFormat::GenericTypographic()); sf.SetFormatFlags(sf.GetFormatFlags() | StringFormatFlagsNoWrap | StringFormatFlagsMeasureTrailingSpaces);
    gr->DrawString(s.c_str(), -1, f, PointF((REAL)x, (REAL)y), &sf, &br);
}

// --- открыть контекстное меню, если есть ---
static int find_ctx(int h) { while (valid(h)) { if (g.el[h].ctx) return g.el[h].ctx; if (h == g.window) break; h = g.el[h].parent; } return 0; }

// --- главный разбор ввода кадра ---
void input_frame() {
    int root = valid(g.modal) ? g.modal : g.window;
    int hot = hit_test(root, g.mx, g.my);
    g.hot = hot;
    int hv_tgt = actionable(hot);

    // наведение (плавно)
    for (int i = 1; i < (int)g.el.size(); i++) {
        if (!g.el[i].alive) continue;
        double tgt = (i == hv_tgt && hover_kind(g.el[i].kind)) ? 1.0 : 0.0;
        double d = tgt - g.el[i].hv;
        if (std::fabs(d) > 0.02) { g.el[i].hv += d * 0.3; g.dirty = true; } else g.el[i].hv = tgt;
    }

    // подсказка
    if (hot != g.tip_el) { g.tip_el = hot; g.tip_since = g.now_ms; g.tip_show = false; }
    else if (!g.tip_show && valid(hot) && !g.el[hot].tip.empty() && g.now_ms - g.tip_since > 600) { g.tip_show = true; g.dirty = true; }
    if (g.mx != g.pmx || g.my != g.pmy) { g.tip_since = g.now_ms; if (g.tip_show) { g.tip_show = false; g.dirty = true; } }

    bool down = (g.buttons & 1) && !(g.pbuttons & 1);
    bool up = !(g.buttons & 1) && (g.pbuttons & 1);
    bool rdown = (g.buttons & 2) && !(g.pbuttons & 2);

    // колесо — прокрутка ближайшего списка/скролла/memo
    if (g.wheel) {
        int s = hot;
        while (valid(s)) { int k = g.el[s].kind; if (k == K_SCROLL || k == K_LIST || k == K_MEMO) break; if (s == g.window) { s = 0; break; } s = g.el[s].parent; }
        if (valid(s)) { g.el[s].scroll -= g.wheel * P(40); if (g.el[s].scroll < 0) g.el[s].scroll = 0; g.dirty = true; }
        g.wheel = 0;
    }

    if (rdown) { int m = find_ctx(hot); if (m) { Popup pu; pu.menu = m; pu.x = g.mx; pu.y = g.my; pu.hover = 0; menu_close_all(); g.popups.push_back(pu); request_redraw(); return; } }

    if (down) {
        int a = actionable(hot);
        g.active = a; g.press_x = g.mx; g.press_y = g.my;
        if (valid(a)) {
            Element &e = g.el[a];
            switch (e.kind) {
            case K_EDIT: case K_MEMO: case K_PASSWORD: set_focus(a); { int c = caret_from_x(a, g.mx, g.my); e.caret = c; e.anchor = c; } break;
            case K_SLIDER: { int tl = e.ax + P(8), tr = e.ax + e.aw - P(8); double f = tr > tl ? (double)(g.mx - tl) / (tr - tl) : 0; if (f < 0) f = 0; if (f > 1) f = 1; int nv = e.vmin + (int)(f * (e.vmax - e.vmin) + 0.5); if (e.iv && *e.iv != nv) { *e.iv = nv; fire_change(a); } set_focus(0); } break;
            case K_NUMBER: { if (g.mx > e.ax + e.aw - P(28)) { bool upper = g.my < e.ay + e.ah / 2; int nv = (e.iv ? *e.iv : 0) + (upper ? e.step : -e.step); if (nv < e.vmin) nv = e.vmin; if (nv > e.vmax) nv = e.vmax; if (e.iv) *e.iv = nv; fire_change(a); } } break;
            case K_COMBO: g.combo_open = a; g.combo_scroll = 0; g.combo_hover = e.iv ? *e.iv : 0; break;
            case K_LIST: { int rh; { int tw; text_size(L"Ag", eff_font(e), 0, 0, false, 0, &tw, &rh); } rh += P(8); int idx = (g.my - (e.ay + P(e.pt)) + e.scroll) / rh; if (idx >= 0 && idx < e.count) { int prev = e.iv ? *e.iv : -1; if (e.iv) *e.iv = idx; if (idx == prev) fire_click(a); else fire_change(a); } set_focus(0); } break;
            case K_TAB: { int tabs = e.parent; int idx = 0, c = g.el[tabs].first; while (c) { if (g.el[c].kind == K_TAB) { if (c == a) break; idx++; } c = g.el[c].next; } if (g.el[tabs].ival != idx) { g.el[tabs].ival = idx; fire_change(tabs); } } break;
            case K_MENU: if (g.el[a].parent == g.window || g.el[g.el[a].parent].kind == K_MENUBAR) menu_open_bar(a); break;
            default: if (is_focusable(e.kind)) {} else set_focus(0); break;
            }
        } else set_focus(0);
    }

    // перетаскивание
    if ((g.buttons & 1) && valid(g.active)) {
        Element &e = g.el[g.active];
        if (e.kind == K_SLIDER) { int tl = e.ax + P(8), tr = e.ax + e.aw - P(8); double f = tr > tl ? (double)(g.mx - tl) / (tr - tl) : 0; if (f < 0) f = 0; if (f > 1) f = 1; int nv = e.vmin + (int)(f * (e.vmax - e.vmin) + 0.5); if (e.iv && *e.iv != nv) { *e.iv = nv; fire_change(g.active); } }
        else if (is_focusable(e.kind)) { int c = caret_from_x(g.active, g.mx, g.my); e.caret = c; g.dirty = true; }
    }

    if (up) {
        int a = g.active;
        if (valid(a) && actionable(hit_test(g.window, g.mx, g.my)) == a) {
            Element &e = g.el[a];
            switch (e.kind) {
            case K_BUTTON: case K_TOOL: fire_click(a); break;
            case K_CHECK: if (e.iv) *e.iv = !*e.iv; fire_change(a); fire_click(a); break;
            case K_RADIO: if (e.iv && *e.iv != e.ival) { *e.iv = e.ival; fire_change(a); } fire_click(a); break;
            case K_TEXT: case K_IMAGE: case K_BORDER: case K_STACK: case K_PAINT: if (e.on_click) fire_click(a); break;
            default: break;
            }
        }
        g.active = 0;
    }

    // горячие клавиши окна
    for (const KeyEv &k : g.keys) {
        if (!k.vk) continue;
        if (valid(g.focus) && is_focusable(g.el[g.focus].kind)) continue;
        for (Hotkey &hk : g.hotkeys) if (hk.vk == k.vk && hk.mods == (k.mods & (EL_SHIFT | EL_CTRL | EL_ALT))) { if (hk.fn) hk.fn(0, hk.arg); request_redraw(); }
    }

    // клавиши в поле ввода
    if (valid(g.focus) && is_focusable(g.el[g.focus].kind) && !g.keys.empty()) edit_input(g.focus, g.keys);
}

} // namespace eli
