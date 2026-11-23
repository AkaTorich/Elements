// Раскладка Elements, как в WPF: measure (желаемый размер) и arrange
// (размещение). Вся геометрия — в пикселях; размеры из API (точки) переводятся
// px() при чтении. availw/availh == EL_INF — без ограничения.
#include "el_internal.h"
#include <algorithm>

namespace eli {

static inline int P(int d) { return px(d); }

static int eff_font(const Element &e) { return e.fsize > 0 ? e.fsize : g.def_font; }

static std::wstring disp_text(const Element &e) {
    if (e.live) return wide(e.live);
    return wide(e.text.c_str());
}

static void child_list(int h, std::vector<int> &out) {
    int c = g.el[h].first;
    while (c) {
        Element &ce = g.el[c];
        if (ce.alive && !ce.hidden && ce.kind != K_MENU && ce.kind != K_TAB)
            out.push_back(c);
        c = ce.next;
    }
}

// --- желаемый размер содержимого (без padding) для элемента-контрола ---
static void control_content(int h, int availw, int *cw, int *ch) {
    Element &e = g.el[h];
    int fs = eff_font(e), bold = e.bold, mono = e.mono;
    std::wstring t = disp_text(e);
    switch (e.kind) {
    case K_TEXT: {
        if (e.wrap && availw < EL_INF) {
            int innerw = availw - P(e.pl) - P(e.pr);
            int w, hh; text_size(t, fs, bold, mono, true, innerw < 10 ? 10 : innerw, &w, &hh);
            *cw = w; *ch = hh;
        } else { int w, hh; text_size(t, fs, bold, mono, false, 0, &w, &hh); *cw = w; *ch = hh; }
        break; }
    case K_BUTTON: case K_TOOL: {
        int w, hh; text_size(t, fs, bold, mono, false, 0, &w, &hh);
        int iw = 0;
        if (e.icon_img) { iw = P(e.icon_size > 0 ? e.icon_size : 20) + (t.empty() ? 0 : P(6)); }
        *cw = w + iw; *ch = std::max(hh, e.icon_img ? P(e.icon_size > 0 ? e.icon_size : 20) : 0);
        if (e.kind == K_TOOL && t.empty()) *cw = iw ? P(e.icon_size) : P(20);
        break; }
    case K_CHECK: case K_RADIO: {
        int w, hh; text_size(t, fs, bold, mono, false, 0, &w, &hh);
        int box = P(18);
        *cw = box + (t.empty() ? 0 : P(8) + w); *ch = std::max(box, hh);
        break; }
    case K_EDIT: case K_PASSWORD: {
        int w, hh; text_size(L"Ag", fs, bold, mono, false, 0, &w, &hh);
        *cw = P(120); *ch = hh;
        break; }
    case K_MEMO: {
        int w, hh; text_size(L"Ag", fs, bold, mono, false, 0, &w, &hh);
        *cw = P(160); *ch = hh * 4;
        break; }
    case K_COMBO: case K_NUMBER: {
        int w = 0, hh = 0, mw = P(40);
        if (e.items) for (int i = 0; i < e.count; i++) { int iw; text_size(wide(e.items[i]), fs, bold, mono, false, 0, &iw, &hh); mw = std::max(mw, iw); }
        else text_size(L"0000", fs, bold, mono, false, 0, &mw, &hh);
        if (!hh) { int t2; text_size(L"Ag", fs, bold, mono, false, 0, &t2, &hh); }
        *cw = mw; *ch = hh;
        break; }
    case K_LIST: {
        int mw = P(60), rh = 0;
        int th2; text_size(L"Ag", fs, bold, mono, false, 0, &th2, &rh);
        if (e.items) for (int i = 0; i < e.count; i++) { int iw; int hh; text_size(wide(e.items[i]), fs, bold, mono, false, 0, &iw, &hh); mw = std::max(mw, iw); }
        *cw = mw; *ch = (rh + P(8)) * std::max(1, e.count);
        break; }
    case K_SLIDER: { *cw = P(120); *ch = P(22); break; }
    case K_PROGRESS: { *cw = P(100); *ch = P(14); break; }
    case K_SEP: { *cw = P(1); *ch = P(9); break; }
    case K_IMAGE: {
        if (e.img) { *cw = e.img->GetWidth(); *ch = e.img->GetHeight(); }
        else { *cw = P(60); *ch = P(40); }
        break; }
    case K_PAINT: { *cw = P(80); *ch = P(60); break; }
    default: { *cw = 0; *ch = 0; break; }
    }
}

static int border_inset(const Element &e) {
    if (e.kind == K_BORDER || e.kind == K_GROUP) {
        int bw = e.bw > 0 ? P(e.bw) : (e.has_bd ? P(1) : 0);
        return bw;
    }
    return 0;
}

void measure(int h, int availw, int availh) {
    Element &e = g.el[h];
    int ml = P(e.ml), mt = P(e.mt), mr = P(e.mr), mb = P(e.mb);
    int cavw = (availw >= EL_INF) ? EL_INF : std::max(0, availw - ml - mr);
    int cavh = (availh >= EL_INF) ? EL_INF : std::max(0, availh - mt - mb);
    bool fixW = e.w >= 0, fixH = e.h >= 0;
    int pl = P(e.pl), pt = P(e.pt), pr = P(e.pr), pb = P(e.pb);
    int ins = border_inset(e);
    int hdr = 0;
    if (e.kind == K_GROUP) { int w, hh; text_size(wide(e.text.c_str()), eff_font(e), 1, 0, false, 0, &w, &hh); hdr = hh + P(4); }

    int boxW = fixW ? P(e.w) : cavw;
    int boxH = fixH ? P(e.h) : cavh;
    int innerAvW = (boxW >= EL_INF) ? EL_INF : std::max(0, boxW - pl - pr - 2 * ins);
    int innerAvH = (boxH >= EL_INF) ? EL_INF : std::max(0, boxH - pt - pb - 2 * ins - hdr);

    int cw = 0, ch = 0;   // желаемое содержимое (без padding/border)
    std::vector<int> kids; child_list(h, kids);

    switch (e.kind) {
    case K_WINDOW: case K_DOCK: {
        int remW = innerAvW, remH = innerAvH, usedCross = 0;
        for (size_t i = 0; i + 1 < kids.size(); i++) {
            Element &c = g.el[kids[i]];
            measure(kids[i], remW, remH);
            if (c.dock == D_LEFT || c.dock == D_RIGHT) { if (remW < EL_INF) remW = std::max(0, remW - c.dw); }
            else { if (remH < EL_INF) remH = std::max(0, remH - c.dh); }
        }
        if (!kids.empty()) measure(kids.back(), remW, remH);
        cw = (innerAvW >= EL_INF) ? 0 : innerAvW;
        ch = (innerAvH >= EL_INF) ? 0 : innerAvH;
        break; }
    case K_STACK: {
        int gap = e.gap >= 0 ? P(e.gap) : P(8);
        int n = 0;
        if (!e.horiz) {
            for (int k : kids) { measure(k, innerAvW, EL_INF); cw = std::max(cw, g.el[k].dw); ch += g.el[k].dh; n++; }
            if (n > 1) ch += gap * (n - 1);
        } else {
            for (int k : kids) { measure(k, EL_INF, innerAvH); ch = std::max(ch, g.el[k].dh); cw += g.el[k].dw; n++; }
            if (n > 1) cw += gap * (n - 1);
        }
        break; }
    case K_WRAP: {
        int gap = e.gap >= 0 ? P(e.gap) : P(8);
        int limit = (innerAvW >= EL_INF) ? EL_INF : innerAvW;
        int x = 0, rowh = 0, maxw = 0;
        for (int k : kids) {
            measure(k, EL_INF, EL_INF);
            int w = g.el[k].dw, hh = g.el[k].dh;
            if (x > 0 && limit < EL_INF && x + w > limit) { ch += rowh + gap; maxw = std::max(maxw, x - gap); x = 0; rowh = 0; }
            x += w + gap; rowh = std::max(rowh, hh);
        }
        ch += rowh; maxw = std::max(maxw, x > 0 ? x - gap : 0);
        cw = (limit < EL_INF) ? limit : maxw;
        break; }
    case K_GRID: {
        GridDef *gd = e.grid;
        for (int i = 0; i < gd->nc; i++) gd->cs[i] = (gd->ct[i] == 1) ? P(gd->cv[i]) : 0;
        for (int i = 0; i < gd->nr; i++) gd->rs[i] = (gd->rt[i] == 1) ? P(gd->rv[i]) : 0;
        for (int k : kids) {
            Element &c = g.el[k];
            measure(k, EL_INF, EL_INF);
            int col = std::min(std::max(0, c.col), gd->nc - 1);
            int row = std::min(std::max(0, c.row), gd->nr - 1);
            if (c.cspan <= 1 && gd->ct[col] == 0) gd->cs[col] = std::max(gd->cs[col], c.dw);
            if (c.rspan <= 1 && gd->rt[row] == 0) gd->rs[row] = std::max(gd->rs[row], c.dh);
        }
        int gap = e.gap >= 0 ? P(e.gap) : 0;
        int fixedW = 0; for (int i = 0; i < gd->nc; i++) if (gd->ct[i] != 2) fixedW += gd->cs[i];
        int fixedH = 0; for (int i = 0; i < gd->nr; i++) if (gd->rt[i] != 2) fixedH += gd->rs[i];
        cw = fixedW + gap * std::max(0, gd->nc - 1);
        ch = fixedH + gap * std::max(0, gd->nr - 1);
        if (innerAvW < EL_INF) cw = std::max(cw, innerAvW);
        break; }
    case K_SCROLL: {
        if (!kids.empty()) { measure(kids[0], innerAvW < EL_INF ? innerAvW - P(10) : EL_INF, EL_INF); cw = g.el[kids[0]].dw; ch = g.el[kids[0]].dh; }
        if (innerAvH < EL_INF) ch = std::min(ch, innerAvH);
        break; }
    case K_CANVAS: {
        for (int k : kids) { measure(k, EL_INF, EL_INF); cw = std::max(cw, P(g.el[k].px) + g.el[k].dw); ch = std::max(ch, P(g.el[k].py) + g.el[k].dh); }
        break; }
    case K_BORDER: {
        if (!kids.empty()) { measure(kids[0], innerAvW, innerAvH); cw = g.el[kids[0]].dw; ch = g.el[kids[0]].dh; }
        break; }
    case K_GROUP: {
        if (!kids.empty()) { measure(kids[0], innerAvW, innerAvH); cw = g.el[kids[0]].dw; ch = g.el[kids[0]].dh; }
        { int w, hh; text_size(wide(e.text.c_str()), eff_font(e), 1, 0, false, 0, &w, &hh); cw = std::max(cw, w); }
        break; }
    case K_TABS: {
        int stripH = 0, stripW = 0;
        int c = e.first;
        while (c) { if (g.el[c].kind == K_TAB) { int w, hh; text_size(wide(g.el[c].text.c_str()), eff_font(e), 0, 0, false, 0, &w, &hh); stripH = std::max(stripH, hh + P(14)); stripW += w + P(28); } c = g.el[c].next; }
        int sel = e.ival, idx = 0, body = 0, bodyW = 0;
        c = e.first;
        while (c) { if (g.el[c].kind == K_TAB) { if (idx == sel) { measure(c, innerAvW, innerAvH >= EL_INF ? EL_INF : innerAvH - stripH); body = g.el[c].dh; bodyW = g.el[c].dw; } idx++; } c = g.el[c].next; }
        cw = std::max(stripW, bodyW);
        ch = stripH + body;
        break; }
    case K_TAB: {
        int gap = P(0);
        for (int k : kids) { measure(k, innerAvW, EL_INF); cw = std::max(cw, g.el[k].dw); ch += g.el[k].dh; }
        (void)gap;
        break; }
    case K_MENUBAR: {
        int c = e.first;
        while (c) { if (g.el[c].kind == K_MENU) { int w, hh; text_size(wide(g.el[c].text.c_str()), eff_font(e), 0, 0, false, 0, &w, &hh); cw += w + P(20); ch = std::max(ch, hh + P(8)); } c = g.el[c].next; }
        break; }
    case K_TOOLBAR: {
        int gap = e.gap >= 0 ? P(e.gap) : P(4);
        int n = 0;
        for (int k : kids) { measure(k, EL_INF, innerAvH); ch = std::max(ch, g.el[k].dh); cw += g.el[k].dw; n++; }
        if (n > 1) cw += gap * (n - 1);
        break; }
    default:
        control_content(h, boxW, &cw, &ch);
        break;
    }

    int fullw = fixW ? P(e.w) : (cw + pl + pr + 2 * ins);
    int fullh = fixH ? P(e.h) : (ch + pt + pb + 2 * ins + hdr);
    if (e.minw >= 0) fullw = std::max(fullw, P(e.minw));
    if (e.minh >= 0) fullh = std::max(fullh, P(e.minh));
    if (e.maxw >= 0) fullw = std::min(fullw, P(e.maxw));
    if (e.maxh >= 0) fullh = std::min(fullh, P(e.maxh));
    e.dw = fullw + ml + mr;
    e.dh = fullh + mt + mb;
}

// ----------------------------------------------------------------- arrange -

static void place(int h, int sx, int sy, int sw, int sh) {
    Element &e = g.el[h];
    int ml = P(e.ml), mt = P(e.mt), mr = P(e.mr), mb = P(e.mb);
    int avw = sw - ml - mr, avh = sh - mt - mb;
    if (avw < 0) avw = 0; if (avh < 0) avh = 0;
    int w = e.dw - ml - mr, hh = e.dh - mt - mb;
    if (e.w >= 0) w = P(e.w);
    if (e.h >= 0) hh = P(e.h);

    int x, y;
    if (e.ha == A_STRETCH && e.w < 0) { x = sx + ml; w = avw; }
    else if (e.ha == A_END) x = sx + sw - mr - w;
    else if (e.ha == A_CENTER) x = sx + ml + (avw - w) / 2;
    else x = sx + ml;
    if (e.va == A_STRETCH && e.h < 0) { y = sy + mt; hh = avh; }
    else if (e.va == A_END) y = sy + sh - mb - hh;
    else if (e.va == A_CENTER) y = sy + mt + (avh - hh) / 2;
    else y = sy + mt;

    e.ax = x; e.ay = y; e.aw = w; e.ah = hh;
}

void arrange(int h, int sx, int sy, int sw, int sh, int unused) {
    (void)unused;
    place(h, sx, sy, sw, sh);
    Element &e = g.el[h];
    int ins = border_inset(e);
    int hdr = 0;
    if (e.kind == K_GROUP) { int w, hh; text_size(wide(e.text.c_str()), eff_font(e), 1, 0, false, 0, &w, &hh); hdr = hh + P(4); }
    int cx = e.ax + P(e.pl) + ins, cy = e.ay + P(e.pt) + ins + hdr;
    int cw = e.aw - P(e.pl) - P(e.pr) - 2 * ins, ch = e.ah - P(e.pt) - P(e.pb) - 2 * ins - hdr;
    if (cw < 0) cw = 0; if (ch < 0) ch = 0;

    std::vector<int> kids; child_list(h, kids);

    switch (e.kind) {
    case K_WINDOW: case K_DOCK: {
        int l = cx, t = cy, r = cx + cw, b = cy + ch;
        for (size_t i = 0; i + 1 < kids.size(); i++) {
            Element &c = g.el[kids[i]];
            if (c.dock == D_LEFT) { arrange(kids[i], l, t, c.dw, b - t, 0); l += c.dw; }
            else if (c.dock == D_RIGHT) { arrange(kids[i], r - c.dw, t, c.dw, b - t, 0); r -= c.dw; }
            else if (c.dock == D_TOP) { arrange(kids[i], l, t, r - l, c.dh, 0); t += c.dh; }
            else { arrange(kids[i], l, b - c.dh, r - l, c.dh, 0); b -= c.dh; }
        }
        if (!kids.empty()) arrange(kids.back(), l, t, r - l, b - t, 0);
        break; }
    case K_STACK: {
        int gap = e.gap >= 0 ? P(e.gap) : P(8);
        if (!e.horiz) { int y = cy; for (int k : kids) { arrange(k, cx, y, cw, g.el[k].dh, 0); y += g.el[k].dh + gap; } }
        else { int x = cx; for (int k : kids) { arrange(k, x, cy, g.el[k].dw, ch, 0); x += g.el[k].dw + gap; } }
        break; }
    case K_WRAP: {
        int gap = e.gap >= 0 ? P(e.gap) : P(8);
        int x = cx, y = cy, rowh = 0;
        for (int k : kids) {
            int w = g.el[k].dw, hh = g.el[k].dh;
            if (x > cx && x + w > cx + cw) { x = cx; y += rowh + gap; rowh = 0; }
            arrange(k, x, y, w, hh, 0); x += w + gap; rowh = std::max(rowh, hh);
        }
        break; }
    case K_GRID: {
        GridDef *gd = e.grid;
        int gap = e.gap >= 0 ? P(e.gap) : 0;
        int fixedW = 0, starW = 0; for (int i = 0; i < gd->nc; i++) { if (gd->ct[i] == 2) starW += gd->cv[i]; else fixedW += gd->cs[i]; }
        int fixedH = 0, starH = 0; for (int i = 0; i < gd->nr; i++) { if (gd->rt[i] == 2) starH += gd->rv[i]; else fixedH += gd->rs[i]; }
        int remW = std::max(0, cw - fixedW - gap * std::max(0, gd->nc - 1));
        int remH = std::max(0, ch - fixedH - gap * std::max(0, gd->nr - 1));
        for (int i = 0; i < gd->nc; i++) if (gd->ct[i] == 2) gd->cs[i] = starW > 0 ? remW * gd->cv[i] / starW : 0;
        for (int i = 0; i < gd->nr; i++) if (gd->rt[i] == 2) gd->rs[i] = starH > 0 ? remH * gd->rv[i] / starH : 0;
        int off = cx; for (int i = 0; i < gd->nc; i++) { gd->co[i] = off; off += gd->cs[i] + gap; }
        off = cy; for (int i = 0; i < gd->nr; i++) { gd->ro[i] = off; off += gd->rs[i] + gap; }
        for (int k : kids) {
            Element &c = g.el[k];
            int col = std::min(std::max(0, c.col), gd->nc - 1);
            int row = std::min(std::max(0, c.row), gd->nr - 1);
            int spanW = 0; for (int i = col; i < std::min(gd->nc, col + c.cspan); i++) spanW += gd->cs[i] + gap; spanW -= gap;
            int spanH = 0; for (int i = row; i < std::min(gd->nr, row + c.rspan); i++) spanH += gd->rs[i] + gap; spanH -= gap;
            arrange(k, gd->co[col], gd->ro[row], std::max(0, spanW), std::max(0, spanH), 0);
        }
        break; }
    case K_SCROLL: {
        if (!kids.empty()) {
            int contentH = g.el[kids[0]].dh;
            int maxs = std::max(0, contentH - ch);
            if (e.scroll < 0) e.scroll = 0; if (e.scroll > maxs) e.scroll = maxs;
            int sbw = contentH > ch ? P(10) : 0;
            arrange(kids[0], cx, cy - e.scroll, cw - sbw, g.el[kids[0]].dh, 0);
        }
        break; }
    case K_CANVAS: {
        for (int k : kids) arrange(k, cx + P(g.el[k].px), cy + P(g.el[k].py), g.el[k].dw, g.el[k].dh, 0);
        break; }
    case K_BORDER: {
        if (!kids.empty()) arrange(kids[0], cx, cy, cw, ch, 0);
        break; }
    case K_GROUP: {
        if (!kids.empty()) arrange(kids[0], cx, cy, cw, ch, 0);
        break; }
    case K_TABS: {
        int stripH = 0; int c = e.first;
        while (c) { if (g.el[c].kind == K_TAB) { int w, hh; text_size(wide(g.el[c].text.c_str()), eff_font(e), 0, 0, false, 0, &w, &hh); stripH = std::max(stripH, hh + P(14)); } c = g.el[c].next; }
        e.strip_h = stripH;
        int sel = e.ival, idx = 0, hx = e.ax; c = e.first;
        while (c) { if (g.el[c].kind == K_TAB) {
            int w, hh; text_size(wide(g.el[c].text.c_str()), eff_font(e), 0, 0, false, 0, &w, &hh);
            int iw = w + P(28);
            g.el[c].hdr_x = hx; g.el[c].hdr_w = iw; hx += iw;
            if (idx == sel) arrange(c, e.ax + P(e.pl), e.ay + stripH + P(e.pt), e.aw - P(e.pl) - P(e.pr), e.ah - stripH - P(e.pt) - P(e.pb), 0);
            idx++;
        } c = g.el[c].next; }
        break; }
    case K_TAB: {
        int y = cy; for (int k : kids) { arrange(k, cx, y, cw, g.el[k].dh, 0); y += g.el[k].dh; }
        break; }
    case K_MENUBAR: {
        int x = e.ax; int c = e.first;
        while (c) { if (g.el[c].kind == K_MENU) { int w, hh; text_size(wide(g.el[c].text.c_str()), eff_font(e), 0, 0, false, 0, &w, &hh); int iw = w + P(20); Element &mc = g.el[c]; mc.ax = x; mc.ay = e.ay; mc.aw = iw; mc.ah = e.ah; x += iw; } c = g.el[c].next; }
        break; }
    case K_TOOLBAR: {
        int gap = e.gap >= 0 ? P(e.gap) : P(4);
        int x = cx; for (int k : kids) { arrange(k, x, cy, g.el[k].dw, ch, 0); x += g.el[k].dw + gap; }
        break; }
    default:
        break;
    }
}

void layout_root() {
    int w = 0, h = 0; win_client_size(&w, &h);
    ensure_back(w, h);
    if (!valid(g.window)) return;
    // применить активную таблицу стилей
    style_apply_sheet(g.window);
    measure(g.window, w, h);
    place(g.window, 0, 0, w, h);
    // окно всегда занимает весь клиент
    g.el[g.window].ax = 0; g.el[g.window].ay = 0; g.el[g.window].aw = w; g.el[g.window].ah = h;
    arrange(g.window, 0, 0, w, h, 0);
    // модальное окно программы (вне обычного дерева) — по центру
    if (valid(g.modal)) {
        measure(g.modal, w, h);
        int dw = g.el[g.modal].dw, dh = g.el[g.modal].dh;
        arrange(g.modal, (w - dw) / 2, (h - dh) / 2, dw, dh, 0);
    }
}

} // namespace eli
