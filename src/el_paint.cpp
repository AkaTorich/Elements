// Отрисовка Elements на GDI+: фон/рамки/градиенты/тени/стекло, все элементы,
// трансформации (анимации) через слой, поверхность своего рисования (el_g_*).
#include "el_internal.h"
#include <algorithm>
#include <cmath>

using namespace Gdiplus;

namespace eli {

static inline int P(int d) { return px(d); }
static int eff_font(const Element &e) { return e.fsize > 0 ? e.fsize : g.def_font; }

static u32 eff_fg(const Element &e) {
    if (e.has_fg) return e.fg;
    return th(6);
}
static u32 panel_bg(const Element &e) { return th(1); }

void round_rect_path(GraphicsPath &p, RectF r, float rad) {
    p.Reset();
    if (rad <= 0.5f) { p.AddRectangle(r); return; }
    float d = rad * 2;
    if (d > r.Width) d = r.Width; if (d > r.Height) d = r.Height;
    p.AddArc(r.X, r.Y, d, d, 180, 90);
    p.AddArc(r.X + r.Width - d, r.Y, d, d, 270, 90);
    p.AddArc(r.X + r.Width - d, r.Y + r.Height - d, d, d, 0, 90);
    p.AddArc(r.X, r.Y + r.Height - d, d, d, 90, 90);
    p.CloseFigure();
}

static float radius_of(const Element &e) {
    int r = (e.radius >= 0) ? e.radius : g.corner;
    return (float)P(r);
}

static void draw_shadow(Graphics *gr, RectF r, float rad, const Element &e) {
    if (e.sh_a <= 0) return;
    int layers = 6;
    float blur = (float)P(e.sh_blur);
    for (int i = layers; i >= 1; i--) {
        float t = (float)i / layers;
        float grow = blur * t;
        int a = (int)(e.sh_a * (1.0f - t) * 0.5f / layers * 6);
        if (a <= 0) continue; if (a > 255) a = 255;
        RectF rr(r.X - grow + P(e.sh_dx), r.Y - grow + P(e.sh_dy), r.Width + grow * 2, r.Height + grow * 2);
        GraphicsPath p; round_rect_path(p, rr, rad + grow);
        SolidBrush b(gca(e.sh_col, a));
        gr->FillPath(&b, &p);
    }
}

static void fill_shape(Graphics *gr, GraphicsPath &path, RectF r, const Element &e, u32 bg) {
    int alpha = (e.bg_a > 0) ? e.bg_a : 255;
    if (e.grad) {
        LinearGradientMode mode = (e.grad == 2) ? LinearGradientModeHorizontal :
                                  (e.grad == 3) ? LinearGradientModeForwardDiagonal : LinearGradientModeVertical;
        LinearGradientBrush br(r, gca(e.bg, alpha), gca(e.bg2, alpha), mode);
        gr->FillPath(&br, &path);
    } else {
        SolidBrush br(gca(bg, alpha));
        gr->FillPath(&br, &path);
    }
}

// рисует фон+рамку в прямоугольнике r по стилю элемента, цвет фона bg
static void draw_box(Graphics *gr, RectF r, const Element &e, u32 bg, bool hasbg, u32 bd, bool hasbd) {
    float rad = radius_of(e);
    GraphicsPath path; round_rect_path(path, r, rad);
    if (e.sh_a > 0) draw_shadow(gr, r, rad, e);
    if (hasbg || e.grad) fill_shape(gr, path, r, e, bg);
    int bw = e.bw;
    if (bw < 0) bw = (hasbd ? 1 : 0);
    if (bw > 0 && hasbd) {
        Pen pen(gc(bd), (REAL)P(bw));
        pen.SetAlignment(PenAlignmentCenter);
        gr->DrawPath(&pen, &path);
    }
}

static void draw_text_in(Graphics *gr, RectF r, const std::wstring &s, u32 color, int size, int bold, int mono, int align, bool wrapW, bool vcenter) {
    if (s.empty()) return;
    Font *f = get_font(size, bold, mono);
    SolidBrush br(gc(color));
    StringFormat sf(StringFormat::GenericTypographic());
    sf.SetFormatFlags(sf.GetFormatFlags() | StringFormatFlagsMeasureTrailingSpaces);
    if (!wrapW) sf.SetFormatFlags(sf.GetFormatFlags() | StringFormatFlagsNoWrap);
    if (align == 1) sf.SetAlignment(StringAlignmentCenter);
    else if (align == 2) sf.SetAlignment(StringAlignmentFar);
    else sf.SetAlignment(StringAlignmentNear);
    sf.SetLineAlignment(vcenter ? StringAlignmentCenter : StringAlignmentNear);
    sf.SetTrimming(StringTrimmingEllipsisCharacter);
    gr->DrawString(s.c_str(), -1, f, r, &sf, &br);
}

// ---- трансформации (анимации) ----
static void draw_inner(int h);

static void draw_layered(int h, double tx, double ty, double sc, double rot, double op) {
    Element &e = g.el[h];
    int m = P(60);
    int lw = e.aw + 2 * m, lh = e.ah + 2 * m;
    if (lw < 1) lw = 1; if (lh < 1) lh = 1;
    Bitmap layer(lw, lh, PixelFormat32bppPARGB);
    Graphics lg(&layer);
    lg.SetSmoothingMode(SmoothingModeAntiAlias);
    lg.SetTextRenderingHint(TextRenderingHintAntiAlias);
    lg.SetPixelOffsetMode(PixelOffsetModeHalf);
    lg.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    Graphics *save = g.g;
    g.g = &lg;
    g.g->TranslateTransform((REAL)(m - e.ax), (REAL)(m - e.ay));
    draw_inner(h);
    g.g = save;

    float cx = (float)(e.ax + e.aw / 2.0), cy = (float)(e.ay + e.ah / 2.0);
    GraphicsState st = g.g->Save();
    g.g->TranslateTransform((REAL)(cx + px((int)tx)), (REAL)(cy + px((int)ty)));
    if (rot != 0) g.g->RotateTransform((REAL)rot);
    if (sc != 100) g.g->ScaleTransform((REAL)(sc / 100.0), (REAL)(sc / 100.0));
    g.g->TranslateTransform(-cx, -cy);
    ImageAttributes ia;
    if (op < 255) {
        ColorMatrix cm = { 1,0,0,0,0, 0,1,0,0,0, 0,0,1,0,0, 0,0,0,(REAL)(op / 255.0),0, 0,0,0,0,1 };
        ia.SetColorMatrix(&cm);
    }
    RectF dst((REAL)(e.ax - m), (REAL)(e.ay - m), (REAL)lw, (REAL)lh);
    g.g->DrawImage(&layer, dst, 0, 0, (REAL)lw, (REAL)lh, UnitPixel, &ia);
    g.g->Restore(st);
}

void draw_element(int h) {
    if (!valid(h)) return;
    Element &e = g.el[h];
    if (e.hidden) return;
    double tx, ty, sc, rot, op; transform_of(h, &tx, &ty, &sc, &rot, &op);
    if (op >= 255 && sc == 100 && rot == 0) {
        if (tx || ty) {
            GraphicsState st = g.g->Save();
            g.g->TranslateTransform((REAL)px((int)tx), (REAL)px((int)ty));
            draw_inner(h);
            g.g->Restore(st);
        } else draw_inner(h);
    } else {
        draw_layered(h, tx, ty, sc, rot, op);
    }
}

static void draw_children(int h) {
    int c = g.el[h].first;
    while (c) { if (g.el[c].alive && g.el[c].kind != K_MENU && g.el[c].kind != K_TAB) draw_element(c); c = g.el[c].next; }
}

static void paint_surface(int h) {
    Element &e = g.el[h];
    if (!e.paint_fn) return;
    Region old; g.g->GetClip(&old);
    g.g->SetClip(Rect(e.ax, e.ay, e.aw, e.ah));
    int sp = g.cur_paint, ox = g.paint_ox, oy = g.paint_oy;
    g.cur_paint = h; g.paint_ox = e.ax; g.paint_oy = e.ay;
    e.paint_fn(h, dip(e.aw), dip(e.ah), e.paint_arg);
    g.cur_paint = sp; g.paint_ox = ox; g.paint_oy = oy;
    g.g->SetClip(&old);
    e.needs_redraw = false;
}

static void draw_inner(int h) {
    Element &e = g.el[h];
    Graphics *gr = g.g;
    RectF r((REAL)e.ax, (REAL)e.ay, (REAL)e.aw, (REAL)e.ah);
    int fs = eff_font(e), bold = e.bold, mono = e.mono;
    bool hover = e.hv > 0.01;
    bool pressed = (g.active == h) && (g.buttons & 1);
    std::wstring t = e.live ? wide(e.live) : wide(e.text.c_str());

    switch (e.kind) {
    case K_WINDOW: {
        // фон окна: градиент/картинка/цвет из стиля, иначе тема
        if (e.has_bg || e.grad) draw_box(gr, r, e, e.bg, true, e.bd, false);
        if (g.win_canvas && valid(g.win_canvas)) { Element &pc = g.el[g.win_canvas]; pc.ax = e.ax; pc.ay = e.ay; pc.aw = e.aw; pc.ah = e.ah; paint_surface(g.win_canvas); }
        draw_children(h);
        break; }
    case K_STACK: case K_DOCK: case K_WRAP: case K_CANVAS: case K_TAB: case K_TOOLBAR: case K_GRID: {
        if (e.has_bg || e.grad) draw_box(gr, r, e, e.bg, true, e.bd, e.has_bd);
        else if (e.has_bd) draw_box(gr, r, e, 0, false, e.bd, true);
        draw_children(h);
        break; }
    case K_BORDER: {
        draw_box(gr, r, e, e.bg, e.has_bg, e.has_bd ? e.bd : th(5), e.has_bd || (e.bw > 0));
        draw_children(h);
        break; }
    case K_GROUP: {
        int hdrh; { int w; text_size(wide(e.text.c_str()), fs, 1, 0, false, 0, &w, &hdrh); }
        RectF fr(r.X, r.Y + hdrh / 2.0f, r.Width, r.Height - hdrh / 2.0f);
        GraphicsPath p; round_rect_path(p, fr, radius_of(e));
        Pen pen(gc(th(5)), 1.0f); gr->DrawPath(&pen, &p);
        SolidBrush tb(gc(th(8)));
        Font *f = get_font(fs, 1, 0);
        RectF tr(r.X + P(10), r.Y, r.Width - P(20), (REAL)hdrh + 2);
        StringFormat sf(StringFormat::GenericTypographic()); sf.SetFormatFlags(sf.GetFormatFlags() | StringFormatFlagsNoWrap);
        gr->DrawString(wide(e.text.c_str()).c_str(), -1, f, tr, &sf, &tb);
        draw_children(h);
        break; }
    case K_SCROLL: {
        Region old; gr->GetClip(&old);
        gr->SetClip(Rect(e.ax, e.ay, e.aw, e.ah));
        draw_children(h);
        gr->SetClip(&old);
        int c = e.first;
        if (c) {
            int contentH = g.el[c].dh, ch = e.ah;
            if (contentH > ch) {
                int trackx = e.ax + e.aw - P(9);
                SolidBrush tb(gca(th(8), 40)); gr->FillRectangle(&tb, trackx, e.ay, P(7), e.ah);
                int knob = std::max(P(24), ch * ch / contentH);
                int ky = e.ay + (e.scroll * (ch - knob)) / std::max(1, contentH - ch);
                GraphicsPath p; round_rect_path(p, RectF((REAL)trackx, (REAL)ky, (REAL)P(7), (REAL)knob), (REAL)P(3));
                SolidBrush kb(gc(th(8))); gr->FillPath(&kb, &p);
            }
        }
        break; }
    case K_TABS: {
        int stripH = e.strip_h, idx = 0, sel = e.ival, c = e.first;
        SolidBrush line(gc(th(5))); gr->FillRectangle(&line, e.ax, e.ay + stripH - P(1), e.aw, P(1));
        while (c) { if (g.el[c].kind == K_TAB) {
            Element &tb = g.el[c];
            bool on = (idx == sel), hot = (g.hot == c);
            if (on) { SolidBrush b(gc(th(1))); gr->FillRectangle(&b, tb.hdr_x, e.ay, tb.hdr_w, stripH);
                      SolidBrush acc(gc(th(9))); gr->FillRectangle(&acc, tb.hdr_x, e.ay + stripH - P(3), tb.hdr_w, P(3)); }
            else if (hot) { SolidBrush b(gca(th(6), 18)); gr->FillRectangle(&b, tb.hdr_x, e.ay, tb.hdr_w, stripH); }
            draw_text_in(gr, RectF((REAL)tb.hdr_x, (REAL)e.ay, (REAL)tb.hdr_w, (REAL)stripH), wide(tb.text.c_str()), on ? th(7) : th(8), fs, 0, 0, 1, false, true);
            idx++;
        } c = g.el[c].next; }
        idx = 0; c = e.first;
        while (c) { if (g.el[c].kind == K_TAB) { if (idx == sel) draw_children(c); idx++; } c = g.el[c].next; }
        break; }
    case K_MENUBAR: {
        SolidBrush bg(gc(th(1))); gr->FillRectangle(&bg, e.ax, e.ay, e.aw, e.ah);
        int c = e.first;
        while (c) { if (g.el[c].kind == K_MENU) { Element &mi = g.el[c];
            bool hot = g.hot == c || mi.menu_open;
            if (hot) { SolidBrush hb(gc(mi.menu_open ? th(4) : th(3))); gr->FillRectangle(&hb, mi.ax, mi.ay, mi.aw, mi.ah); }
            draw_text_in(gr, RectF((REAL)mi.ax, (REAL)mi.ay, (REAL)mi.aw, (REAL)mi.ah), wide(mi.text.c_str()), th(6), fs, 0, 0, 1, false, true);
        } c = g.el[c].next; }
        break; }
    case K_TEXT: {
        draw_text_in(gr, r, t, eff_fg(e), fs, bold, mono, e.talign, e.wrap, false);
        break; }
    case K_BUTTON: case K_TOOL: {
        u32 bg = e.has_bg ? e.bg : th(2);
        if (e.kind == K_TOOL && !e.has_bg) { if (!hover && !pressed) { /* плоская: без фона */ } }
        u32 base = bg;
        if (pressed) base = e.has_pbg ? e.pbg : th(4);
        else if (hover) base = e.has_hbg ? e.hbg : mix(bg, g.dark ? 0xffffff : 0x000000, 0.08 + 0.12 * e.hv);
        bool drawbg = !(e.kind == K_TOOL && !e.has_bg && !hover && !pressed);
        u32 bd = e.has_bd ? e.bd : th(5);
        bool hasbd = e.has_bd || (e.bw > 0);
        if (hover && e.has_hbd) { bd = e.hbd; hasbd = true; }
        if (drawbg) draw_box(gr, r, e, base, true, bd, hasbd || e.grad == 0 ? hasbd : false);
        // значок + текст
        int cx = e.ax, cw = e.aw;
        if (e.icon_img) {
            int is = P(e.icon_size > 0 ? e.icon_size : 20);
            int iy = e.ay + (e.ah - is) / 2;
            int ix = t.empty() ? e.ax + (e.aw - is) / 2 : e.ax + P(e.pl);
            gr->DrawImage(e.icon_img, ix, iy, is, is);
            if (!t.empty()) { cx = ix + is + P(6); cw = e.aw - (cx - e.ax) - P(e.pr); }
        }
        if (!t.empty()) {
            u32 fg = e.has_fg ? e.fg : (e.kind == K_TOOL ? th(6) : th(7));
            if (hover && e.has_hfg) fg = e.hfg;
            draw_text_in(gr, RectF((REAL)cx, (REAL)e.ay, (REAL)cw, (REAL)e.ah), t, fg, fs, bold ? bold : (e.kind == K_BUTTON ? 0 : 0), mono, e.icon_img && !t.empty() ? 0 : 1, false, true);
        }
        break; }
    case K_CHECK: case K_RADIO: {
        int box = P(18);
        int by = e.ay + (e.ah - box) / 2;
        int on = e.iv ? (e.kind == K_CHECK ? (*e.iv != 0) : (*e.iv == e.ival)) : 0;
        RectF br((REAL)e.ax, (REAL)by, (REAL)box, (REAL)box);
        if (e.kind == K_RADIO) {
            Pen pen(gc(on ? th(9) : th(5)), 1.5f); gr->DrawEllipse(&pen, br);
            if (on) { SolidBrush b(gc(th(9))); gr->FillEllipse(&b, br.X + P(4), br.Y + P(4), (REAL)(box - P(8)), (REAL)(box - P(8))); }
        } else {
            GraphicsPath p; round_rect_path(p, br, (REAL)P(3));
            if (on) { SolidBrush b(gc(th(9))); gr->FillPath(&b, &p); Pen w(gc(th(10)), 2.0f);
                      Point chk[3] = { Point(e.ax + P(4), by + P(9)), Point(e.ax + P(8), by + P(13)), Point(e.ax + P(14), by + P(5)) };
                      gr->DrawLines(&w, chk, 3); }
            else { Pen pen(gc(th(5)), 1.5f); gr->DrawPath(&pen, &p); }
        }
        if (!t.empty()) draw_text_in(gr, RectF((REAL)(e.ax + box + P(8)), (REAL)e.ay, (REAL)(e.aw - box - P(8)), (REAL)e.ah), t, eff_fg(e), fs, bold, mono, 0, false, true);
        break; }
    case K_EDIT: case K_PASSWORD: case K_MEMO: {
        edit_draw(h);
        break; }
    case K_COMBO: case K_NUMBER: {
        u32 bg = e.has_bg ? e.bg : th(11);
        draw_box(gr, r, e, bg, true, th(5), true);
        std::wstring val;
        if (e.kind == K_COMBO) { int s = e.iv ? *e.iv : 0; if (e.items && s >= 0 && s < e.count) val = wide(e.items[s]); }
        else { wchar_t b[24]; wsprintfW(b, L"%d", e.iv ? *e.iv : 0); val = b; }
        draw_text_in(gr, RectF((REAL)(e.ax + P(e.pl)), (REAL)e.ay, (REAL)(e.aw - P(e.pl) - P(e.pr)), (REAL)e.ah), val, eff_fg(e), fs, bold, mono, 0, false, true);
        // стрелка(и)
        if (e.kind == K_COMBO) {
            int ax = e.ax + e.aw - P(18), ay = e.ay + e.ah / 2;
            Pen pen(gc(th(8)), 1.6f);
            Point dn[3] = { Point(ax - P(4), ay - P(2)), Point(ax, ay + P(3)), Point(ax + P(4), ay - P(2)) };
            gr->DrawLines(&pen, dn, 3);
        } else {
            int ax = e.ax + e.aw - P(16), ay = e.ay + e.ah / 2;
            Pen pen(gc(th(8)), 1.6f);
            Point up[3] = { Point(ax - P(4), ay - P(1)), Point(ax, ay - P(5)), Point(ax + P(4), ay - P(1)) };
            gr->DrawLines(&pen, up, 3);
            Point dn[3] = { Point(ax - P(4), ay + P(2)), Point(ax, ay + P(6)), Point(ax + P(4), ay + P(2)) };
            gr->DrawLines(&pen, dn, 3);
        }
        break; }
    case K_LIST: {
        u32 bg = e.has_bg ? e.bg : th(11);
        draw_box(gr, r, e, bg, true, th(5), true);
        Region old; gr->GetClip(&old); gr->SetClip(Rect(e.ax, e.ay, e.aw, e.ah));
        int rh; { int w; text_size(L"Ag", fs, 0, 0, false, 0, &w, &rh); } rh += P(8);
        int y = e.ay + P(e.pt) - e.scroll;
        int sel = e.iv ? *e.iv : -1;
        for (int i = 0; i < e.count; i++) {
            if (y + rh > e.ay && y < e.ay + e.ah) {
                if (i == sel) { SolidBrush b(gc(th(9))); GraphicsPath p; round_rect_path(p, RectF((REAL)(e.ax + P(3)), (REAL)y, (REAL)(e.aw - P(6)), (REAL)rh), (REAL)P(4)); gr->FillPath(&b, &p); }
                else if (g.hot == h && g.my >= y && g.my < y + rh) { SolidBrush b(gca(th(6), 24)); gr->FillRectangle(&b, e.ax + P(3), y, e.aw - P(6), rh); }
                draw_text_in(gr, RectF((REAL)(e.ax + P(10)), (REAL)y, (REAL)(e.aw - P(16)), (REAL)rh), wide(e.items[i]), i == sel ? th(10) : th(6), fs, 0, mono, 0, false, true);
            }
            y += rh;
        }
        gr->SetClip(&old);
        break; }
    case K_SLIDER: {
        int cy = e.ay + e.ah / 2;
        int tl = e.ax + P(8), tr = e.ax + e.aw - P(8);
        SolidBrush tb(gc(th(5))); GraphicsPath tp; round_rect_path(tp, RectF((REAL)tl, (REAL)(cy - P(2)), (REAL)(tr - tl), (REAL)P(4)), (REAL)P(2)); gr->FillPath(&tb, &tp);
        int v = e.iv ? *e.iv : 0; if (v < e.vmin) v = e.vmin; if (v > e.vmax) v = e.vmax;
        double tfrac = (e.vmax > e.vmin) ? (double)(v - e.vmin) / (e.vmax - e.vmin) : 0;
        int kx = tl + (int)((tr - tl) * tfrac);
        SolidBrush fb(gc(th(9))); GraphicsPath fp; round_rect_path(fp, RectF((REAL)tl, (REAL)(cy - P(2)), (REAL)(kx - tl), (REAL)P(4)), (REAL)P(2)); gr->FillPath(&fb, &fp);
        SolidBrush kb(gc(th(9))); gr->FillEllipse(&kb, kx - P(8), cy - P(8), P(16), P(16));
        SolidBrush kin(gc(th(10))); gr->FillEllipse(&kin, kx - P(4), cy - P(4), P(8), P(8));
        break; }
    case K_PROGRESS: {
        u32 fg = e.has_fg ? e.fg : th(9);
        GraphicsPath bp; round_rect_path(bp, r, (REAL)(e.ah / 2.0f)); SolidBrush bb(gc(th(5))); gr->FillPath(&bb, &bp);
        int v = e.iv ? *e.iv : 0; if (v < 0) v = 0; if (v > e.vmax) v = e.vmax;
        double f = e.vmax > 0 ? (double)v / e.vmax : 0;
        if (f > 0) { RectF fr(r.X, r.Y, (REAL)(r.Width * f), r.Height); GraphicsPath fp; round_rect_path(fp, fr, (REAL)(e.ah / 2.0f)); SolidBrush fb(gc(fg)); gr->FillPath(&fb, &fp); }
        break; }
    case K_SEP: {
        SolidBrush b(gc(th(5)));
        if (e.aw > e.ah) gr->FillRectangle(&b, e.ax, e.ay + e.ah / 2, e.aw, P(1));
        else gr->FillRectangle(&b, e.ax + e.aw / 2, e.ay, P(1), e.ah);
        break; }
    case K_IMAGE: {
        if (e.img) {
            int iw = e.img->GetWidth(), ih = e.img->GetHeight();
            double s = std::min((double)e.aw / iw, (double)e.ah / ih);
            int dw = (int)(iw * s), dh = (int)(ih * s);
            int dx = e.ax + (e.aw - dw) / 2, dy = e.ay + (e.ah - dh) / 2;
            gr->DrawImage(e.img, dx, dy, dw, dh);
        }
        break; }
    case K_PAINT: {
        paint_surface(h);
        break; }
    default: break;
    }

    if (e.disabled && e.kind != K_WINDOW) {
        SolidBrush veil(gca(th(0), 110)); gr->FillRectangle(&veil, e.ax, e.ay, e.aw, e.ah);
    }
}

void draw_tree() {
    Graphics *gr = g.g;
    Element &w = g.el[g.window];
    gr->ResetTransform();
    gr->ResetClip();
    if (g.layered) gr->Clear(Color(0, 0, 0, 0));
    else gr->Clear(gc(th(0)));
    draw_element(g.window);
}

// ----------------------------------------------------------- el_g_* --------

static Graphics *PG() { return g.g; }
static int OX() { return g.paint_ox; }
static int OY() { return g.paint_oy; }

void el_g_fill(int x, int y, int w, int h, int rgb) {
    SolidBrush b(gc((u32)rgb)); PG()->FillRectangle(&b, OX() + px(x), OY() + px(y), px(w), px(h));
}
void el_g_round(int x, int y, int w, int h, int radius, int rgb) {
    GraphicsPath p; round_rect_path(p, RectF((REAL)(OX() + px(x)), (REAL)(OY() + px(y)), (REAL)px(w), (REAL)px(h)), (REAL)px(radius));
    SolidBrush b(gc((u32)rgb)); PG()->FillPath(&b, &p);
}
void el_g_frame(int x, int y, int w, int h, int radius, int rgb, int width) {
    GraphicsPath p; round_rect_path(p, RectF((REAL)(OX() + px(x)), (REAL)(OY() + px(y)), (REAL)px(w), (REAL)px(h)), (REAL)px(radius));
    Pen pen(gc((u32)rgb), (REAL)px(width ? width : 1)); PG()->DrawPath(&pen, &p);
}
void el_g_line(int x0, int y0, int x1, int y1, int rgb, int width) {
    Pen pen(gc((u32)rgb), (REAL)px(width ? width : 1)); pen.SetStartCap(LineCapRound); pen.SetEndCap(LineCapRound);
    PG()->DrawLine(&pen, OX() + px(x0), OY() + px(y0), OX() + px(x1), OY() + px(y1));
}
void el_g_disc(int cx, int cy, int rad, int rgb) {
    SolidBrush b(gc((u32)rgb)); PG()->FillEllipse(&b, OX() + px(cx) - px(rad), OY() + px(cy) - px(rad), px(rad) * 2, px(rad) * 2);
}
void el_g_ring(int cx, int cy, int rad, int width, int rgb) {
    Pen pen(gc((u32)rgb), (REAL)px(width ? width : 1)); PG()->DrawEllipse(&pen, OX() + px(cx) - px(rad), OY() + px(cy) - px(rad), px(rad) * 2, px(rad) * 2);
}
void el_g_shade(int x, int y, int w, int h, int rgb, int alpha) {
    SolidBrush b(gca((u32)rgb, alpha)); PG()->FillRectangle(&b, OX() + px(x), OY() + px(y), px(w), px(h));
}
void el_g_text(int x, int y, const char *s, int rgb, int size, int bold) {
    if (!s || !*s) return;
    Font *f = get_font(size > 0 ? size : g.def_font, bold, 0);
    SolidBrush br(gc((u32)rgb));
    StringFormat sf(StringFormat::GenericTypographic()); sf.SetFormatFlags(sf.GetFormatFlags() | StringFormatFlagsNoWrap | StringFormatFlagsMeasureTrailingSpaces);
    PG()->DrawString(wide(s).c_str(), -1, f, PointF((REAL)(OX() + px(x)), (REAL)(OY() + px(y))), &sf, &br);
}
int el_g_text_w(const char *s, int size, int bold) { return dip(text_w(wide(s), size > 0 ? size : g.def_font, bold, 0)); }
void el_g_poly(const int *xy, int count, int rgb) {
    if (count < 3) return;
    std::vector<PointF> pts(count);
    for (int i = 0; i < count; i++) pts[i] = PointF((REAL)(OX() + px(xy[i * 2])), (REAL)(OY() + px(xy[i * 2 + 1])));
    GraphicsPath p; p.AddPolygon(pts.data(), count);
    SolidBrush b(gc((u32)rgb)); PG()->FillPath(&b, &p);
}
void el_g_poly_frame(const int *xy, int count, int rgb, int width) {
    if (count < 2) return;
    std::vector<PointF> pts(count);
    for (int i = 0; i < count; i++) pts[i] = PointF((REAL)(OX() + px(xy[i * 2])), (REAL)(OY() + px(xy[i * 2 + 1])));
    GraphicsPath p; p.AddPolygon(pts.data(), count);
    Pen pen(gc((u32)rgb), (REAL)px(width ? width : 1)); pen.SetLineJoin(LineJoinRound);
    PG()->DrawPath(&pen, &p);
}
void el_g_spline(const int *xy, int count, int rgb, int width) {
    if (count < 2) return;
    std::vector<PointF> pts(count);
    for (int i = 0; i < count; i++) pts[i] = PointF((REAL)(OX() + px(xy[i * 2])), (REAL)(OY() + px(xy[i * 2 + 1])));
    Pen pen(gc((u32)rgb), (REAL)px(width ? width : 1)); pen.SetStartCap(LineCapRound); pen.SetEndCap(LineCapRound);
    PG()->DrawCurve(&pen, pts.data(), count, 0.5f);
}
void el_g_blob(const int *xy, int count, int rgb) {
    if (count < 3) return;
    std::vector<PointF> pts(count);
    for (int i = 0; i < count; i++) pts[i] = PointF((REAL)(OX() + px(xy[i * 2])), (REAL)(OY() + px(xy[i * 2 + 1])));
    SolidBrush b(gc((u32)rgb)); PG()->FillClosedCurve(&b, pts.data(), count);
}
void el_g_arc(int cx, int cy, int r, int start_deg, int sweep_deg, int rgb, int width) {
    Pen pen(gc((u32)rgb), (REAL)px(width ? width : 1)); pen.SetStartCap(LineCapRound); pen.SetEndCap(LineCapRound);
    PG()->DrawArc(&pen, OX() + px(cx) - px(r), OY() + px(cy) - px(r), px(r) * 2, px(r) * 2, (REAL)start_deg, (REAL)sweep_deg);
}

// ----------------------------------------------------- мышь на поверхности --
int el_mouse(el e, int *x, int *y, int *buttons) {
    if (!valid(e)) return 0;
    Element &el = g.el[e];
    if (x) *x = dip(g.mx - el.ax); if (y) *y = dip(g.my - el.ay);
    if (buttons) *buttons = g.buttons;
    return (g.mx >= el.ax && g.mx < el.ax + el.aw && g.my >= el.ay && g.my < el.ay + el.ah) ? 1 : 0;
}
int el_wheel(el e, int *mods) {
    if (!valid(e)) return 0;
    Element &el = g.el[e];
    bool over = (g.mx >= el.ax && g.mx < el.ax + el.aw && g.my >= el.ay && g.my < el.ay + el.ah);
    if (mods) *mods = el_mods();
    if (over && g.wheel) { int w = g.wheel; g.wheel = 0; return w; }
    return 0;
}

} // namespace eli
