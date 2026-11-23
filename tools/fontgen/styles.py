# 30 гарнитур: одни и те же оси букв, разное перо, контраст, засечки, ширина, высота строчных,
# скругление овалов, наклон. Толщина — вес штриха поперёк кромки пера (единицы шрифта).
from pen import Nib


class Style:
    def __init__(self, name, weight=86, contrast=1.0, angle=0.0, power=1.0, sq=2.2, wf=1.0, xh=0.73, slant=0.0,
                 serif=None, rounded=False, mono=False, a_single=False, dot=None, cap=700, ov=0.016,
                 serif_len=None, serif_th=0.4):
        self.name = name
        self.file = name.replace(" ", "") + ".ttf"
        self.weight = float(weight)
        self.CAP = cap
        self.XH = int(round(cap * xh))
        self.ASC = int(round(cap * 1.05))
        self.DESC = -int(round(cap * 0.3))
        self.nib = Nib(weight, weight * contrast, angle, power)
        self.sq = sq
        self.wf = wf
        self.slant = slant
        self.serif = serif
        self.round = rounded
        self.mono = mono
        self.mono_w = 600.0 * wf
        self.a_single = a_single
        self.dot = dot or ("round" if (rounded or sq <= 2.1) else "square")
        self.ov = ov
        base = {"slab": 0.62, "bracket": 0.58, "hair": 0.62, "wedge": 0.52}.get(serif, 0.55)
        self.serif_len = serif_len if serif_len is not None else base * weight + 22
        self.serif_th = serif_th
        # поле до края знака; у антикв засечки сами дают воздух — поле меньше
        self.sb = (24 + 0.22 * weight) if serif else (40 + 0.45 * weight)
        self.space = (600.0 if mono else 250.0 + 0.4 * weight) * (wf if not mono else 1.0)


STYLES = [
    Style("Elm Grotesk",            weight=86,  contrast=0.86, sq=2.25),
    Style("Elm Grotesk Light",      weight=50,  contrast=0.9,  sq=2.25),
    Style("Elm Grotesk Bold",       weight=146, contrast=0.84, sq=2.25, wf=1.03),
    Style("Elm Grotesk Black",      weight=205, contrast=0.82, sq=2.3,  wf=1.08),
    Style("Elm Grotesk Italic",     weight=86,  contrast=0.86, sq=2.25, slant=11),
    Style("Elm Grotesk Condensed",  weight=92,  contrast=0.86, sq=2.4,  wf=0.78, xh=0.76),
    Style("Elm Grotesk Extended",   weight=96,  contrast=0.86, sq=2.25, wf=1.3,  xh=0.71),
    Style("Elm Compressed",         weight=128, contrast=0.8,  sq=2.7,  wf=0.7,  xh=0.78),
    Style("Elm Geometric",          weight=80,  contrast=1.0,  sq=2.0,  xh=0.68, a_single=True),
    Style("Elm Geometric Light",    weight=44,  contrast=1.0,  sq=2.0,  xh=0.68, a_single=True),
    Style("Elm Geometric Bold",     weight=142, contrast=1.0,  sq=2.0,  xh=0.68, a_single=True, wf=1.04),
    Style("Elm Humanist",           weight=86,  contrast=0.62, angle=24, power=1.6, sq=2.12, wf=0.95, xh=0.71),
    Style("Elm Humanist Bold",      weight=142, contrast=0.62, angle=24, power=1.6, sq=2.12, wf=0.98, xh=0.71),
    Style("Elm Humanist Italic",    weight=86,  contrast=0.62, angle=24, power=1.6, sq=2.12, wf=0.92, xh=0.71, slant=12, a_single=True),
    Style("Elm Rounded",            weight=96,  contrast=1.0,  sq=2.35, rounded=True),
    Style("Elm Rounded Bold",       weight=154, contrast=1.0,  sq=2.35, rounded=True, wf=1.04),
    Style("Elm Techno",             weight=90,  contrast=1.0,  sq=4.2,  wf=1.06, dot="square"),
    Style("Elm Techno Bold",        weight=146, contrast=1.0,  sq=4.2,  wf=1.1,  dot="square"),
    Style("Elm Mono",               weight=84,  contrast=1.0,  sq=2.3,  mono=True),
    Style("Elm Mono Bold",          weight=132, contrast=1.0,  sq=2.3,  mono=True),
    Style("Elm Typewriter",         weight=68,  contrast=1.0,  sq=2.1,  mono=True, serif="slab"),
    Style("Elm Slab",               weight=88,  contrast=0.92, sq=2.2,  serif="slab", xh=0.72),
    Style("Elm Slab Bold",          weight=148, contrast=0.9,  sq=2.2,  serif="slab", xh=0.72, wf=1.04),
    Style("Elm Serif",              weight=96,  contrast=0.3,  angle=16, power=2.6, sq=2.0, wf=0.92, xh=0.66, serif="bracket"),
    Style("Elm Serif Bold",         weight=156, contrast=0.3,  angle=16, power=2.6, sq=2.0, wf=0.96, xh=0.66, serif="bracket"),
    Style("Elm Serif Italic",       weight=96,  contrast=0.3,  angle=16, power=2.6, sq=2.0, wf=0.9,  xh=0.66, serif="bracket", slant=13, a_single=True),
    Style("Elm Didone",             weight=124, contrast=0.12, angle=12, power=3.2, sq=2.0, wf=0.94, xh=0.64, serif="hair"),
    Style("Elm Oldstyle",           weight=88,  contrast=0.42, angle=30, power=1.8, sq=2.05, wf=0.94, xh=0.63, serif="wedge"),
    Style("Elm Hairline",           weight=26,  contrast=1.0,  sq=2.1,  wf=1.05),
    Style("Elm Wide Light",         weight=50,  contrast=1.0,  sq=2.25, wf=1.42, xh=0.71),
]
