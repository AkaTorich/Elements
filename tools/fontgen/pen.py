# Перо: осевая линия штриха -> контур TrueType.
# Толщина зависит от направления штриха, как у широкого пера: перо держат под углом,
# штрих поперёк кромки — толстый, вдоль — тонкий (контраст антиквы, гуманистов, дидон);
# при thin == thick штрих монолинейный (гротеск, геометрический).
# Открытый штрих — один контур; концы прямые (поперёк) или круглые. Замкнутый — наружный
# контур по часовой стрелке и просвет против: правило ненулевой обмотки TrueType сливает
# наложенные штрихи в одну заливку, а просветы остаются пустыми.
import math

MITER = 2.4          # острый угол: выступ наружу не длиннее 2,4 полутолщины, иначе срез


def unit(a, b):
    dx, dy = b[0] - a[0], b[1] - a[1]
    n = math.hypot(dx, dy)
    return (dx / n, dy / n) if n > 1e-9 else (1.0, 0.0)


def clean(pts, closed=False):
    out = []
    for p in pts:
        p = (float(p[0]), float(p[1]))
        if not out or math.hypot(p[0] - out[-1][0], p[1] - out[-1][1]) > 0.75:
            out.append(p)
    if closed and len(out) > 2 and math.hypot(out[0][0] - out[-1][0], out[0][1] - out[-1][1]) <= 0.75:
        out.pop()
    return out


def area(poly):
    s = 0.0
    for i in range(len(poly)):
        x0, y0 = poly[i]
        x1, y1 = poly[(i + 1) % len(poly)]
        s += x0 * y1 - x1 * y0
    return s / 2.0


def clockwise(poly):
    return poly if area(poly) < 0 else poly[::-1]


def counter(poly):
    return poly if area(poly) > 0 else poly[::-1]


class Nib:
    """thick — толщина поперёк кромки пера, thin — вдоль; angle — наклон кромки (0 — горизонтально);
    power — жёсткость: >1 резче отделяет толстые штрихи (стойки, «\\») от тонких (горизонтали, «/»)."""

    def __init__(self, thick, thin, angle_deg=0.0, power=1.0):
        self.thick = float(thick)
        self.thin = float(thin)
        self.p = float(power)
        a = math.radians(angle_deg)
        self.n = (math.cos(a), math.sin(a))

    def width(self, d):
        c = abs(d[0] * self.n[1] - d[1] * self.n[0])
        return self.thin + (self.thick - self.thin) * c ** self.p

    @property
    def vertical(self):
        return self.width((0.0, 1.0))

    @property
    def horizontal(self):
        return self.width((1.0, 0.0))


def _side(pts, closed, sign, nib):
    n = len(pts)
    m = n if closed else n - 1
    dirs = [unit(pts[i], pts[(i + 1) % n]) for i in range(m)]
    out = []
    for i in range(n):
        if closed:
            din, dout = dirs[i - 1], dirs[i]
        elif i == 0:
            din = dout = dirs[0]
        elif i == n - 1:
            din = dout = dirs[-1]
        else:
            din, dout = dirs[i - 1], dirs[i]
        tx, ty = din[0] + dout[0], din[1] + dout[1]
        tl = math.hypot(tx, ty)
        tan = (tx / tl, ty / tl) if tl > 1e-9 else dout
        h = nib.width(tan) / 2.0
        n1 = (-din[1], din[0])
        n2 = (-dout[1], dout[0])
        bx, by = n1[0] + n2[0], n1[1] + n2[1]
        bl = math.hypot(bx, by)
        p = pts[i]
        if bl < 1e-9:
            out.append((p[0] + n1[0] * h * sign, p[1] + n1[1] * h * sign))
            continue
        bx, by = bx / bl, by / bl
        cosh = bx * n1[0] + by * n1[1]
        k = h / max(cosh, 1e-6)
        cross = din[0] * dout[1] - din[1] * dout[0]
        outer = (cross < 0 and sign > 0) or (cross > 0 and sign < 0)
        if outer and k > MITER * h:
            out.append((p[0] + n1[0] * h * sign, p[1] + n1[1] * h * sign))
            out.append((p[0] + n2[0] * h * sign, p[1] + n2[1] * h * sign))
        else:
            k = min(k, MITER * h * 1.6)
            out.append((p[0] + bx * k * sign, p[1] + by * k * sign))
    return out


def _cap(center, d, h, steps=10):
    """Полукруг на конце: d — направление наружу; от левого края к правому через вершину."""
    nl = (-d[1], d[0])
    pts = []
    for i in range(1, steps):
        t = math.pi * i / steps
        pts.append((center[0] + nl[0] * h * math.cos(t) + d[0] * h * math.sin(t),
                    center[1] + nl[1] * h * math.cos(t) + d[1] * h * math.sin(t)))
    return pts


def stroke(points, nib, cap="flat"):
    pts = clean(points)
    if len(pts) < 2:
        return []
    L = _side(pts, False, +1, nib)
    R = _side(pts, False, -1, nib)
    poly = list(L)
    if cap == "round":
        d = unit(pts[-2], pts[-1])
        poly += _cap(pts[-1], d, nib.width(d) / 2.0)
    poly += R[::-1]
    if cap == "round":
        d = unit(pts[1], pts[0])
        poly += _cap(pts[0], d, nib.width(d) / 2.0)
    return [clockwise(poly)]


def loop(points, nib):
    pts = clean(points, closed=True)
    if len(pts) < 3:
        return []
    a = _side(pts, True, +1, nib)
    b = _side(pts, True, -1, nib)
    outer, inner = (a, b) if abs(area(a)) > abs(area(b)) else (b, a)
    return [clockwise(outer), counter(inner)]


def slanted(x0, y0, x1, y1, nib):
    """Наклонный штрих, концы срезаны ГОРИЗОНТАЛЬНО ровно на высотах y0 и y1 (A, V, X, N…)."""
    d = unit((x0, y0), (x1, y1))
    w = nib.width(d)
    s = abs(d[1])
    a = (w / 2.0) / max(s, 0.25)
    return [clockwise([(x0 - a, y0), (x0 + a, y0), (x1 + a, y1), (x1 - a, y1)])]


def polygon(points):
    pts = clean(points, closed=True)
    return [clockwise(pts)] if len(pts) >= 3 else []


def disc(cx, cy, r, steps=20):
    return [clockwise([(cx + r * math.cos(2 * math.pi * i / steps), cy + r * math.sin(2 * math.pi * i / steps))
                       for i in range(steps)])]
