// Готовые окна Elements: сообщение, вопрос, уведомление-тост, выбор файла и
// папки (родные диалоги Windows), своё модальное окно.
#include "el_internal.h"
#include <commdlg.h>
#include <shlobj.h>
#include <string>
#include <algorithm>

#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")

using namespace Gdiplus;

namespace eli {

static inline int P(int d) { return px(d); }

// ------------------------------------------------- сообщение / вопрос ------

void el_message(const char *title, const char *text) {
    g.msg_kind = 1; g.msg_title = title ? title : ""; g.msg_text = text ? text : "";
    g.ask_fn = nullptr;
    request_redraw();
}
void el_ask(const char *title, const char *text, void (*fn)(int, void *), void *arg) {
    g.msg_kind = 2; g.msg_title = title ? title : ""; g.msg_text = text ? text : "";
    g.ask_fn = fn; g.ask_arg = arg;
    request_redraw();
}

// геометрия окна сообщения + кнопок
static void msg_layout(int *x, int *y, int *w, int *h, RECT *b1, RECT *b2) {
    int tw, thh; text_size(wide(g.msg_text.c_str()), g.def_font, 0, 0, true, 360, &tw, &thh);
    int titw, tith; text_size(wide(g.msg_title.c_str()), g.def_font + 3, 1, 0, false, 0, &titw, &tith);
    int cw = std::max(std::max(tw, titw) + P(44), P(300));
    int ch = tith + P(14) + thh + P(24) + P(40);
    int winW = g.el[g.window].aw, winH = g.el[g.window].ah;
    int cx = g.el[g.window].ax + (winW - cw) / 2, cy = g.el[g.window].ay + (winH - ch) / 2;
    *x = cx; *y = cy; *w = cw; *h = ch;
    int bw = P(96), bh = P(34), by = cy + ch - P(18) - bh;
    if (g.msg_kind == 2) {
        b1->left = cx + cw - P(18) - bw; b1->top = by; b1->right = b1->left + bw; b1->bottom = by + bh;      // Да
        b2->left = b1->left - P(10) - bw; b2->top = by; b2->right = b2->left + bw; b2->bottom = by + bh;      // Нет
    } else {
        b1->left = cx + cw - P(18) - bw; b1->top = by; b1->right = b1->left + bw; b1->bottom = by + bh;      // OK
        *b2 = RECT{ 0, 0, 0, 0 };
    }
}

void overlay_frame_input() {
    if (!g.msg_kind) return;
    int x, y, w, h; RECT b1, b2; msg_layout(&x, &y, &w, &h, &b1, &b2);
    bool down = (g.buttons & 1) && !(g.pbuttons & 1);
    auto in = [&](RECT &r) { return g.mx >= r.left && g.mx < r.right && g.my >= r.top && g.my < r.bottom; };
    for (const KeyEv &k : g.keys) {
        if (k.vk == VK_ESCAPE) { int kind = g.msg_kind; g.msg_kind = 0; if (kind == 2 && g.ask_fn) g.ask_fn(0, g.ask_arg); return; }
        if (k.vk == VK_RETURN) { int kind = g.msg_kind; g.msg_kind = 0; if (kind == 2 && g.ask_fn) g.ask_fn(1, g.ask_arg); return; }
    }
    if (down) {
        if (in(b1)) { int kind = g.msg_kind; g.msg_kind = 0; if (kind == 2 && g.ask_fn) g.ask_fn(1, g.ask_arg); }
        else if (g.msg_kind == 2 && in(b2)) { g.msg_kind = 0; if (g.ask_fn) g.ask_fn(0, g.ask_arg); }
    }
    request_redraw();
}

static void draw_button(Graphics *gr, RECT r, const wchar_t *label, bool primary) {
    RectF rr((REAL)r.left, (REAL)r.top, (REAL)(r.right - r.left), (REAL)(r.bottom - r.top));
    bool hot = g.mx >= r.left && g.mx < r.right && g.my >= r.top && g.my < r.bottom;
    GraphicsPath p; round_rect_path(p, rr, (REAL)P(6));
    u32 bg = primary ? th(9) : th(2);
    if (hot) bg = mix(bg, g.dark ? 0xffffff : 0x000000, 0.1);
    SolidBrush b(gc(bg)); gr->FillPath(&b, &p);
    if (!primary) { Pen pen(gc(th(5)), 1.0f); gr->DrawPath(&pen, &p); }
    Font *f = get_font(g.def_font, 0, 0); SolidBrush tb(gc(primary ? th(10) : th(6)));
    StringFormat sf; sf.SetAlignment(StringAlignmentCenter); sf.SetLineAlignment(StringAlignmentCenter);
    gr->DrawString(label, -1, f, rr, &sf, &tb);
}

void overlay_draw() {
    Graphics *gr = g.g;
    // модальное окно программы
    if (valid(g.modal)) {
        SolidBrush veil(gca(0x000000, 120)); gr->FillRectangle(&veil, g.el[g.window].ax, g.el[g.window].ay, g.el[g.window].aw, g.el[g.window].ah);
        draw_element(g.modal);
    }
    // сообщение / вопрос
    if (g.msg_kind) {
        SolidBrush veil(gca(0x000000, 120)); gr->FillRectangle(&veil, g.el[g.window].ax, g.el[g.window].ay, g.el[g.window].aw, g.el[g.window].ah);
        int x, y, w, h; RECT b1, b2; msg_layout(&x, &y, &w, &h, &b1, &b2);
        RectF r((REAL)x, (REAL)y, (REAL)w, (REAL)h);
        for (int i = 6; i >= 1; i--) { SolidBrush sb(gca(0x000000, 12)); GraphicsPath sp; round_rect_path(sp, RectF(r.X - i, r.Y - i + 4, r.Width + 2 * i, r.Height + 2 * i), (REAL)P(12)); gr->FillPath(&sb, &sp); }
        GraphicsPath p; round_rect_path(p, r, (REAL)P(10));
        SolidBrush bg(gc(th(1))); gr->FillPath(&bg, &p);
        Pen pen(gc(th(5)), 1.0f); gr->DrawPath(&pen, &p);
        int titw, tith; text_size(wide(g.msg_title.c_str()), g.def_font + 3, 1, 0, false, 0, &titw, &tith);
        Font *tf = get_font(g.def_font + 3, 1, 0); SolidBrush ttb(gc(th(7)));
        StringFormat nf(StringFormat::GenericTypographic()); nf.SetFormatFlags(nf.GetFormatFlags() | StringFormatFlagsNoWrap);
        gr->DrawString(wide(g.msg_title.c_str()).c_str(), -1, tf, PointF((REAL)(x + P(22)), (REAL)(y + P(18))), &nf, &ttb);
        Font *bf = get_font(g.def_font, 0, 0); SolidBrush btb(gc(th(6)));
        RectF txr((REAL)(x + P(22)), (REAL)(y + P(18) + tith + P(10)), (REAL)(w - P(44)), (REAL)(h - tith - P(90)));
        StringFormat wf(StringFormat::GenericTypographic());
        gr->DrawString(wide(g.msg_text.c_str()).c_str(), -1, bf, txr, &wf, &btb);
        if (g.msg_kind == 2) { draw_button(gr, b1, L"Да", true); draw_button(gr, b2, L"Нет", false); }
        else draw_button(gr, b1, L"OK", true);
    }
    // тост
    if (g.toast_until > g.now_ms && !g.toast_text.empty()) {
        double age = g.now_ms - g.toast_t0, life = g.toast_until - g.toast_t0;
        double appear = std::min(1.0, age / 200.0);
        double fade = std::min(1.0, (g.toast_until - g.now_ms) / 300.0);
        double a = std::min(appear, fade);
        int tw, thh; text_size(wide(g.toast_text.c_str()), g.def_font, 0, 0, false, 0, &tw, &thh);
        int bw = tw + P(28), bh = thh + P(16);
        int cx = g.el[g.window].ax + (g.el[g.window].aw - bw) / 2;
        int cy = g.el[g.window].ay + g.el[g.window].ah - bh - P(24) - (int)((1 - appear) * P(20));
        RectF r((REAL)cx, (REAL)cy, (REAL)bw, (REAL)bh);
        GraphicsPath p; round_rect_path(p, r, (REAL)(bh / 2.0f));
        SolidBrush bg(gca(g.dark ? 0x202024 : 0x303030, (int)(230 * a))); gr->FillPath(&bg, &p);
        Font *f = get_font(g.def_font, 0, 0); SolidBrush tb(gca(0xffffff, (int)(255 * a)));
        StringFormat sf; sf.SetAlignment(StringAlignmentCenter); sf.SetLineAlignment(StringAlignmentCenter);
        gr->DrawString(wide(g.toast_text.c_str()).c_str(), -1, f, r, &sf, &tb);
        request_redraw();
    }
}

void el_toast(const char *text, int ms) {
    g.toast_text = text ? text : ""; g.toast_t0 = g.now_ms; g.toast_until = g.now_ms + (ms <= 0 ? 2500 : ms);
    request_redraw();
}

// ------------------------------------------------- модальное окно ----------

el el_dialog(const char *title, int width) {
    int outer = el_alloc(K_BORDER);           // не добавляем в дерево окна — раскладывается отдельно
    Element &o = g.el[outer];
    o.w = width > 0 ? width : 320;
    o.has_bg = true; o.bg = th(1); o.has_bd = true; o.bd = th(5); o.bw = 1; o.radius = 10;
    o.pl = o.pr = 18; o.pt = 16; o.pb = 16;
    o.sh_col = 0x000000; o.sh_blur = 24; o.sh_dx = 0; o.sh_dy = 10; o.sh_a = 120;
    int stack = el_alloc(K_STACK); g.el[stack].horiz = false; el_add_child(outer, stack);
    if (title && *title) { int t = el_alloc(K_TEXT); g.el[t].text = title; g.el[t].fsize = g.def_font + 3; g.el[t].bold = 1; g.el[t].mb = 10; el_add_child(stack, t); }
    g.modal = outer;
    request_redraw();
    return stack;
}
void el_dialog_close(el d) {
    if (!valid(d)) { g.modal = 0; return; }
    int outer = g.el[d].parent ? g.el[d].parent : d;
    if (g.modal == outer || g.modal == d) g.modal = 0;
    // освободить поддерево
    el_remove(outer);
    request_redraw();
}

// ------------------------------------------------- файлы -------------------

static std::wstring make_filter(const char *filter) {
    // ".txt;.c" -> "Файлы (*.txt;*.c)\0*.txt;*.c\0Все файлы\0*.*\0"
    std::wstring res;
    if (filter && *filter) {
        std::wstring f = wide(filter);
        std::wstring pats;
        std::wstring cur;
        for (wchar_t c : f) { if (c == L';') { if (!cur.empty()) { if (!pats.empty()) pats += L";"; pats += L"*" + cur; cur.clear(); } } else cur += c; }
        if (!cur.empty()) { if (!pats.empty()) pats += L";"; pats += L"*" + cur; }
        res = L"Файлы (" + pats + L")"; res.push_back(0); res += pats; res.push_back(0);
    }
    std::wstring all = L"Все файлы (*.*)"; res += all; res.push_back(0); res += L"*.*"; res.push_back(0); res.push_back(0);
    return res;
}

void el_open_file(const char *title, const char *folder, const char *filter, void (*fn)(const char *, void *), void *arg) {
    wchar_t buf[1024] = { 0 };
    std::wstring flt = make_filter(filter);
    std::wstring dir = wide(folder ? folder : ""); std::wstring ttl = wide(title ? title : "");
    OPENFILENAMEW ofn = { sizeof(ofn) };
    ofn.hwndOwner = g.hwnd; ofn.lpstrFilter = flt.c_str(); ofn.lpstrFile = buf; ofn.nMaxFile = 1024;
    ofn.lpstrInitialDir = dir.empty() ? nullptr : dir.c_str(); ofn.lpstrTitle = ttl.empty() ? nullptr : ttl.c_str();
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_EXPLORER;
    if (GetOpenFileNameW(&ofn)) { if (fn) { std::string u = narrow(buf); fn(u.c_str(), arg); } }
}
void el_save_file(const char *title, const char *folder, const char *name, void (*fn)(const char *, void *), void *arg) {
    wchar_t buf[1024] = { 0 };
    std::wstring nm = wide(name ? name : ""); wcsncpy(buf, nm.c_str(), 1023);
    std::wstring dir = wide(folder ? folder : ""); std::wstring ttl = wide(title ? title : "");
    OPENFILENAMEW ofn = { sizeof(ofn) };
    ofn.hwndOwner = g.hwnd; ofn.lpstrFile = buf; ofn.nMaxFile = 1024;
    ofn.lpstrInitialDir = dir.empty() ? nullptr : dir.c_str(); ofn.lpstrTitle = ttl.empty() ? nullptr : ttl.c_str();
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY | OFN_EXPLORER;
    if (GetSaveFileNameW(&ofn)) { if (fn) { std::string u = narrow(buf); fn(u.c_str(), arg); } }
}
void el_pick_folder(const char *title, const char *folder, void (*fn)(const char *, void *), void *arg) {
    (void)folder;
    BROWSEINFOW bi = { 0 };
    std::wstring ttl = wide(title ? title : "");
    bi.hwndOwner = g.hwnd; bi.lpszTitle = ttl.c_str(); bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
    if (pidl) {
        wchar_t path[MAX_PATH];
        if (SHGetPathFromIDListW(pidl, path)) { std::wstring p = path; if (!p.empty() && p.back() != L'\\') p += L"\\"; if (fn) { std::string u = narrow(p); fn(u.c_str(), arg); } }
        CoTaskMemFree(pidl);
    }
}

} // namespace eli
