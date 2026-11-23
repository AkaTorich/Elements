# Оси букв: латиница и кириллица (заглавные и строчные), цифры, знаки.
# Координаты — в единицах шрифта (1000 на кегль), ноль — линия строки, ось Y вверх.
# Буква рисуется по оси штриха; толщину, контраст, засечки, скругления и ширину задаёт стиль.
# Высоты берутся из стиля: CAP (заглавные), XH (строчные), ASC и DESC (выносные).
# Строчная кириллица в прямом начертании — уменьшенные заглавные (в, г, д, ж, и, к, л, м, н, п, т…),
# кроме а, е, о, р, с, у, х (как латинские), б и ф (свои формы).
import math
import pen as P


class G:
    def __init__(self, st, W, H):
        self.st = st
        self.nib = st.nib
        self.W = float(W)
        self.H = float(H)
        self.CAP, self.XH, self.ASC, self.DESC = st.CAP, st.XH, st.ASC, st.DESC
        self.vh = self.nib.vertical / 2.0
        self.hh = self.nib.horizontal / 2.0
        self.ov = st.ov * self.H
        self.cap = "round" if st.round else "flat"
        self.out = []

    # ── опорные места по оси ──
    @property
    def l(self):
        return self.vh

    @property
    def r(self):
        return self.W - self.vh

    @property
    def m(self):
        return self.W / 2.0

    @property
    def b(self):
        return self.hh

    @property
    def t(self):
        return self.H - self.hh

    @property
    def e(self):
        """Отступ конца штриха от линии при круглых концах (чтобы круг не вылезал за линию)."""
        return self.vh if (self.cap == "round" and not self.st.serif) else 0.0

    # ── кривые ──
    def arc(self, cx, cy, rx, ry, a0, a1, n=None):
        e = self.st.sq
        if n is None:
            n = max(6, int(abs(a1 - a0) / 5))
        pts = []
        for i in range(n + 1):
            a = math.radians(a0 + (a1 - a0) * i / n)
            c, s = math.cos(a), math.sin(a)
            pts.append((cx + rx * math.copysign(abs(c) ** (2.0 / e), c),
                        cy + ry * math.copysign(abs(s) ** (2.0 / e), s)))
        return pts

    # ── штрихи ──
    def curve(self, pts):
        self.out += P.stroke(pts, self.nib, self.cap)

    def line(self, *pts):
        self.out += P.stroke(list(pts), self.nib, self.cap)

    def loop(self, pts):
        self.out += P.loop(pts, self.nib)

    def oval(self, x0, y0, x1, y1):
        cx, cy, rx, ry = (x0 + x1) / 2, (y0 + y1) / 2, (x1 - x0) / 2, (y1 - y0) / 2
        self.loop(self.arc(cx, cy, rx, ry, 90, 450, 72)[:-1])

    def poly(self, pts):
        self.out += P.polygon(pts)

    def stem(self, x, y0, y1, bot="both", top="both", ktop=1.0):
        a, b = y0, y1
        if self.cap == "round" and not self.st.serif:
            a, b = y0 + self.vh, y1 - self.vh
        self.out += P.stroke([(x, a), (x, b)], self.nib, self.cap)
        if self.st.serif:
            if bot:
                self.serif(x, self.vh, y0, True, bot)
            if top:
                self.serif(x, self.vh, y1, False, top, ktop)

    def bar(self, x0, y, x1):
        self.out += P.stroke([(x0, y), (x1, y)], self.nib, "flat" if self.st.serif else self.cap)

    def arm(self, x0, y, x1, beak=None):
        self.bar(x0, y, x1)
        if self.st.serif and beak:
            self.beak(x1, y, beak)

    def slant(self, x0, y0, x1, y1, c0="h", c1="h", s0=None, s1=None, thick=False):
        d = P.unit((x0, y0), (x1, y1))
        w = self.nib.vertical if thick else self.nib.width(d)
        h = w / 2.0
        n = (-d[1], d[0])

        def ends(px, py, c):
            q1 = (px + n[0] * h, py + n[1] * h)
            q2 = (px - n[0] * h, py - n[1] * h)
            if c == "h" and abs(d[1]) > 0.2:
                return [(q[0] + d[0] * (py - q[1]) / d[1], py) for q in (q1, q2)]
            if c == "v" and abs(d[0]) > 0.2:
                return [(px, q[1] + d[1] * (px - q[0]) / d[0]) for q in (q1, q2)]
            return [q1, q2]

        a = ends(x0, y0, c0)
        b = ends(x1, y1, c1)
        self.out += P.polygon([a[0], b[0], b[1], a[1]])
        if self.st.serif:
            for (px, py, e, s) in ((x0, y0, a, s0), (x1, y1, b, s1)):
                if s:
                    half = abs(e[0][0] - e[1][0]) / 2.0
                    up = py <= min(y0, y1) + 1
                    self.serif(px, half, py, up, s, 0.7)

    def dot(self, x, y, k=1.0):
        r = max(self.nib.vertical, self.nib.horizontal) * 0.6 * k
        if self.st.dot == "square":
            self.poly([(x - r, y - r), (x + r, y - r), (x + r, y + r), (x - r, y + r)])
        else:
            self.out += P.disc(x, y, r * 1.05)

    def dotr(self, k=1.0):
        return max(self.nib.vertical, self.nib.horizontal) * 0.6 * k

    # ── засечки ──
    def serif(self, x, half, y, up, side, k=1.0):
        st = self.st
        kind = st.serif
        L = st.serif_len * k * (self.H / self.CAP) ** 0.5
        th = max(self.nib.horizontal, st.serif_th * self.nib.vertical)
        if kind == "slab":
            th = max(th, 0.75 * self.nib.vertical)
        if kind == "hair":
            th = max(self.nib.horizontal, 12.0)
        x0 = x - half - (L if side in ("both", "left") else 0)
        x1 = x + half + (L if side in ("both", "right") else 0)
        sg = 1 if up else -1
        ya, yb = y, y + sg * th
        if kind in ("slab", "hair"):
            self.poly([(x0, ya), (x1, ya), (x1, yb), (x0, yb)])
            return
        br = th * (1.6 if kind == "wedge" else 1.25)
        xl, xr = x - half, x + half
        pts = [(x0, ya), (x1, ya), (x1, yb)]
        if side in ("both", "right"):
            pts += [(xr + (x1 - xr) * (1 - t) ** 2, yb + sg * br * t * t) for t in (0.25, 0.5, 0.75)] if kind == "bracket" else []
            pts.append((xr, yb + sg * br))
        else:
            pts.append((xr, yb))
        if side in ("both", "left"):
            pts.append((xl, yb + sg * br))
            pts += [(xl - (xl - x0) * (1 - t) ** 2, yb + sg * br * t * t) for t in (0.75, 0.5, 0.25)] if kind == "bracket" else []
        else:
            pts.append((xl, yb))
        pts.append((x0, yb))
        self.poly(pts)

    def beak(self, x, y, kind):
        st = self.st
        th = max(self.nib.horizontal, 0.55 * self.nib.vertical)
        if st.serif == "slab":
            th = 0.8 * self.nib.vertical
        if st.serif == "hair":
            th = max(self.nib.horizontal * 1.2, 14.0)
        ln = 0.17 * self.H + self.hh
        for sg in ((-1,) if kind == "down" else (1,) if kind == "up" else (-1, 1)):
            y0 = y - sg * self.hh
            self.poly([(x - th, y0), (x, y0), (x, y0 + sg * ln), (x - th * 0.6, y0 + sg * ln)])

    # ── составные части ──
    def bowl_r(self, x0, yt, yb, xr, k=1.0):
        """Петля вправо от стойки: верхняя перекладина, полукруг справа, нижняя перекладина."""
        ry = (yt - yb) / 2.0
        cy = (yt + yb) / 2.0
        rx = min((xr - x0) * 0.62, ry * 1.25 * k)
        cx = xr - rx
        self.curve([(x0, yt)] + self.arc(cx, cy, rx, ry, 90, -90) + [(x0, yb)])

    def bowl_l(self, x0, yt, yb, xl):
        ry = (yt - yb) / 2.0
        cy = (yt + yb) / 2.0
        rx = min((x0 - xl) * 0.62, ry * 1.25)
        cx = xl + rx
        self.curve([(x0, yt)] + self.arc(cx, cy, rx, ry, 90, 270) + [(x0, yb)])

    def da(self, x0, y0, x1, y1):
        """Полуширина наклонного штриха по горизонтали (чтобы внешний край лёг на край буквы)."""
        d = P.unit((x0, y0), (x1, y1))
        return self.nib.width(d) / 2.0 / max(abs(d[1]), 0.25)


# ════════════════════════════ ЗАГЛАВНЫЕ ════════════════════════════

def A(g):
    W, H, m = g.W, g.H, g.m
    a = g.da(0, 0, m, H)
    g.slant(a, 0, m, H, s0="both")
    g.slant(m, H, W - a, 0, s1="both")
    y = 0.28 * H
    xl = a + (m - a) * y / H
    xr = (W - a) - ((W - a) - m) * y / H
    g.bar(xl, y, xr)


def B(g):
    l, r, H = g.l, g.r, g.H
    mid = 0.53 * H
    g.stem(l, 0, H, bot="left", top="left")
    g.bowl_r(l, g.t, mid, r - 0.07 * g.W)
    g.bowl_r(l, mid, g.b, r)


def C(g):
    o = g.ov
    g.curve(g.arc(g.m + 0.01 * g.W, g.H / 2, (g.r - g.l) / 2, (g.t - g.b) / 2 + o, 44, 316))


def D(g):
    l, r, H = g.l, g.r, g.H
    g.stem(l, 0, H, bot="left", top="left")
    g.bowl_r(l, g.t, g.b, r, 1.35)


def E(g):
    l, H, W = g.l, g.H, g.W
    g.stem(l, 0, H, bot="left", top="left")
    g.arm(l, g.t, W, "down")
    g.arm(l, 0.51 * H, 0.86 * W, "both")
    g.arm(l, g.b, W, "up")


def F(g):
    l, H, W = g.l, g.H, g.W
    g.stem(l, 0, H, bot="both", top="left")
    g.arm(l, g.t, W, "down")
    g.arm(l, 0.5 * H, 0.84 * W, "both")


def G_(g):
    l, r, H, W = g.l, g.r, g.H, g.W
    o = g.ov
    cx, cy = g.m, H / 2
    rx, ry = (r - l) / 2, (g.t - g.b) / 2 + o
    gy = 0.44 * H
    g.curve(g.arc(cx, cy, rx, ry, 44, 360) + [(r, gy)])
    g.bar(0.54 * W, gy, r)
    if g.st.serif:
        g.stem(r, 0.05 * H, gy, bot=None, top=None)


def H_(g):
    l, r, H = g.l, g.r, g.H
    g.stem(l, 0, H)
    g.stem(r, 0, H)
    g.bar(l, 0.5 * H, r)


def I_(g):
    g.stem(g.m, 0, g.H)
    if g.st.mono:
        g.bar(g.m - 0.32 * g.st.mono_w, g.b, g.m + 0.32 * g.st.mono_w)
        g.bar(g.m - 0.32 * g.st.mono_w, g.t, g.m + 0.32 * g.st.mono_w)


def J(g):
    l, r, H = g.l, g.r, g.H
    rx = (r - l) / 2
    ry = min(rx * 1.05, 0.34 * H)
    cy = g.b - g.ov + ry
    g.curve([(r, H - g.e)] + g.arc(r - rx, cy, rx, ry, 0, -168))
    if g.st.serif:
        g.serif(r, g.vh, H, False, "both")


def K(g):
    l, H, W = g.l, g.H, g.W
    g.stem(l, 0, H)
    a = g.da(W, H, l, 0.36 * H)
    x0, y0, x1, y1 = W - a, H, l, 0.34 * H
    g.slant(x0, y0, x1, y1, "h", "v", s0="both")
    t = 0.42
    jx, jy = x0 + (x1 - x0) * t, y0 + (y1 - y0) * t
    a2 = g.da(jx, jy, W, 0)
    g.slant(jx, jy, W - a2, 0, "p", "h", s1="both")


def L(g):
    l, H, W = g.l, g.H, g.W
    g.stem(l, 0, H, bot="left", top="both")
    g.arm(l, g.b, W, "up")


def M(g):
    l, r, H, W, m = g.l, g.r, g.H, g.W, g.m
    g.stem(l, 0, H, top="left")
    g.stem(r, 0, H, top="right")
    a = g.da(0, H, m, 0)
    g.slant(a, H, m, 0)
    g.slant(m, 0, W - a, H)


def N(g):
    l, r, H, W = g.l, g.r, g.H, g.W
    g.stem(l, 0, H, top="left")
    g.stem(r, 0, H, bot=None)
    a = g.da(0, H, W, 0)
    g.slant(a, H, W - a, 0)


def O(g):
    o = g.ov
    g.oval(g.l, g.b - o, g.r, g.t + o)


def P_(g):
    l, r, H = g.l, g.r, g.H
    g.stem(l, 0, H, top="left")
    g.bowl_r(l, g.t, 0.46 * H, r)


def Q(g):
    O(g)
    W, H = g.W, g.H
    g.slant(0.56 * W, 0.2 * H, 0.98 * W, -0.1 * H, "p", "h")


def R(g):
    l, r, H, W = g.l, g.r, g.H, g.W
    g.stem(l, 0, H, top="left")
    mid = 0.47 * H
    g.bowl_r(l, g.t, mid, r - 0.04 * W)
    jx = l + (r - l) * 0.48
    a = g.da(jx, mid, W, 0)
    g.slant(jx, mid, W - a, 0, "p", "h", s1="both")


def S(g):
    l, r, H, m = g.l, g.r, g.H, g.m
    o = g.ov
    mid = 0.525 * H
    ry1 = (g.t + o - mid) / 2
    ry2 = (mid - (g.b - o)) / 2
    rx2 = (r - l) / 2
    rx1 = rx2 * 0.93
    g.curve(g.arc(m, mid + ry1, rx1, ry1, 32, 270) + g.arc(m, mid - ry2, rx2, ry2, 90, -148)[1:])


def T(g):
    H, W, m = g.H, g.W, g.m
    g.arm(m, g.t, 0, "down")
    g.arm(m, g.t, W, "down")
    g.stem(m, 0, H, top=None)


def U_lat(g):
    l, r, H = g.l, g.r, g.H
    rx = (r - l) / 2
    ry = min(rx * 1.02, 0.42 * H)
    cy = g.b - g.ov + ry
    g.curve([(l, H - g.e), (l, cy)] + g.arc(g.m, cy, rx, ry, 180, 360) + [(r, H - g.e)])
    if g.st.serif:
        g.serif(l, g.vh, H, False, "both")
        g.serif(r, g.vh, H, False, "both")


def V(g):
    W, H, m = g.W, g.H, g.m
    a = g.da(0, H, m, 0)
    g.slant(a, H, m, 0, s0="both")
    g.slant(m, 0, W - a, H, s1="both")


def W_(g):
    W, H = g.W, g.H
    a = g.da(0, H, 0.25 * W, 0)
    xs = [a, 0.27 * W, 0.5 * W, 0.73 * W, W - a]
    ys = [H, 0, H * 0.94, 0, H]
    for i in range(4):
        s0 = "both" if i == 0 else None
        s1 = "both" if i == 3 else None
        g.slant(xs[i], ys[i], xs[i + 1], ys[i + 1], s0=s0, s1=s1)


def X(g):
    W, H = g.W, g.H
    a = g.da(0, H, W, 0)
    g.slant(a, H, W - a, 0, s0="both", s1="both")
    g.slant(W - a, H, a, 0, s0="both", s1="both")


def Y(g):
    W, H, m = g.W, g.H, g.m
    jy = 0.44 * H
    a = g.da(0, H, m, jy)
    g.slant(a, H, m, jy, "h", "p", s0="both")
    g.slant(W - a, H, m, jy, "h", "p", s0="both")
    g.stem(m, 0, jy + g.vh, top=None)


def Z(g):
    W, H = g.W, g.H
    g.arm(W - g.vh * 0.2, g.t, 0.06 * W, "down")
    d = P.unit((W, H), (0, 0))
    a = g.nib.vertical / 2.0 / max(abs(d[1]), 0.25)
    g.slant(W - a, H - g.hh, a, g.hh, thick=True)
    g.arm(0.0, g.b, W, "up")


# ════════════════════════════ СТРОЧНЫЕ ════════════════════════════

def a_(g):
    l, r, H = g.l, g.r, g.H
    o = g.ov
    if g.st.a_single:
        g.oval(l, g.b - o, r, g.t + o)
        g.stem(r, 0, H, top="right", bot="right")
        return
    st_top = 0.6 * H
    g.stem(r, 0, st_top, top=None, bot="right")
    rx = (r - l) / 2 * 0.98
    ry = g.t + o - st_top
    g.curve(g.arc(r - rx, st_top, rx, ry, 0, 158))
    yb = 0.52 * H
    rxb = (r - l) / 2 * 0.98
    ryb = (yb - (g.b - o)) / 2
    cxb = l + rxb
    g.curve([(r, yb), (cxb, yb)] + g.arc(cxb, yb - ryb, rxb, ryb, 90, 320) + [(r, 0.16 * H)])


def b_(g):
    o = g.ov
    g.stem(g.l, 0, g.ASC, top="left", bot=None)
    g.oval(g.l, g.b - o, g.r, g.t + o)


def c_(g):
    o = g.ov
    g.curve(g.arc(g.m + 0.01 * g.W, g.H / 2, (g.r - g.l) / 2, (g.t - g.b) / 2 + o, 42, 318))


def d_(g):
    o = g.ov
    g.stem(g.r, 0, g.ASC, top="left", bot="right")
    g.oval(g.l, g.b - o, g.r, g.t + o)


def e_(g):
    l, r, H = g.l, g.r, g.H
    o = g.ov
    cy = H / 2 + 0.02 * H
    rx = (r - l) / 2
    ry = (g.t - g.b) / 2 + o
    g.curve([(l, cy)] + g.arc(g.m, H / 2, rx, ry, 0, 322))
    g.bar(l, cy, r)


def f_(g):
    W = g.W
    x = 0.36 * W
    rx = (W - x) * 0.62
    ry = min(rx * 0.9, (g.ASC - g.XH) * 0.9)
    cy = g.ASC - g.hh - ry + g.ov
    g.curve([(x, g.e), (x, cy)] + g.arc(x + rx, cy, rx, ry, 180, 42))
    g.bar(0, g.XH - g.hh, 0.9 * W)
    if g.st.serif:
        g.serif(x, g.vh, 0, True, "both")


def g_(g):
    l, r, H = g.l, g.r, g.H
    o = g.ov
    g.oval(l, g.b - o, r, g.t + o)
    ry = min((r - l) * 0.42, -g.DESC * 0.42)
    cy = g.DESC + g.hh - o + ry
    g.curve([(r, H - g.e), (r, cy)] + g.arc(g.m, cy, (r - l) / 2, ry, 0, -158))
    if g.st.serif:
        g.serif(r, g.vh, H, False, "right")


def h_(g):
    l, r, H = g.l, g.r, g.H
    g.stem(l, 0, g.ASC, top="left")
    rx = (r - l) / 2
    ry = min(rx * 0.95, 0.46 * H)
    cy = g.t + g.ov - ry
    g.curve([(l, cy)] + g.arc(g.m, cy, rx, ry, 180, 0) + [(r, g.e)])
    if g.st.serif:
        g.serif(r, g.vh, 0, True, "both")


def i_(g):
    g.stem(g.m, 0, g.H, top="left")
    g.dot(g.m, g.XH + (g.ASC - g.XH) * 0.62)
    if g.st.mono:
        g.bar(g.m - 0.3 * g.st.mono_w, g.b, g.m + 0.3 * g.st.mono_w)
        g.bar(g.m - 0.22 * g.st.mono_w, g.t, g.m)


def j_(g):
    r = g.r
    rx = max(g.W * 0.5, 60)
    ry = min(rx, -g.DESC * 0.45)
    cy = g.DESC + g.hh + ry
    g.curve([(r, g.H - g.e), (r, cy)] + g.arc(r - rx, cy, rx, ry, 0, -150))
    g.dot(r, g.XH + (g.ASC - g.XH) * 0.62)
    if g.st.serif:
        g.serif(r, g.vh, g.H, False, "left")


def k_(g):
    l, H, W = g.l, g.H, g.W
    g.stem(l, 0, g.ASC, top="left")
    a = g.da(W, H, l, 0.33 * H)
    x0, y0, x1, y1 = W - a, H, l, 0.32 * H
    g.slant(x0, y0, x1, y1, "h", "v", s0="both")
    t = 0.45
    jx, jy = x0 + (x1 - x0) * t, y0 + (y1 - y0) * t
    a2 = g.da(jx, jy, W, 0)
    g.slant(jx, jy, W - a2, 0, "p", "h", s1="both")


def l_(g):
    g.stem(g.m, 0, g.ASC, top="left")
    if g.st.mono:
        g.bar(g.m - 0.3 * g.st.mono_w, g.b, g.m + 0.3 * g.st.mono_w)
        g.bar(g.m - 0.22 * g.st.mono_w, g.ASC - g.hh, g.m)


def m_(g):
    l, r, H = g.l, g.r, g.H
    g.stem(l, 0, H, top="left")
    xm = (l + r) / 2
    for x0, x1 in ((l, xm), (xm, r)):
        rx = (x1 - x0) / 2
        ry = min(rx * 1.0, 0.46 * H)
        cy = g.t + g.ov - ry
        g.curve([(x0, cy)] + g.arc((x0 + x1) / 2, cy, rx, ry, 180, 0) + [(x1, g.e)])
        if g.st.serif:
            g.serif(x1, g.vh, 0, True, "both")


def n_(g):
    l, r, H = g.l, g.r, g.H
    g.stem(l, 0, H, top="left")
    rx = (r - l) / 2
    ry = min(rx * 0.95, 0.46 * H)
    cy = g.t + g.ov - ry
    g.curve([(l, cy)] + g.arc(g.m, cy, rx, ry, 180, 0) + [(r, g.e)])
    if g.st.serif:
        g.serif(r, g.vh, 0, True, "both")


def o_(g):
    o = g.ov
    g.oval(g.l, g.b - o, g.r, g.t + o)


def p_(g):
    o = g.ov
    g.stem(g.l, g.DESC, g.H, top="left")
    g.oval(g.l, g.b - o, g.r, g.t + o)


def q_(g):
    o = g.ov
    g.stem(g.r, g.DESC, g.H, top="right")
    g.oval(g.l, g.b - o, g.r, g.t + o)


def r_(g):
    l, H, W = g.l, g.H, g.W
    g.stem(l, 0, H, top="left")
    rx = (W - l) * 0.6
    ry = min(rx * 0.85, 0.42 * H)
    cy = g.t + g.ov - ry
    g.curve([(l, cy)] + g.arc(l + rx, cy, rx, ry, 180, 52))


def t_(g):
    W = g.W
    x = 0.34 * W
    top = g.XH + (g.ASC - g.XH) * 0.5
    rx = (W - x) * 0.6
    ry = min(rx * 0.9, 0.3 * g.H)
    cy = g.b - g.ov + ry
    g.curve([(x, top), (x, cy)] + g.arc(x + rx, cy, rx, ry, 180, 312))
    g.bar(0, g.XH - g.hh, 0.9 * W)


def u_(g):
    l, r, H = g.l, g.r, g.H
    rx = (r - l) / 2
    ry = min(rx * 0.95, 0.46 * H)
    cy = g.b - g.ov + ry
    g.curve([(l, H - g.e), (l, cy)] + g.arc(g.m, cy, rx, ry, 180, 360))
    g.stem(r, 0, H, top="left", bot="right")
    if g.st.serif:
        g.serif(l, g.vh, H, False, "left")


def y_(g):
    W, H, m = g.W, g.H, g.m
    a = g.da(0, H, m, 0)
    g.slant(a, H, m, 0, "h", "p", s0="both")
    x0 = W - a
    xd = x0 + (m - x0) * (H - g.DESC) / H
    g.slant(x0, H, xd, g.DESC, s0="both", s1="both")


# ════════════════════════════ КИРИЛЛИЦА ════════════════════════════

def Be(g):
    l, r, H, W = g.l, g.r, g.H, g.W
    g.stem(l, 0, H, bot="left", top="left")
    g.arm(l, g.t, 0.92 * W, "down")
    g.bowl_r(l, 0.56 * H, g.b, r)


def Ge(g):
    g.stem(g.l, 0, g.H, top="left")
    g.arm(g.l, g.t, 0.95 * g.W, "down")


def De(g):
    W, H = g.W, g.H
    xl = 0.2 * W
    g.slant(xl + 0.02 * W, H, xl - 0.04 * W, 0, "h", "h")
    g.bar(xl, g.t, g.r)
    g.stem(g.r, 0, H, bot=None, top="right")
    g.bar(0, g.b, W)
    foot = g.DESC * 0.72 if g.H > g.XH + 1 else g.DESC * 0.62
    g.stem(g.vh, foot, g.b, bot=None, top=None)
    g.stem(W - g.vh, foot, g.b, bot=None, top=None)


def Zhe(g):
    W, H, m = g.W, g.H, g.m
    g.stem(m, 0, H)
    mid = 0.5 * H
    a = g.da(0, H, m, mid)
    for sx in (-1, 1):
        x_out = m + sx * (W / 2 - a)
        g.slant(x_out, H, m + sx * g.vh * 0.3, mid + 0.04 * H, "h", "v", s0="both")
        g.slant(m + sx * (g.vh + 0.08 * W), mid, x_out + sx * 0.0, 0, "p", "h", s1="both")


def Ze(g):
    l, r, H = g.l, g.r, g.H
    o = g.ov
    mid = 0.53 * H
    cx = g.m * 0.98
    ry1 = (g.t + o - mid) / 2
    ry2 = (mid - (g.b - o)) / 2
    rx2 = (r - l) / 2
    rx1 = rx2 * 0.9
    g.curve(g.arc(cx, mid + ry1, rx1, ry1, 150, -90))
    g.curve(g.arc(cx, mid - ry2, rx2, ry2, 90, -150))
    g.bar(0.36 * g.W, mid, cx)


def I_cyr(g):
    l, r, H, W = g.l, g.r, g.H, g.W
    g.stem(l, 0, H, bot="left")
    g.stem(r, 0, H, top="right")
    a = g.da(0, 0, W, H)
    g.slant(a, 0, W - a, H)


def Ishort(g):
    I_cyr(g)
    W, H = g.W, g.H
    bw, bh = 0.2 * W, 0.075 * g.CAP
    y = H + 0.05 * g.CAP + bh
    g.curve(g.arc(g.m, y, bw, bh, 195, 345, 20))


def El(g):
    W, H = g.W, g.H
    xt = 0.26 * W
    a = g.da(0, 0, xt, H)
    g.slant(a, 0, xt, H, "h", "h", s0="both")
    g.bar(xt, g.t, g.r)
    g.stem(g.r, 0, H, top="right")


def Pe(g):
    l, r, H = g.l, g.r, g.H
    g.stem(l, 0, H, top="left")
    g.stem(r, 0, H, top="right")
    g.bar(l, g.t, r)


def U_cyr(g):
    W, H = g.W, g.H
    a = g.da(0, H, 0.5 * W, 0.3 * H)
    g.slant(a, H, 0.52 * W, 0.3 * H, "h", "p", s0="both")
    g.slant(W - a, H, 0.3 * W, 0, "h", "h", s0="both", s1="both")


def Ef(g):
    W, H, m = g.W, g.H, g.m
    g.stem(m, 0, H)
    g.oval(g.l, 0.15 * H, g.r, 0.85 * H)


def Tse(g):
    l, H, W = g.l, g.H, g.W
    r = W - 0.12 * W - g.vh
    g.stem(l, 0, H, bot=None)
    g.stem(r, 0, H, bot=None)
    g.bar(l, g.b, W)
    g.stem(W - g.vh, g.DESC * 0.72, g.b, bot=None, top=None)


def Che(g):
    l, r, H = g.l, g.r, g.H
    y1 = 0.62 * H
    rx = (r - l) * 0.45
    ry = 0.2 * H
    g.curve([(l, H - g.e), (l, y1)] + g.arc(l + rx, y1, rx, ry, 180, 270) + [(r, y1 - ry)])
    g.stem(r, 0, H)
    if g.st.serif:
        g.serif(l, g.vh, H, False, "both")


def Sha(g):
    l, r, H, m = g.l, g.r, g.H, g.m
    for x in (l, m, r):
        g.stem(x, 0, H, bot=None)
    g.bar(l, g.b, r)


def Shcha(g):
    W, H = g.W, g.H
    l = g.l
    r = W - 0.1 * W - g.vh
    m = (l + r) / 2
    for x in (l, m, r):
        g.stem(x, 0, H, bot=None)
    g.bar(l, g.b, W)
    g.stem(W - g.vh, g.DESC * 0.72, g.b, bot=None, top=None)


def Hard(g):
    W, H = g.W, g.H
    xs = 0.26 * W
    g.arm(xs, g.t, 0.0, "down")
    g.stem(xs, 0, H, top=None, bot="left")
    g.bowl_r(xs, 0.56 * H, g.b, g.r)


def Yery(g):
    W, H = g.W, g.H
    l = g.l
    g.stem(l, 0, H, top="both", bot="left", ktop=0.5)
    g.bowl_r(l, 0.56 * H, g.b, 0.62 * W)
    g.stem(g.r, 0, H)


def Soft(g):
    l, H = g.l, g.H
    g.stem(l, 0, H, top="both", bot="left", ktop=0.5)
    g.bowl_r(l, 0.56 * H, g.b, g.r)


def Ereversed(g):
    l, r, H = g.l, g.r, g.H
    o = g.ov
    g.curve(g.arc(g.m - 0.01 * g.W, H / 2, (r - l) / 2, (g.t - g.b) / 2 + o, 136, -136))
    g.bar(0.34 * g.W, H / 2, r)


def Yu(g):
    W, H = g.W, g.H
    l = g.l
    xo = 0.34 * W
    o = g.ov
    g.stem(l, 0, H)
    g.bar(l, H / 2, xo + g.vh)
    g.oval(xo + g.vh, g.b - o, g.r, g.t + o)


def Ya(g):
    l, r, H, W = g.l, g.r, g.H, g.W
    g.stem(r, 0, H, top="right")
    mid = 0.47 * H
    g.bowl_l(r, g.t, mid, l + 0.03 * W)
    jx = r - (r - l) * 0.5
    a = g.da(jx, mid, 0, 0)
    g.slant(jx, mid, a, 0, "p", "h", s1="both")


def be_(g):
    l, r, H = g.l, g.r, g.H
    o = g.ov
    g.oval(l, g.b - o, r, g.t + o)
    rx = (r - l) * 0.5
    y0 = H * 0.5
    ry = (g.ASC - g.hh) - y0
    g.curve([(l, y0)] + g.arc(l + rx, y0, rx, ry, 180, 90, 16) + [(r + g.vh * 0.6, g.ASC - g.hh)])


def ef_(g):
    W, H, m = g.W, g.H, g.m
    o = g.ov
    g.stem(m, g.DESC, g.ASC, top="left")
    g.oval(g.l, g.b - o, g.r, g.t + o)


def diaeresis(g, x0, x1, y):
    g.dot(x0, y, 0.85)
    g.dot(x1, y, 0.85)


def Yo(g):
    E(g)
    y = g.H + 0.11 * g.CAP
    diaeresis(g, g.W * 0.32, g.W * 0.72, y)


def yo_(g):
    e_(g)
    y = g.XH + (g.ASC - g.XH) * 0.6
    diaeresis(g, g.W * 0.3, g.W * 0.7, y)


# ════════════════════════════ ЦИФРЫ ════════════════════════════

def d0(g):
    o = g.ov
    g.oval(g.l, g.b - o, g.r, g.t + o)


def d1(g):
    W, H = g.W, g.H
    x = 0.58 * W
    g.stem(x, 0, H, top=None, bot=None)
    g.line((x, H - g.hh * 0.5), (x - 0.34 * W, 0.78 * H))
    if g.st.serif or g.st.mono:
        g.bar(x - 0.3 * W, g.b, x + 0.3 * W)


def d2(g):
    l, r, H = g.l, g.r, g.H
    o = g.ov
    rx = (r - l) / 2
    ry = 0.27 * H
    cy = g.t + o - ry
    pts = g.arc(g.m, cy, rx, ry, 158, -28)
    g.curve(pts + [(l + g.vh * 0.2, g.b + g.hh * 0.5)])
    g.arm(l, g.b, g.W, "up")


def d3(g):
    l, r, H = g.l, g.r, g.H
    o = g.ov
    mid = 0.55 * H
    ry1 = (g.t + o - mid) / 2
    ry2 = (mid - (g.b - o)) / 2
    rx2 = (r - l) / 2
    rx1 = rx2 * 0.88
    cx = g.m + 0.02 * g.W
    g.curve(g.arc(cx, mid + ry1, rx1, ry1, 155, -90))
    g.curve(g.arc(cx, mid - ry2, rx2, ry2, 90, -155))
    g.bar(0.42 * g.W, mid, cx)


def d4(g):
    W, H = g.W, g.H
    x = 0.7 * W
    y = 0.3 * H
    g.stem(x, 0, H, top=None)
    g.line((x, H - g.hh), (g.l * 0.6, y))
    g.bar(0, y, W)


def d5(g):
    l, r, H = g.l, g.r, g.H
    o = g.ov
    cyb = 0.35 * H
    ry = cyb - (g.b - o)
    rx = (r - l) / 2
    p0 = g.arc(g.m, cyb, rx, ry, 142, 142, 1)[0]
    g.arm(p0[0], g.t, g.r, "down")
    g.curve([(p0[0], g.t), p0] + g.arc(g.m, cyb, rx, ry, 142, -150)[1:])


def d6(g):
    l, r, H = g.l, g.r, g.H
    o = g.ov
    top = 0.6 * H
    g.oval(l, g.b - o, r, top)
    cy = (g.b - o + top) / 2
    g.curve(g.arc(g.m, cy, (r - l) / 2, g.t + o - cy, 180, 52))


def d7(g):
    W, H = g.W, g.H
    g.arm(W, g.t, 0.0, "down")
    a = g.da(W, H, 0.3 * W, 0)
    g.slant(W - a, H, 0.32 * W, 0, "h", "h")


def d8(g):
    l, r, H = g.l, g.r, g.H
    o = g.ov
    mid = 0.55 * H
    g.oval(l + 0.06 * g.W, mid - g.hh, r - 0.06 * g.W, g.t + o)
    g.oval(l, g.b - o, r, mid + g.hh)


def d9(g):
    l, r, H = g.l, g.r, g.H
    o = g.ov
    bot = 0.4 * H
    g.oval(l, bot, r, g.t + o)
    cy = (bot + g.t + o) / 2
    g.curve(g.arc(g.m, cy, (r - l) / 2, cy - (g.b - o), 0, -128))


# ════════════════════════════ ЗНАКИ ════════════════════════════

def p_period(g):
    rr = g.dotr()
    g.dot(g.W / 2, rr)


def p_comma(g):
    rr = g.dotr()
    x = g.W / 2
    g.dot(x, rr)
    g.line((x + rr * 0.55, rr * 0.6), (x + rr * 0.3, -rr * 0.8), (x - rr * 0.7, -rr * 2.3))


def p_colon(g):
    rr = g.dotr()
    g.dot(g.W / 2, rr)
    g.dot(g.W / 2, g.XH - rr)


def p_semicolon(g):
    p_comma(g)
    rr = g.dotr()
    g.dot(g.W / 2, g.XH - rr)


def p_excl(g):
    rr = g.dotr()
    x = g.W / 2
    g.stem(x, 2.6 * rr + 0.06 * g.CAP, g.CAP, bot=None, top=None)
    g.dot(x, rr)


def p_quest(g):
    l, r = g.l, g.r
    H = g.CAP
    o = g.ov
    rr = g.dotr()
    rx = (r - l) / 2
    ry = 0.25 * H
    cy = H - g.hh + o - ry
    pts = g.arc(g.m, cy, rx, ry, 160, -58)
    g.curve(pts + [(g.m, 0.42 * H), (g.m, 2.6 * rr + 0.06 * H)])
    g.dot(g.m, rr)


def p_hyphen(g):
    g.bar(g.vh * 0.3, g.XH * 0.5, g.W - g.vh * 0.3)


def p_dash(g):
    g.bar(0, g.XH * 0.5, g.W)


def paren(g, left):
    """Скобка — дуга окружности во всю высоту строки с выносными."""
    W = g.W
    top, bot = g.ASC + 10 - g.e, g.DESC * 0.75 + g.e
    half = (top - bot) / 2
    cy = (top + bot) / 2
    bulge = max(W - 2 * g.vh, 40)
    a = 2 * math.atan(bulge / half)
    R = half / math.sin(a)
    n = 30
    if left:
        cx = g.vh + R
        angs = [math.pi - a + 2 * a * i / n for i in range(n + 1)]
    else:
        cx = W - g.vh - R
        angs = [a - 2 * a * i / n for i in range(n + 1)]
    g.curve([(cx + R * math.cos(t), cy + R * math.sin(t)) for t in angs])


def p_lparen(g):
    paren(g, True)


def p_rparen(g):
    paren(g, False)


def p_lbracket(g):
    top, bot = g.ASC + 10, g.DESC * 0.75
    x = g.vh
    g.line((g.W, top - g.hh), (x, top - g.hh), (x, bot + g.hh), (g.W, bot + g.hh))


def p_rbracket(g):
    top, bot = g.ASC + 10, g.DESC * 0.75
    x = g.W - g.vh
    g.line((0, top - g.hh), (x, top - g.hh), (x, bot + g.hh), (0, bot + g.hh))


def p_slash(g):
    W = g.W
    a = g.da(0, g.DESC * 0.5, W, g.ASC)
    g.slant(a, g.DESC * 0.5, W - a, g.ASC)


def p_backslash(g):
    W = g.W
    a = g.da(0, g.ASC, W, g.DESC * 0.5)
    g.slant(a, g.ASC, W - a, g.DESC * 0.5)


def p_plus(g):
    W = g.W
    y = g.XH * 0.5
    g.bar(0, y, W)
    g.line((W / 2, y - W * 0.48), (W / 2, y + W * 0.48))


def p_equal(g):
    W = g.W
    y = g.XH * 0.5
    g.bar(0, y + 0.13 * g.CAP, W)
    g.bar(0, y - 0.13 * g.CAP, W)


def p_star(g):
    W = g.W
    cx, cy, rr = W / 2, g.CAP * 0.78, W * 0.42
    for ang in (90, 210, 330):
        a = math.radians(ang)
        g.line((cx, cy), (cx + rr * math.cos(a), cy + rr * math.sin(a)))
    for ang in (30, 150, 270):
        a = math.radians(ang)
        g.line((cx, cy), (cx + rr * 0.9 * math.cos(a), cy + rr * 0.9 * math.sin(a)))


def p_percent(g):
    W, H = g.W, g.CAP
    a = g.da(0, 0, W, H)
    g.slant(W * 0.2 + a * 0.3, 0, W * 0.8 - a * 0.3, H)
    rw = W * 0.2
    g.oval(g.vh, H * 0.58, g.vh + 2 * rw, H)
    g.oval(W - g.vh - 2 * rw, 0, W - g.vh, H * 0.42)


def p_hash(g):
    W, H = g.W, g.CAP * 0.95
    g.slant(0.36 * W, 0, 0.46 * W, H)
    g.slant(0.66 * W, 0, 0.76 * W, H)
    g.bar(0.06 * W, 0.66 * H, W)
    g.bar(0, 0.34 * H, 0.94 * W)


def p_quote(g):
    x0, x1 = g.W * 0.28, g.W * 0.72
    for x in (x0, x1):
        g.line((x, g.CAP), (x, g.CAP * 0.72))


def p_apos(g):
    g.line((g.W / 2, g.CAP), (g.W / 2, g.CAP * 0.72))


def chevron(g, x0, x1, left):
    y = g.XH * 0.5
    hgt = g.XH * 0.32
    if left:
        g.line((x1, y + hgt), (x0, y), (x1, y - hgt))
    else:
        g.line((x0, y + hgt), (x1, y), (x0, y - hgt))


def p_laquo(g):
    W = g.W
    chevron(g, g.vh, W * 0.48, True)
    chevron(g, W * 0.48, W - g.vh, True)


def p_raquo(g):
    W = g.W
    chevron(g, g.vh, W * 0.52, False)
    chevron(g, W * 0.52, W - g.vh, False)


def p_less(g):
    chevron(g, g.vh, g.W - g.vh, True)


def p_greater(g):
    chevron(g, g.vh, g.W - g.vh, False)


def p_numero(g):
    W, H = g.W, g.CAP
    nw = W * 0.58
    sub = G(g.st, nw, H)
    N(sub)
    g.out += sub.out
    o = g.ov
    x0 = nw + 0.06 * W
    g.oval(x0 + g.vh, H * 0.42, W - g.vh, H * 0.86)
    g.bar(x0 + g.vh * 0.2, H * 0.24, W - g.vh * 0.2)


def p_underscore(g):
    g.bar(0, g.DESC * 0.45, g.W)


def p_bar(g):
    g.line((g.W / 2, g.DESC * 0.85), (g.W / 2, g.ASC + 20))


def p_tilde(g):
    W = g.W
    y = g.XH * 0.5
    pts = [(W * i / 16, y + 0.06 * g.CAP * math.sin(2 * math.pi * i / 16)) for i in range(17)]
    g.curve(pts)


def p_dollar(g):
    sub = G(g.st, g.W, g.CAP)
    S(sub)
    g.out += sub.out
    g.line((g.m, -0.1 * g.CAP), (g.m, 1.1 * g.CAP))


def p_caret(g):
    W, H = g.W, g.CAP
    g.line((g.vh, 0.55 * H), (W / 2, H), (W - g.vh, 0.55 * H))


def p_degree(g):
    W = g.W
    g.oval(g.vh, g.CAP * 0.62, W - g.vh, g.CAP)


def p_middot(g):
    g.dot(g.W / 2, g.XH * 0.5)


def p_ellipsis(g):
    rr = g.dotr()
    for k in (0.18, 0.5, 0.82):
        g.dot(g.W * k, rr)


def p_ruble(g):
    W, H = g.W, g.CAP
    x = 0.26 * W
    g.stem(x, 0, H, top="left")
    g.bowl_r(x, H - g.hh, 0.44 * H, W - g.vh)
    g.bar(0, 0.44 * H, x + 0.45 * W)
    g.bar(0, 0.22 * H, 0.66 * W)


def p_euro(g):
    sub = G(g.st, g.W, g.CAP)
    C(sub)
    g.out += sub.out
    g.bar(0, 0.58 * g.CAP, 0.62 * g.W)
    g.bar(0, 0.42 * g.CAP, 0.58 * g.W)


# ════════════════════════════ РЕЕСТР ════════════════════════════
# символ -> (рисовалка, ширина тела при обычной ширине, высота 'C'|'X', поля слева/справа)
# поля: s — прямой край, r — круглый, d — наклонный, p — знак
REG = {}


def reg(chars, fn, W, h, sp):
    for ch in chars:
        REG[ch] = (fn, W, h, sp)


# латиница
reg("A", A, 620, "C", "dd"); reg("B", B, 530, "C", "sr"); reg("C", C, 600, "C", "rr"); reg("D", D, 590, "C", "sr")
reg("E", E, 480, "C", "ss"); reg("F", F, 460, "C", "ss"); reg("G", G_, 620, "C", "rs"); reg("H", H_, 560, "C", "ss")
reg("I", I_, 0, "C", "ss"); reg("J", J, 440, "C", "rs"); reg("K", K, 560, "C", "sd"); reg("L", L, 450, "C", "sp")
reg("M", M, 700, "C", "ss"); reg("N", N, 570, "C", "ss"); reg("O", O, 650, "C", "rr"); reg("P", P_, 520, "C", "sr")
reg("Q", Q, 650, "C", "rr"); reg("R", R, 540, "C", "sd"); reg("S", S, 530, "C", "rr"); reg("T", T, 560, "C", "pp")
reg("U", U_lat, 560, "C", "ss"); reg("V", V, 600, "C", "dd"); reg("W", W_, 860, "C", "dd"); reg("X", X, 580, "C", "dd")
reg("Y", Y, 580, "C", "dd"); reg("Z", Z, 520, "C", "rr")
reg("a", a_, 470, "X", "rs"); reg("b", b_, 500, "X", "sr"); reg("c", c_, 450, "X", "rr"); reg("d", d_, 500, "X", "rs")
reg("e", e_, 470, "X", "rr"); reg("f", f_, 300, "X", "pp"); reg("g", g_, 500, "X", "rs"); reg("h", h_, 470, "X", "ss")
reg("i", i_, 0, "X", "ss"); reg("j", j_, 170, "X", "ps"); reg("k", k_, 450, "X", "sd"); reg("l", l_, 0, "X", "ss")
reg("m", m_, 740, "X", "ss"); reg("n", n_, 470, "X", "ss"); reg("o", o_, 500, "X", "rr"); reg("p", p_, 500, "X", "sr")
reg("q", q_, 500, "X", "rs"); reg("r", r_, 300, "X", "sp"); reg("s", lambda g: S(g), 410, "X", "rr")
reg("t", t_, 290, "X", "pp"); reg("u", u_, 470, "X", "ss"); reg("v", lambda g: V(g), 450, "X", "dd")
reg("w", lambda g: W_(g), 680, "X", "dd"); reg("x", lambda g: X(g), 450, "X", "dd"); reg("y", y_, 460, "X", "dd")
reg("z", lambda g: Z(g), 410, "X", "rr")

# кириллица: совпадающие с латиницей
for cy, la in zip("АВЕКМНОРСТХ", "ABEKMHOPCTX"):
    REG[cy] = REG[la]
for cy, la in zip("аеорсух", "aeopcyx"):
    REG[cy] = REG[la]
reg("Б", Be, 530, "C", "sr"); reg("Г", Ge, 450, "C", "sp"); reg("Д", De, 640, "C", "dd"); reg("Ж", Zhe, 840, "C", "dd")
reg("З", Ze, 520, "C", "rr"); reg("И", I_cyr, 570, "C", "ss"); reg("Й", Ishort, 570, "C", "ss"); reg("Л", El, 580, "C", "ds")
reg("П", Pe, 560, "C", "ss"); reg("У", U_cyr, 580, "C", "dd"); reg("Ф", Ef, 700, "C", "rr"); reg("Ц", Tse, 590, "C", "sp")
reg("Ч", Che, 530, "C", "ss"); reg("Ш", Sha, 780, "C", "ss"); reg("Щ", Shcha, 820, "C", "sp"); reg("Ъ", Hard, 640, "C", "pr")
reg("Ы", Yery, 740, "C", "ss"); reg("Ь", Soft, 520, "C", "sr"); reg("Э", Ereversed, 600, "C", "rr"); reg("Ю", Yu, 820, "C", "sr")
reg("Я", Ya, 540, "C", "ds"); reg("Ё", Yo, 480, "C", "ss")
# строчная кириллица — уменьшенные заглавные (ширина 0,82)
for up, lo in zip("ВГДЖЗИЙКЛМНПТЦЧШЩЪЫЬЭЮЯ", "вгджзийклмнптцчшщъыьэюя"):
    fn, W, h, sp = REG[up]
    REG[lo] = (fn, W * 0.82, "X", sp)
reg("б", be_, 500, "X", "rr"); reg("ф", ef_, 640, "X", "rr"); reg("ё", yo_, 470, "X", "rr")

# цифры (табличные)
for ch, fn in zip("0123456789", (d0, d1, d2, d3, d4, d5, d6, d7, d8, d9)):
    reg(ch, fn, 520, "C", "rr")

# знаки
reg(".", p_period, 0, "X", "pp"); reg(",", p_comma, 0, "X", "pp"); reg(":", p_colon, 0, "X", "pp")
reg(";", p_semicolon, 0, "X", "pp"); reg("!", p_excl, 0, "C", "pp"); reg("?", p_quest, 430, "C", "pp")
reg("-", p_hyphen, 300, "X", "pp"); reg("–", p_dash, 500, "X", "pp"); reg("—", p_dash, 920, "X", "pp")
reg("(", p_lparen, 230, "C", "pp"); reg(")", p_rparen, 230, "C", "pp")
reg("[", p_lbracket, 220, "C", "pp"); reg("]", p_rbracket, 220, "C", "pp")
reg("/", p_slash, 360, "C", "pp"); reg("\\", p_backslash, 360, "C", "pp")
reg("+", p_plus, 520, "X", "pp"); reg("=", p_equal, 520, "X", "pp"); reg("*", p_star, 400, "C", "pp")
reg("%", p_percent, 760, "C", "pp"); reg("#", p_hash, 600, "C", "pp")
reg('"', p_quote, 300, "C", "pp"); reg("'", p_apos, 120, "C", "pp")
reg("«", p_laquo, 440, "X", "pp"); reg("»", p_raquo, 440, "X", "pp")
reg("<", p_less, 460, "X", "pp"); reg(">", p_greater, 460, "X", "pp")
reg("№", p_numero, 960, "C", "sp"); reg("_", p_underscore, 520, "X", "pp"); reg("|", p_bar, 0, "C", "pp")
reg("~", p_tilde, 520, "X", "pp"); reg("$", p_dollar, 530, "C", "rr"); reg("^", p_caret, 460, "C", "pp")
reg("°", p_degree, 260, "C", "pp"); reg("·", p_middot, 0, "X", "pp"); reg("…", p_ellipsis, 680, "X", "pp")
reg("₽", p_ruble, 560, "C", "sp"); reg("€", p_euro, 600, "C", "rr")
