# Рисует 30 ОРИГИНАЛЬНЫХ шрифтов и сохраняет их в .ttf (fontTools).
# Каждая буква задана своими штрихами на сетке (x 0..6, y 0..14, базовая 0,
# высота прописной 14). 30 шрифтов — это 30 стилей отрисовки этих штрихов.
# Покрытие: латиница, кириллица, цифры, знаки препинания. Строчные берут
# начертания прописных (шрифты рисовочные/заголовочные).
import math, os
from fontTools.fontBuilder import FontBuilder
from fontTools.pens.ttGlyphPen import TTGlyphPen

EM = 1000
SX = 92           # ширина клетки в единицах шрифта
SY = 50           # высота клетки (y 14 -> 700)
GAP = 60          # боковые поля

def arc(cx, cy, rx, ry, a0, a1, n=14):
    return [(cx + rx * math.cos(math.radians(a)), cy + ry * math.sin(math.radians(a)))
            for a in [a0 + (a1 - a0) * i / n for i in range(n + 1)]]

def ell(cx, cy, rx, ry, n=20):
    return arc(cx, cy, rx, ry, 90, 450, n)

# --- штрихи букв (списки полилиний в клетках сетки) ---
G = {}
G['A'] = [[(0,0),(3,14),(6,0)], [(1.1,4.5),(4.9,4.5)]]
G['B'] = [[(0,0),(0,14),(4,14),(6,11.5),(4,7),(0,7)], [(4,7),(6,3.5),(4,0),(0,0)]]
G['C'] = [[(6,11),(4,14),(2,14),(0,11),(0,3),(2,0),(4,0),(6,3)]]
G['D'] = [[(0,0),(0,14),(3,14),(6,10),(6,4),(3,0),(0,0)]]
G['E'] = [[(6,14),(0,14),(0,0),(6,0)], [(0,7),(4.5,7)]]
G['F'] = [[(6,14),(0,14),(0,0)], [(0,7),(4.5,7)]]
G['G'] = [[(6,11),(4,14),(2,14),(0,11),(0,3),(2,0),(4,0),(6,3),(6,7),(3.5,7)]]
G['H'] = [[(0,0),(0,14)], [(6,0),(6,14)], [(0,7),(6,7)]]
G['I'] = [[(1,14),(5,14)], [(3,14),(3,0)], [(1,0),(5,0)]]
G['J'] = [[(2,14),(6,14)], [(5,14),(5,3),(3.5,0),(1.5,0),(0,3)]]
G['K'] = [[(0,0),(0,14)], [(6,14),(0.3,7)], [(2.2,8.6),(6,0)]]
G['L'] = [[(0,14),(0,0),(6,0)]]
G['M'] = [[(0,0),(0,14),(3,6),(6,14),(6,0)]]
G['N'] = [[(0,0),(0,14),(6,0),(6,14)]]
G['O'] = [ell(3,7,3,7)]
G['P'] = [[(0,0),(0,14),(4,14),(6,11.5),(4,7),(0,7)]]
G['Q'] = [ell(3,7,3,7), [(3.5,3),(6.3,-1.2)]]
G['R'] = [[(0,0),(0,14),(4,14),(6,11.5),(4,7),(0,7)], [(3,7),(6,0)]]
G['S'] = [[(6,11),(4,14),(2,14),(0,11.5),(2,7.5),(4,6.5),(6,3.5),(4,0),(2,0),(0,2.5)]]
G['T'] = [[(0,14),(6,14)], [(3,14),(3,0)]]
G['U'] = [[(0,14),(0,3),(2,0),(4,0),(6,3),(6,14)]]
G['V'] = [[(0,14),(3,0),(6,14)]]
G['W'] = [[(0,14),(1.5,0),(3,9),(4.5,0),(6,14)]]
G['X'] = [[(0,0),(6,14)], [(0,14),(6,0)]]
G['Y'] = [[(0,14),(3,7),(6,14)], [(3,7),(3,0)]]
G['Z'] = [[(0,14),(6,14),(0,0),(6,0)]]

G['0'] = [ell(3,7,2.7,7), [(1,2.5),(5,11.5)]]
G['1'] = [[(1,11),(3,14),(3,0)], [(1,0),(5,0)]]
G['2'] = [[(0,11),(2,14),(4,14),(6,11),(6,9),(0,0),(6,0)]]
G['3'] = [[(0,12),(2,14),(4,14),(6,12),(4,7.5),(6,3),(4,0),(2,0),(0,2)], [(3,7.5),(4.2,7.5)]]
G['4'] = [[(4.5,0),(4.5,14),(0,4),(6,4)]]
G['5'] = [[(6,14),(1,14),(1,8),(4,8),(6,6),(6,2.5),(4,0),(2,0),(0,2)]]
G['6'] = [[(5,13),(3,14),(1,11),(0,6),(0,3),(2,0),(4,0),(6,2.5),(6,5),(4,7),(1.5,7),(0,5.5)]]
G['7'] = [[(0,14),(6,14),(2.5,0)]]
G['8'] = [ell(3,10.7,2.2,3.3,16), ell(3,3.4,2.8,3.4,16)]
G['9'] = [[(1,1),(3,0),(5,3),(6,8),(6,11),(4,14),(2,14),(0,11.5),(0,9),(2,7),(4.5,7),(6,8.5)]]

# знаки
G['.'] = [[(3,0.6),(3,0.6)]]
G[','] = [[(3,0.8),(3,0.8)], [(3.2,0.8),(2.3,-2)]]
G['!'] = [[(3,14),(3,3.5)], [(3,0.8),(3,0.8)]]
G['?'] = [[(0,11),(2,14),(4,14),(6,11.5),(4,8),(3,6),(3,4)], [(3,0.8),(3,0.8)]]
G[':'] = [[(3,9),(3,9)], [(3,2.5),(3,2.5)]]
G[';'] = [[(3,9),(3,9)], [(3.2,2.5),(2.3,0)]]
G['-'] = [[(1.3,7),(4.7,7)]]
G[u'—'] = [[(0,7),(6,7)]]       # тире
G['+'] = [[(3,10),(3,4)], [(1,7),(5,7)]]
G['='] = [[(1,9),(5,9)], [(1,5),(5,5)]]
G['/'] = [[(0,0),(6,14)]]
G['('] = [[(4.5,14),(1.5,10),(1.5,4),(4.5,0)]]
G[')'] = [[(1.5,14),(4.5,10),(4.5,4),(1.5,0)]]
G["'"] = [[(3,14),(3,10.5)]]
G['"'] = [[(2,14),(2,10.5)], [(4,14),(4,10.5)]]
G['*'] = [[(3,13),(3,7)], [(0.6,11.5),(5.4,8.5)], [(5.4,11.5),(0.6,8.5)]]
G['%'] = [[(0,2),(6,12)], ell(1.4,11,1.3,2.2,10), ell(4.6,3,1.3,2.2,10)]
G['#'] = [[(1.8,14),(1.2,0)], [(4.2,14),(3.8,0)], [(0.6,10),(5.6,10)], [(0.4,4),(5.4,4)]]

# кириллица (общие с латиницей ниже через CMAP)
G['Б'] = [[(5.5,14),(0,14),(0,0),(4,0),(6,2.5),(6,5),(4,7),(0,7)]]
G['Г'] = [[(0,0),(0,14),(5.5,14)]]
G['Д'] = [[(1.4,3),(1.4,14),(4.6,14),(4.6,3)], [(0,3),(6,3)], [(0.6,3),(0.6,-2.2)], [(5.4,3),(5.4,-2.2)]]
G['Ж'] = [[(3,0),(3,14)], [(3,7),(0,14)], [(3,7),(0,0)], [(3,7),(6,14)], [(3,7),(6,0)]]
G['З'] = [[(0,12),(2,14),(4,14),(6,12),(4,7.5),(6,3),(4,0),(2,0),(0,2)], [(3.2,7.5),(4,7.5)]]
G['И'] = [[(0,0),(0,14)], [(6,0),(6,14)], [(0,0),(6,14)]]
G['Й'] = [[(0,0),(0,14)], [(6,0),(6,14)], [(0,0),(6,14)], [(2,15.3),(3,16.3),(4,15.3)]]
G['Л'] = [[(0.3,0),(1.6,10),(2.2,14),(5,14),(5,0)]]
G['П'] = [[(0,0),(0,14),(6,14),(6,0)]]
G['Ф'] = [[(3,15),(3,-1)], ell(3,7,2.6,5)]
G['Ц'] = [[(0,14),(0,0),(5.2,0),(5.2,14)], [(5.2,0),(6,0),(6,-2.3)]]
G['Ч'] = [[(0,14),(0,9),(1.8,7),(6,7)], [(6,14),(6,0)]]
G['Ш'] = [[(0,14),(0,0)], [(3,14),(3,0)], [(6,14),(6,0)], [(0,0),(6,0)]]
G['Щ'] = [[(0,14),(0,0)], [(3,14),(3,0)], [(5.4,14),(5.4,0)], [(0,0),(6,0)], [(6,0),(6,-2.3)]]
G['Ъ'] = [[(0,14),(1.8,14)], [(1.4,14),(1.4,0),(4,0),(6,2),(4.5,5),(1.4,5)]]
G['Ы'] = [[(0,14),(0,0),(2.5,0),(4,1.8),(4,5.2),(2.5,7),(0,7)], [(6,14),(6,0)]]
G['Ь'] = [[(0,14),(0,0),(2.8,0),(4.6,1.8),(4.6,5.2),(2.8,7),(0,7)]]
G['Э'] = [[(0,12),(2,14),(4,14),(6,11),(6,3),(4,0),(2,0),(0,2)], [(3,7),(6,7)]]
G['Ю'] = [[(0,0),(0,14)], [(0,7),(2,7)], [[]]]  # заполнится ниже
G['Ю'] = [[(0,0),(0,14)], [(0,7),(2,7)]] + [ [ (p[0]+2.0, p[1]) for p in ell(2.2,7,2.0,7,18) ] ]
G['Я'] = [[(6,0),(6,14),(2,14),(0,12),(0,9.5),(2,7),(6,7)], [(2.4,7),(0,0)]]
G['Ё'] = [[(6,14),(0,14),(0,0),(6,0)], [(0,7),(4.5,7)], [(1.6,16),(1.6,16)], [(4.4,16),(4.4,16)]]

# ширины клеток для узких знаков
WIDTH = {'I':6,'.':6,',':6,'!':6,':':6,';':6,"'":6,'1':6,'J':6}

# карта код -> имя глифа (строчные -> прописные; кириллические двойники латиницы)
def build_cmap():
    cmap = {}
    # латиница
    for ch in 'ABCDEFGHIJKLMNOPQRSTUVWXYZ':
        cmap[ord(ch)] = ch; cmap[ord(ch.lower())] = ch
    for ch in '0123456789':
        cmap[ord(ch)] = ch
    for ch in ".,!?:;-+=/()\"'*%#":
        cmap[ord(ch)] = ch
    cmap[0x2014] = u'—'; cmap[0x2013] = u'—'
    cmap[0x20] = 'space'
    # кириллица: совпадающие с латиницей
    same = {'А':'A','В':'B','Е':'E','К':'K','М':'M','Н':'H','О':'O','Р':'P','С':'C','Т':'T','У':'Y','Х':'X'}
    cyr = 'АБВГДЕЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯ'
    for ch in cyr:
        key = same.get(ch, ch)
        cmap[ord(ch)] = key
        cmap[ord(ch.lower())] = key
    cmap[ord('Ё')] = 'Ё'; cmap[ord('ё')] = 'Ё'
    return cmap

CMAP = build_cmap()

# --- отрисовка штрихов в контуры (в единицах шрифта) ---
def cw(poly):
    a = 0.0
    for i in range(len(poly)):
        x0, y0 = poly[i]; x1, y1 = poly[(i + 1) % len(poly)]
        a += x0 * y1 - x1 * y0
    return poly[::-1] if a > 0 else poly

def tu(p, st):
    x = p[0] * SX * st['sx']; y = p[1] * SY * st['sy']
    x += st['slant'] * (y / EM)
    return (x, y)

def disc(x, y, r):
    return cw([(x + r * math.cos(math.radians(a)), y + r * math.sin(math.radians(a)))
               for a in range(0, 360, 45)])

def seg_quad(p0, p1, hw, cap):
    x0, y0 = p0; x1, y1 = p1
    dx, dy = x1 - x0, y1 - y0; L = math.hypot(dx, dy)
    if L < 1e-6: return None
    nx, ny = -dy / L * hw, dx / L * hw
    if cap == 'square':
        ex, ey = dx / L * hw, dy / L * hw
        x0 -= ex; y0 -= ey; x1 += ex; y1 += ey
    return cw([(x0 + nx, y0 + ny), (x1 + nx, y1 + ny), (x1 - nx, y1 - ny), (x0 - nx, y0 - ny)])

def render(strokes, st):
    hw = st['w'] / 2.0
    out = []
    for pl in strokes:
        pts = [tu(p, st) for p in pl]
        if len(pts) == 1 or (len(pts) == 2 and pts[0] == pts[1]):
            out.append(disc(pts[0][0], pts[0][1], max(hw, st['w'] * 0.6)))   # точка
            continue
        eff = st.get('eff')
        if eff == 'dot':
            # пунктиром из точек вдоль линии
            for i in range(len(pts) - 1):
                x0, y0 = pts[i]; x1, y1 = pts[i + 1]
                L = math.hypot(x1 - x0, y1 - y0); step = hw * 2.4; n = max(1, int(L / step))
                for k in range(n + 1):
                    t = k / n; out.append(disc(x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, hw * 0.95))
            continue
        if eff == 'dash':
            for i in range(len(pts) - 1):
                x0, y0 = pts[i]; x1, y1 = pts[i + 1]
                L = math.hypot(x1 - x0, y1 - y0); dl = hw * 2.6
                if L < 1e-6: continue
                ux, uy = (x1 - x0) / L, (y1 - y0) / L; d = 0
                while d < L:
                    a = d; b = min(L, d + dl)
                    q = seg_quad((x0 + ux * a, y0 + uy * a), (x0 + ux * b, y0 + uy * b), hw, 'butt')
                    if q: out.append(q)
                    d += dl * 1.8
            continue
        cap = st.get('cap', 'round')
        for i in range(len(pts) - 1):
            q = seg_quad(pts[i], pts[i + 1], hw, cap)
            if q: out.append(q)
        if cap == 'round':
            for p in pts:
                out.append(disc(p[0], p[1], hw))
        if st.get('serif'):
            for p in (pts[0], pts[-1]):
                out.append(seg_quad((p[0] - hw * 1.6, p[1]), (p[0] + hw * 1.6, p[1]), hw * 0.7, 'butt'))
    return out

def glyph_width(key, st):
    w = WIDTH.get(key, 6)
    return int(w * SX * st['sx'] + 2 * GAP)

def build_font(st, path):
    fb = FontBuilder(EM, isTTF=True)
    order = ['.notdef', 'space'] + [k for k in G.keys()]
    glyphs = {}
    advances = {}
    pen = TTGlyphPen(None); glyphs['.notdef'] = pen.glyph(); advances['.notdef'] = int(4 * SX)
    pen = TTGlyphPen(None); glyphs['space'] = pen.glyph(); advances['space'] = int(3 * SX)
    for key, strokes in G.items():
        pen = TTGlyphPen(None)
        for contour in render(strokes, st):
            if len(contour) < 3: continue
            pen.moveTo(contour[0])
            for pt in contour[1:]:
                pen.lineTo(pt)
            pen.closePath()
        glyphs[key] = pen.glyph()
        advances[key] = glyph_width(key, st)
    fb.setupGlyphOrder(order)
    fb.setupCharacterMap({cp: g for cp, g in CMAP.items() if g in glyphs})
    fb.setupGlyf(glyphs)
    metrics = {g: (advances.get(g, int(6 * SX)), 0) for g in order}
    fb.setupHorizontalMetrics(metrics)
    fb.setupHorizontalHeader(ascent=820, descent=-220)
    fam = st['name']
    fb.setupNameTable({"familyName": fam, "styleName": "Regular",
                       "fullName": fam, "psName": fam.replace(' ', ''),
                       "version": "1.0", "copyright": "Elements original fonts"})
    fb.setupOS2(sTypoAscender=820, sTypoDescender=-220, usWinAscent=880, usWinDescent=260)
    fb.setupPost()
    fb.font["post"].formatType = 3.0   # без имён глифов (имена кириллические, не latin-1)
    fb.save(path)

# --- 30 стилей ---
STYLES = [
    dict(name="Elm Hairline",   w=28,  cap='round',  sx=1.0, sy=1.0, slant=0),
    dict(name="Elm Thin",       w=44,  cap='round',  sx=1.0, sy=1.0, slant=0),
    dict(name="Elm Regular",    w=70,  cap='round',  sx=1.0, sy=1.0, slant=0),
    dict(name="Elm Medium",     w=95,  cap='round',  sx=1.0, sy=1.0, slant=0),
    dict(name="Elm Bold",       w=130, cap='round',  sx=1.0, sy=1.0, slant=0),
    dict(name="Elm Heavy",      w=175, cap='round',  sx=1.05, sy=1.0, slant=0),
    dict(name="Elm Square",     w=85,  cap='square', sx=1.0, sy=1.0, slant=0),
    dict(name="Elm Square Bold",w=150, cap='square', sx=1.05, sy=1.0, slant=0),
    dict(name="Elm Butt",       w=80,  cap='butt',   sx=1.0, sy=1.0, slant=0),
    dict(name="Elm Italic",     w=75,  cap='round',  sx=1.0, sy=1.0, slant=170),
    dict(name="Elm Italic Bold",w=135, cap='round',  sx=1.0, sy=1.0, slant=170),
    dict(name="Elm Thin Italic",w=42,  cap='round',  sx=1.0, sy=1.0, slant=200),
    dict(name="Elm Condensed",  w=80,  cap='round',  sx=0.72, sy=1.0, slant=0),
    dict(name="Elm Cond Bold",  w=140, cap='square', sx=0.72, sy=1.0, slant=0),
    dict(name="Elm Wide",       w=85,  cap='round',  sx=1.32, sy=1.0, slant=0),
    dict(name="Elm Wide Bold",  w=150, cap='round',  sx=1.32, sy=1.0, slant=0),
    dict(name="Elm Tall",       w=70,  cap='round',  sx=0.9, sy=1.2, slant=0),
    dict(name="Elm Short",      w=95,  cap='square', sx=1.1, sy=0.8, slant=0),
    dict(name="Elm Serif",      w=70,  cap='butt',   sx=1.0, sy=1.0, slant=0, serif=True),
    dict(name="Elm Slab",       w=120, cap='butt',   sx=1.05, sy=1.0, slant=0, serif=True),
    dict(name="Elm Dotted",     w=70,  cap='round',  sx=1.0, sy=1.0, slant=0, eff='dot'),
    dict(name="Elm Dotted Bold",w=115, cap='round',  sx=1.05, sy=1.0, slant=0, eff='dot'),
    dict(name="Elm Dashed",     w=70,  cap='butt',   sx=1.0, sy=1.0, slant=0, eff='dash'),
    dict(name="Elm Marker",     w=165, cap='round',  sx=1.0, sy=1.0, slant=60),
    dict(name="Elm Pen",        w=55,  cap='round',  sx=1.0, sy=1.0, slant=120),
    dict(name="Elm Mono",       w=85,  cap='square', sx=0.95, sy=1.0, slant=0),
    dict(name="Elm Mono Thin",  w=48,  cap='square', sx=0.95, sy=1.0, slant=0),
    dict(name="Elm Block",      w=195, cap='square', sx=1.1, sy=1.0, slant=0),
    dict(name="Elm Round Bold", w=155, cap='round',  sx=1.08, sy=1.05, slant=0),
    dict(name="Elm Script",     w=62,  cap='round',  sx=1.0, sy=1.0, slant=210),
]

def main():
    out = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'fonts')
    os.makedirs(out, exist_ok=True)
    for i, st in enumerate(STYLES, 1):
        st.setdefault('slant', 0)
        path = os.path.join(out, "elm%02d.ttf" % i)
        build_font(st, path)
        print("%2d %-16s -> %s" % (i, st['name'], os.path.basename(path)))
    print("done: %d fonts" % len(STYLES))

if __name__ == '__main__':
    main()
