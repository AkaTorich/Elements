// Платформенный слой Windows: окно, GDI+, цикл сообщений, ввод, буфер кадра.
#include "el_internal.h"

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

using namespace Gdiplus;

namespace eli {

static const wchar_t *CLS = L"ElementsWindow";
static bool s_cls = false;

static int btn_from_wparam(WPARAM wp) {
    int b = 0;
    if (wp & MK_LBUTTON) b |= 1;
    if (wp & MK_RBUTTON) b |= 2;
    return b;
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_MOUSEMOVE:
        g.mx = (short)LOWORD(lp); g.my = (short)HIWORD(lp);
        g.dirty = true;
        return 0;
    case WM_LBUTTONDOWN: g.buttons |= 1; g.mx = (short)LOWORD(lp); g.my = (short)HIWORD(lp); SetCapture(hwnd); g.dirty = true; return 0;
    case WM_LBUTTONUP:   g.buttons &= ~1; g.mx = (short)LOWORD(lp); g.my = (short)HIWORD(lp); if (!(g.buttons)) ReleaseCapture(); g.dirty = true; return 0;
    case WM_RBUTTONDOWN: g.buttons |= 2; g.mx = (short)LOWORD(lp); g.my = (short)HIWORD(lp); SetCapture(hwnd); g.dirty = true; return 0;
    case WM_RBUTTONUP:   g.buttons &= ~2; g.mx = (short)LOWORD(lp); g.my = (short)HIWORD(lp); if (!(g.buttons)) ReleaseCapture(); g.dirty = true; return 0;
    case WM_MOUSEWHEEL:  g.wheel += GET_WHEEL_DELTA_WPARAM(wp) / 120; g.dirty = true; return 0;
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN: {
        KeyEv k; k.vk = (int)wp; k.ch = 0; k.down = true; k.mods = el_mods();
        g.keys.push_back(k); g.dirty = true;
        if (msg == WM_SYSKEYDOWN && wp == VK_F4) return DefWindowProc(hwnd, msg, wp, lp); // Alt+F4 → close
        return 0;
    }
    case WM_CHAR: {
        KeyEv k; k.vk = 0; k.ch = (wchar_t)wp; k.down = true; k.mods = el_mods();
        g.keys.push_back(k); g.dirty = true;
        return 0;
    }
    case WM_SIZE:
        g.dirty = true;
        return 0;
    case WM_WINDOWPOSCHANGED:
        win_apply_shape();
        g.dirty = true;
        break;                       // пусть отработает и DefWindowProc
    case WM_NCCALCSIZE:
        if (g.frameless && wp) {
            if (IsZoomed(hwnd)) {
                NCCALCSIZE_PARAMS *p = (NCCALCSIZE_PARAMS *)lp;
                HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
                MONITORINFO mi = { sizeof(mi) }; GetMonitorInfo(mon, &mi);
                p->rgrc[0] = mi.rcWork;
            }
            return 0;                // клиент занимает всё окно (своя рамка)
        }
        break;
    case WM_NCHITTEST: {
        if (!g.frameless) break;
        POINT pt = { (short)LOWORD(lp), (short)HIWORD(lp) };
        RECT wr; GetWindowRect(hwnd, &wr);
        int cx = pt.x - wr.left, cy = pt.y - wr.top;
        int w = wr.right - wr.left, h = wr.bottom - wr.top;
        int b = (int)(6 * g.dpi);
        if (g.resizable) {               // края окна тянутся для любой формы (у безрамочных — по непрозрачным краям)
            bool L = cx < b, R = cx >= w - b, T = cy < b, B = cy >= h - b;
            if (T && L) return HTTOPLEFT; if (T && R) return HTTOPRIGHT; if (B && L) return HTBOTTOMLEFT; if (B && R) return HTBOTTOMRIGHT;
            if (L) return HTLEFT; if (R) return HTRIGHT; if (T) return HTTOP; if (B) return HTBOTTOM;
        }
        if (valid(g.drag_region)) {
            Element &dr = g.el[g.drag_region];
            if (cx >= dr.ax && cx < dr.ax + dr.aw && cy >= dr.ay && cy < dr.ay + dr.ah) {
                int a = hit_test(g.window, cx, cy);
                int k = valid(a) ? g.el[a].kind : 0;
                bool ctrl = (k == K_BUTTON || k == K_TOOL || k == K_CHECK || k == K_RADIO || k == K_EDIT || k == K_MEMO ||
                             k == K_PASSWORD || k == K_COMBO || k == K_LIST || k == K_SLIDER || k == K_NUMBER || k == K_MENU ||
                             k == K_TAB || (k == K_PAINT && g.el[a].on_click));
                if (!ctrl) return HTCAPTION;
            }
        }
        return HTCLIENT;
    }
    case WM_GETMINMAXINFO: {
        MINMAXINFO *mm = (MINMAXINFO *)lp;
        if (g.win_minw > 0) mm->ptMinTrackSize.x = px(g.win_minw);
        if (g.win_minh > 0) mm->ptMinTrackSize.y = px(g.win_minh);
        if (g.win_maxw > 0) mm->ptMaxTrackSize.x = px(g.win_maxw);
        if (g.win_maxh > 0) mm->ptMaxTrackSize.y = px(g.win_maxh);
        if (g.frameless) {
            HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
            MONITORINFO mi = { sizeof(mi) }; GetMonitorInfo(mon, &mi);
            mm->ptMaxPosition.x = mi.rcWork.left - mi.rcMonitor.left;
            mm->ptMaxPosition.y = mi.rcWork.top - mi.rcMonitor.top;
            mm->ptMaxSize.x = mi.rcWork.right - mi.rcWork.left;
            mm->ptMaxSize.y = mi.rcWork.bottom - mi.rcWork.top;
        }
        return 0;
    }
    case WM_SIZING:
        if (g.aspect_w > 0 && g.aspect_h > 0) {
            RECT *r = (RECT *)lp;
            int w = r->right - r->left, h = r->bottom - r->top;
            if (wp == WMSZ_TOP || wp == WMSZ_BOTTOM) r->right = r->left + h * g.aspect_w / g.aspect_h;
            else r->bottom = r->top + w * g.aspect_h / g.aspect_w;
            return TRUE;
        }
        break;
    case WM_SETCURSOR:
        if (LOWORD(lp) == HTCLIENT) { SetCursor(LoadCursor(nullptr, IDC_ARROW)); return TRUE; }
        break;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc = BeginPaint(hwnd, &ps);
        if (g.memdc && !g.layered) BitBlt(dc, 0, 0, g.back_w, g.back_h, g.memdc, 0, 0, SRCCOPY);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_CLOSE:
        g.close_req = true; g.dirty = true;
        return 0;                 // закрытие решает el_frame
    case WM_DESTROY:
        g.quit = true;
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

void gdip_init() {
    // DPI-осознанность (если доступно)
    HMODULE u = GetModuleHandleW(L"user32.dll");
    if (u) {
        typedef BOOL(WINAPI *SetCtx)(DPI_AWARENESS_CONTEXT);
        SetCtx setctx = (SetCtx)GetProcAddress(u, "SetProcessDpiAwarenessContext");
        if (setctx) setctx(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        else { typedef BOOL(WINAPI *SetDA)(void); SetDA sda = (SetDA)GetProcAddress(u, "SetProcessDPIAware"); if (sda) sda(); }
    }
    GdiplusStartupInput si;
    GdiplusStartup(&g.gdip_token, &si, nullptr);
    g.measure_bmp = new Bitmap(4, 4, PixelFormat32bppPARGB);
    g.measure_g = new Graphics(g.measure_bmp);
    g.measure_g->SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
}

void win_create(const char *title, int w, int h) {
    HINSTANCE hi = GetModuleHandle(nullptr);
    if (!s_cls) {
        WNDCLASSEXW wc = { sizeof(wc) };
        wc.lpfnWndProc = WndProc;
        wc.hInstance = hi;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.lpszClassName = CLS;
        wc.hbrBackground = nullptr;
        wc.style = CS_HREDRAW | CS_VREDRAW;
        RegisterClassExW(&wc);
        s_cls = true;
    }
    // ищем DPI основного монитора заранее, чтобы задать физический размер
    HDC sdc = GetDC(nullptr); int dpiX = GetDeviceCaps(sdc, LOGPIXELSX); ReleaseDC(nullptr, sdc);
    g.dpi = dpiX / 96.0; if (g.dpi < 1.0) g.dpi = 1.0;

    RECT r = { 0, 0, (LONG)px(w), (LONG)px(h) };
    DWORD style = WS_OVERLAPPEDWINDOW;
    AdjustWindowRect(&r, style, FALSE);
    HWND hwnd = CreateWindowExW(0, CLS, wide(title).c_str(), style,
        CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top,
        nullptr, nullptr, hi, nullptr);
    g.hwnd = hwnd;
    // уточняем DPI окна
    HMODULE u = GetModuleHandleW(L"user32.dll");
    if (u) {
        typedef UINT(WINAPI *GetDpiW)(HWND);
        GetDpiW gd = (GetDpiW)GetProcAddress(u, "GetDpiForWindow");
        if (gd) { UINT d = gd(hwnd); if (d) g.dpi = d / 96.0; }
    }
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
}

void win_set_title(const std::wstring &t) { if (g.hwnd) SetWindowTextW(g.hwnd, t.c_str()); }

void win_client_size(int *w, int *h) {
    RECT rc; if (g.hwnd && GetClientRect(g.hwnd, &rc)) { if (w) *w = rc.right; if (h) *h = rc.bottom; }
    else { if (w) *w = 0; if (h) *h = 0; }
}

void ensure_back(int w, int h) {
    if (w < 1) w = 1; if (h < 1) h = 1;
    if (g.back && g.back_w == w && g.back_h == h) return;
    if (g.g) { delete g.g; g.g = nullptr; }
    if (g.back) { delete g.back; g.back = nullptr; }
    if (g.memdc) { DeleteDC(g.memdc); g.memdc = nullptr; }
    if (g.dib) { DeleteObject(g.dib); g.dib = nullptr; }
    BITMAPINFO bi = { 0 };
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w; bi.bmiHeader.biHeight = -h;   // сверху вниз
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    HDC sdc = GetDC(nullptr);
    g.dib = CreateDIBSection(sdc, &bi, DIB_RGB_COLORS, &g.dibbits, nullptr, 0);
    ReleaseDC(nullptr, sdc);
    g.memdc = CreateCompatibleDC(nullptr);
    SelectObject(g.memdc, g.dib);
    g.back = new Bitmap(w, h, w * 4, PixelFormat32bppPARGB, (BYTE *)g.dibbits);
    g.g = new Graphics(g.back);
    g.g->SetSmoothingMode(SmoothingModeAntiAlias);
    g.g->SetTextRenderingHint(g.layered ? TextRenderingHintAntiAlias : TextRenderingHintAntiAliasGridFit);
    g.g->SetPixelOffsetMode(PixelOffsetModeHalf);
    g.g->SetInterpolationMode(InterpolationModeHighQualityBicubic);
    g.back_w = w; g.back_h = h;
}

Graphics *G() { return g.g; }

void present() {
    if (!g.hwnd || !g.memdc) return;
    g.g->Flush();
    if (g.layered) {
        RECT wr; GetWindowRect(g.hwnd, &wr);
        POINT pos = { wr.left, wr.top };
        SIZE sz = { g.back_w, g.back_h };
        POINT src = { 0, 0 };
        BLENDFUNCTION bf = { AC_SRC_OVER, 0, (BYTE)(g.win_opacity < 0 ? 255 : g.win_opacity), AC_SRC_ALPHA };
        HDC dc = GetDC(nullptr);
        UpdateLayeredWindow(g.hwnd, dc, &pos, &sz, g.memdc, &src, 0, &bf, ULW_ALPHA);
        ReleaseDC(nullptr, dc);
    } else {
        HDC dc = GetDC(g.hwnd);
        BitBlt(dc, 0, 0, g.back_w, g.back_h, g.memdc, 0, 0, SRCCOPY);
        ReleaseDC(g.hwnd, dc);
    }
}

void win_pump() {
    MSG m;
    while (PeekMessageW(&m, nullptr, 0, 0, PM_REMOVE)) {
        if (m.message == WM_QUIT) { g.quit = true; break; }
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
}

void win_wait(int timeout_ms) {
    MsgWaitForMultipleObjectsEx(0, nullptr, timeout_ms, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
}

// --- клавиши окна ---
int el_mods(void) {
    int m = 0;
    if (GetKeyState(VK_SHIFT) & 0x8000) m |= EL_SHIFT;
    if (GetKeyState(VK_CONTROL) & 0x8000) m |= EL_CTRL;
    if (GetKeyState(VK_MENU) & 0x8000) m |= EL_ALT;
    return m;
}

static int parse_vk(const std::string &name, int *mods) {
    *mods = 0;
    std::string key;
    size_t i = 0;
    std::string s = name;
    // разбор "Ctrl+Shift+S"
    while (i < s.size()) {
        size_t plus = s.find('+', i);
        std::string part = (plus == std::string::npos) ? s.substr(i) : s.substr(i, plus - i);
        std::string low;
        for (char c : part) low += (char)tolower((unsigned char)c);
        if (low == "ctrl") *mods |= EL_CTRL;
        else if (low == "shift") *mods |= EL_SHIFT;
        else if (low == "alt") *mods |= EL_ALT;
        else key = part;
        if (plus == std::string::npos) break;
        i = plus + 1;
    }
    if (key.empty()) return 0;
    std::string low; for (char c : key) low += (char)tolower((unsigned char)c);
    if (low == "enter" || low == "return") return VK_RETURN;
    if (low == "esc" || low == "escape") return VK_ESCAPE;
    if (low == "space") return VK_SPACE;
    if (low == "tab") return VK_TAB;
    if (low == "up") return VK_UP;
    if (low == "down") return VK_DOWN;
    if (low == "left") return VK_LEFT;
    if (low == "right") return VK_RIGHT;
    if (low == "pgup") return VK_PRIOR;
    if (low == "pgdn") return VK_NEXT;
    if (low == "home") return VK_HOME;
    if (low == "end") return VK_END;
    if (low == "del" || low == "delete") return VK_DELETE;
    if (low.size() >= 2 && low[0] == 'f') { int n = atoi(low.c_str() + 1); if (n >= 1 && n <= 24) return VK_F1 + (n - 1); }
    if (key.size() == 1) { char c = (char)toupper((unsigned char)key[0]); return (int)c; }
    return 0;
}

int el_hotkey(const char *combo, el_fn fn, void *arg) {
    if (!combo) return 0;
    Hotkey h; h.vk = parse_vk(combo, &h.mods); h.fn = fn; h.arg = arg;
    if (!h.vk) return 0;
    g.hotkeys.push_back(h);
    return (int)g.hotkeys.size();
}

// --------------------------------------------- нестандартные окна ----------

static void set_exstyle(LONG add, LONG remove) {
    LONG ex = GetWindowLong(g.hwnd, GWL_EXSTYLE);
    ex = (ex & ~remove) | add;
    SetWindowLong(g.hwnd, GWL_EXSTYLE, ex);
}

void el_frameless(int on) {
    if (!g.hwnd) return;
    g.frameless = on != 0;
    LONG st = GetWindowLong(g.hwnd, GWL_STYLE);
    if (on) st = (st & ~WS_CAPTION) | WS_THICKFRAME | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_CLIPCHILDREN;
    else st = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN;
    SetWindowLong(g.hwnd, GWL_STYLE, st);
    SetWindowPos(g.hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    win_apply_shape();
    g.dirty = true;
}

void win_apply_shape() {
    if (!g.hwnd) return;
    if (g.layered) { SetWindowRgn(g.hwnd, nullptr, TRUE); return; }
    int w = 0, h = 0; win_client_size(&w, &h);
    if (w < 1 || h < 1) return;
    HRGN rgn = nullptr;
    switch (g.win_shape) {
    case 1: rgn = CreateRoundRectRgn(0, 0, w + 1, h + 1, px(g.win_round) * 2, px(g.win_round) * 2); break;
    case 2: rgn = CreateEllipticRgn(0, 0, w + 1, h + 1); break;
    case 3: {
        int n = (int)g.region_pts.size() / 2;
        if (n >= 3) { std::vector<POINT> pts(n); for (int i = 0; i < n; i++) { pts[i].x = px(g.region_pts[i * 2]); pts[i].y = px(g.region_pts[i * 2 + 1]); } rgn = CreatePolygonRgn(pts.data(), n, WINDING); }
        break; }
    default: rgn = nullptr; break;
    }
    SetWindowRgn(g.hwnd, rgn, TRUE);     // окно забирает владение rgn
}

void el_window_round(int radius) { g.win_shape = 1; g.win_round = radius; win_apply_shape(); g.dirty = true; }

void el_window_shape(const char *shape) {
    if (!shape) return;
    std::string s = shape;
    if (s.rfind("round", 0) == 0) { g.win_shape = 1; size_t c = s.find(':'); g.win_round = (c != std::string::npos) ? atoi(s.c_str() + c + 1) : 16; }
    else if (s == "ellipse") g.win_shape = 2;
    else if (s == "pixels") {
        g.win_shape = 4; g.layered = true;
        set_exstyle(WS_EX_LAYERED, 0);
        if (!g.frameless) el_frameless(1);
        if (g.g) g.g->SetTextRenderingHint(TextRenderingHintAntiAlias);
    } else g.win_shape = 0;
    win_apply_shape();
    g.dirty = true;
}

void el_window_region(const int *xy, int count) {
    g.region_pts.clear();
    for (int i = 0; i < count * 2; i++) g.region_pts.push_back(xy[i]);
    g.win_shape = 3;
    win_apply_shape();
    g.dirty = true;
}

void el_window_aspect(int w, int h) { g.aspect_w = w; g.aspect_h = h; }

void el_window_opacity(int alpha) {
    if (alpha < 0) alpha = 0; if (alpha > 255) alpha = 255;
    g.win_opacity = alpha;
    if (!g.layered) {                    // равномерная прозрачность без послойного окна
        set_exstyle(WS_EX_LAYERED, 0);
        SetLayeredWindowAttributes(g.hwnd, 0, (BYTE)alpha, LWA_ALPHA);
    }
    g.dirty = true;
}

void el_drag_region(el e) { if (valid(e)) g.drag_region = e; }

void el_window_minmax(int minw, int minh, int maxw, int maxh) {
    g.win_minw = minw; g.win_minh = minh; g.win_maxw = maxw; g.win_maxh = maxh;
}

void el_window_resizable(int on) {
    g.resizable = on != 0;
    if (!g.hwnd) return;
    LONG st = GetWindowLong(g.hwnd, GWL_STYLE);
    if (on) st |= WS_THICKFRAME | WS_MAXIMIZEBOX;
    else st &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
    SetWindowLong(g.hwnd, GWL_STYLE, st);
    SetWindowPos(g.hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
}

el el_window_canvas(void (*fn)(el, int, int, void *), void *arg) {
    int h = el_alloc(K_PAINT);
    g.el[h].paint_fn = fn; g.el[h].paint_arg = arg; g.el[h].needs_redraw = true;
    g.win_canvas = h;
    request_redraw();
    return h;
}

void win_move_begin() {
    if (!g.hwnd) return;
    ReleaseCapture();
    SendMessage(g.hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
}

} // namespace eli
