// Меню Elements: полоса меню, выпадающие и подменю, контекстное меню.
#include "el_internal.h"
#include <algorithm>
#include <cstring>

using namespace Gdiplus;

namespace eli {

static inline int P(int d) { return px(d); }
static int eff_font() { return g.def_font; }

// ------------------------------------------------------------- создание ----

el el_menubar(el parent) {
    if (parent == 0) parent = g.window;
    int h = el_alloc(K_MENUBAR);
    g.el[h].dock = D_TOP;
    el_add_child(parent, h);
    request_redraw();
    return h;
}

el el_menu(el parent, const char *text) {
    int h = el_alloc(K_MENU);
    std::string s = text ? text : "";
    size_t tab = s.find('\t');
    if (tab != std::string::npos) { g.el[h].accel = s.substr(tab + 1); s = s.substr(0, tab); }
    g.el[h].text = s;
    if (valid(parent)) { g.el[h].is_bar = (g.el[parent].kind == K_MENUBAR); el_add_child(parent, h); }
    request_redraw();
    return h;
}

void el_menu_check(el item, int *value) { if (valid(item)) g.el[item].menu_val = value; }

el el_context_menu(el target) {
    if (!valid(target)) return 0;
    int h = el_alloc(K_MENU);
    el_add_child(target, h);          // раскладка его пропустит (K_MENU)
    g.el[target].ctx = h;
    return h;
}

el el_popup_menu(void) {
    int h = el_alloc(K_MENU);
    return h;                          // отдельное, в дереве не висит
}

void el_menu_popup(el menu, el at, int x, int y) {
    if (!valid(menu)) return;
    int ox = 0, oy = 0;
    if (valid(at)) { ox = g.el[at].ax; oy = g.el[at].ay; } else { ox = g.el[g.window].ax; oy = g.el[g.window].ay; }
    menu_close_all();
    Popup pu; pu.menu = menu; pu.x = ox + px(x); pu.y = oy + px(y); pu.hover = 0;
    g.popups.push_back(pu);
    request_redraw();
}

// ------------------------------------------------------------- раскладка ---

static bool has_sub(int item) {
    if (!valid(item)) return false;
    int c = g.el[item].first;
    while (c) { if (g.el[c].kind == K_MENU) return true; c = g.el[c].next; }
    return false;
}

int menu_measure_width(int menu) {
    int w = P(120);
    int c = g.el[menu].first;
    while (c) {
        if (g.el[c].kind == K_MENU) {
            int tw, th2; text_size(wide(g.el[c].text.c_str()), eff_font(), 0, 0, false, 0, &tw, &th2);
            int aw = 0; if (!g.el[c].accel.empty()) { int ah; text_size(wide(g.el[c].accel.c_str()), eff_font(), 0, 0, false, 0, &aw, &ah); }
            int iw = P(28) + tw + (aw ? P(24) + aw : 0) + (has_sub(c) ? P(18) : 0) + P(16);
            w = std::max(w, iw);
        }
        c = g.el[c].next;
    }
    return w;
}

struct Item { int h; int y, height; bool sep; };
static void popup_items(int menu, std::vector<Item> &out, int *width, int *height) {
    out.clear();
    int rh; { int tw; text_size(L"Ag", eff_font(), 0, 0, false, 0, &tw, &rh); } rh += P(10);
    int y = P(6);
    int c = g.el[menu].first;
    while (c) {
        if (g.el[c].kind == K_MENU) { Item it{ c, y, rh, false }; out.push_back(it); y += rh; }
        else if (g.el[c].kind == K_SEP) { Item it{ c, y, P(9), true }; out.push_back(it); y += P(9); }
        c = g.el[c].next;
    }
    *width = menu_measure_width(menu);
    *height = y + P(6);
}

static void popup_bounds(const Popup &pu, int *x, int *y, int *w, int *h) {
    std::vector<Item> items; int ww, hh; popup_items(pu.menu, items, &ww, &hh);
    int px0 = pu.x, py0 = pu.y;
    int winR = g.el[g.window].ax + g.el[g.window].aw, winB = g.el[g.window].ay + g.el[g.window].ah;
    if (px0 + ww > winR) px0 = std::max(0, winR - ww);
    if (py0 + hh > winB) py0 = std::max(0, winB - hh);
    *x = px0; *y = py0; *w = ww; *h = hh;
}

// ------------------------------------------------------------- открытие ----

void menu_open_bar(int item) {
    if (!valid(item)) return;
    menu_close_all();
    g.el[item].menu_open = true;
    Popup pu; pu.menu = item; pu.x = g.el[item].ax; pu.y = g.el[item].ay + g.el[item].ah; pu.hover = 0;
    g.popups.push_back(pu);
    request_redraw();
}

void menu_close_all() {
    for (auto &pu : g.popups) if (valid(pu.menu)) g.el[pu.menu].menu_open = false;
    // снять подсветку у пунктов полосы
    for (int i = 1; i < (int)g.el.size(); i++) if (g.el[i].alive && g.el[i].kind == K_MENU) g.el[i].menu_open = false;
    g.popups.clear();
    request_redraw();
}

static int bar_item_at(int mx, int my) {
    for (int i = 1; i < (int)g.el.size(); i++) {
        Element &e = g.el[i];
        if (e.alive && e.kind == K_MENU && e.is_bar)
            if (mx >= e.ax && mx < e.ax + e.aw && my >= e.ay && my < e.ay + e.ah) return i;
    }
    return 0;
}

// ------------------------------------------------------------- ввод --------

void menu_frame_input() {
    for (const KeyEv &k : g.keys) if (k.vk == VK_ESCAPE) { menu_close_all(); return; }
    bool down = (g.buttons & 1) && !(g.pbuttons & 1);

    int hpop = -1, hitem = 0;
    for (int pi = (int)g.popups.size() - 1; pi >= 0; pi--) {
        int x, y, w, h; popup_bounds(g.popups[pi], &x, &y, &w, &h);
        if (g.mx >= x && g.mx < x + w && g.my >= y && g.my < y + h) {
            std::vector<Item> items; int ww, hh; popup_items(g.popups[pi].menu, items, &ww, &hh);
            for (auto &it : items) if (!it.sep && g.my >= y + it.y && g.my < y + it.y + it.height) { hitem = it.h; break; }
            g.popups[pi].hover = hitem;
            hpop = pi; break;
        } else g.popups[pi].hover = 0;
    }

    // переключение между пунктами полосы при наведении
    if (hpop < 0) {
        int bi = bar_item_at(g.mx, g.my);
        if (bi && !g.popups.empty() && g.popups[0].menu != bi) { menu_open_bar(bi); return; }
    }

    // открыть/закрыть подменю по наведению
    if (hpop >= 0 && valid(hitem) && has_sub(hitem)) {
        bool already = ((int)g.popups.size() > hpop + 1) && g.popups[hpop + 1].menu == hitem;
        if (!already) {
            while ((int)g.popups.size() > hpop + 1) { if (valid(g.popups.back().menu)) g.el[g.popups.back().menu].menu_open = false; g.popups.pop_back(); }
            int x, y, w, h; popup_bounds(g.popups[hpop], &x, &y, &w, &h);
            std::vector<Item> items; int ww, hh; popup_items(g.popups[hpop].menu, items, &ww, &hh);
            int iy = y;
            for (auto &it : items) if (it.h == hitem) { iy = y + it.y; break; }
            Popup sub; sub.menu = hitem; sub.x = x + w - P(2); sub.y = iy; sub.hover = 0;
            g.popups.push_back(sub);
            request_redraw();
        }
    } else if (hpop >= 0) {
        while ((int)g.popups.size() > hpop + 1) { if (valid(g.popups.back().menu)) g.el[g.popups.back().menu].menu_open = false; g.popups.pop_back(); }
    }

    if (down) {
        if (hpop < 0) {
            int bi = bar_item_at(g.mx, g.my);
            if (!bi) menu_close_all();
        } else if (valid(hitem) && g.el[hitem].kind == K_MENU && !has_sub(hitem)) {
            if (g.el[hitem].menu_val) *g.el[hitem].menu_val = !*g.el[hitem].menu_val;
            int act = hitem;
            menu_close_all();
            fire_click(act);
        }
    }
    request_redraw();
}

// ------------------------------------------------------------- рисование ---

void menu_draw() {
    Graphics *gr = g.g;
    for (auto &pu : g.popups) {
        if (!valid(pu.menu)) continue;
        int x, y, w, h; popup_bounds(pu, &x, &y, &w, &h);
        RectF r((REAL)x, (REAL)y, (REAL)w, (REAL)h);
        // тень
        for (int i = 5; i >= 1; i--) { SolidBrush sb(gca(0x000000, 10)); GraphicsPath sp; round_rect_path(sp, RectF(r.X - i, r.Y - i + 3, r.Width + 2 * i, r.Height + 2 * i), (REAL)P(8)); gr->FillPath(&sb, &sp); }
        GraphicsPath p; round_rect_path(p, r, (REAL)P(7));
        SolidBrush bg(gc(th(13))); gr->FillPath(&bg, &p);
        Pen pen(gc(th(5)), 1.0f); gr->DrawPath(&pen, &p);

        std::vector<Item> items; int ww, hh; popup_items(pu.menu, items, &ww, &hh);
        int fs = eff_font();
        for (auto &it : items) {
            int iy = y + it.y;
            if (it.sep) { SolidBrush sb(gc(th(5))); gr->FillRectangle(&sb, x + P(8), iy + P(4), w - P(16), P(1)); continue; }
            Element &mi = g.el[it.h];
            bool hot = (pu.hover == it.h) || mi.menu_open;
            if (hot) { SolidBrush hb(gc(th(3))); GraphicsPath hp; round_rect_path(hp, RectF((REAL)(x + P(4)), (REAL)iy, (REAL)(w - P(8)), (REAL)it.height), (REAL)P(5)); gr->FillPath(&hb, &hp); }
            // галочка
            if (mi.menu_val && *mi.menu_val) { Pen cp(gc(th(9)), 2.0f); Point chk[3] = { Point(x + P(10), iy + it.height / 2), Point(x + P(14), iy + it.height / 2 + P(4)), Point(x + P(20), iy + it.height / 2 - P(4)) }; gr->DrawLines(&cp, chk, 3); }
            SolidBrush tb(gc(mi.disabled ? th(8) : th(6)));
            Font *f = get_font(fs, 0, 0);
            StringFormat sf(StringFormat::GenericTypographic()); sf.SetLineAlignment(StringAlignmentCenter); sf.SetFormatFlags(sf.GetFormatFlags() | StringFormatFlagsNoWrap);
            gr->DrawString(wide(mi.text.c_str()).c_str(), -1, f, RectF((REAL)(x + P(28)), (REAL)iy, (REAL)(w - P(44)), (REAL)it.height), &sf, &tb);
            if (!mi.accel.empty()) { SolidBrush ab(gc(th(8))); StringFormat af(StringFormat::GenericTypographic()); af.SetAlignment(StringAlignmentFar); af.SetLineAlignment(StringAlignmentCenter); af.SetFormatFlags(af.GetFormatFlags() | StringFormatFlagsNoWrap); gr->DrawString(wide(mi.accel.c_str()).c_str(), -1, f, RectF((REAL)(x + P(10)), (REAL)iy, (REAL)(w - P(26)), (REAL)it.height), &af, &ab); }
            if (has_sub(it.h)) { Pen ap(gc(th(8)), 1.6f); int ax = x + w - P(14), ay = iy + it.height / 2; Point arr[3] = { Point(ax - P(2), ay - P(4)), Point(ax + P(3), ay), Point(ax - P(2), ay + P(4)) }; gr->DrawLines(&ap, arr, 3); }
        }
    }
}

} // namespace eli
