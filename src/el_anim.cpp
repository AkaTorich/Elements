// Анимации и таймеры Elements: плавности, трансформации (сдвиг/масштаб/поворот/
// прозрачность), появление элементов, периодические вызовы.
#include "el_internal.h"
#include <cmath>
#include <cstring>

namespace eli {

double ease(int kind, double t) {
    if (t < 0) t = 0; if (t > 1) t = 1;
    switch (kind) {
    case EL_EASE_IN: return t * t;
    case EL_EASE_OUT: return 1 - (1 - t) * (1 - t);
    case EL_EASE_IN_OUT: return t < 0.5 ? 2 * t * t : 1 - std::pow(-2 * t + 2, 2) / 2;
    case EL_OVERSHOOT: { double c1 = 1.70158, c3 = c1 + 1; double x = t - 1; return 1 + c3 * x * x * x + c1 * x * x; }
    case EL_BOUNCE: { double n1 = 7.5625, d1 = 2.75; if (t < 1 / d1) return n1 * t * t; else if (t < 2 / d1) { t -= 1.5 / d1; return n1 * t * t + 0.75; } else if (t < 2.5 / d1) { t -= 2.25 / d1; return n1 * t * t + 0.9375; } else { t -= 2.625 / d1; return n1 * t * t + 0.984375; } }
    default: return t;
    }
}

void el_animate(el e, const char *prop, int from, int to, int ms, int easing, int loop) {
    if (!valid(e) || !prop) return;
    Anim &a = g.el[e].anim;
    a.on = true;
    strncpy(a.prop, prop, sizeof(a.prop) - 1); a.prop[sizeof(a.prop) - 1] = 0;
    a.from = from; a.to = to; a.ms = ms <= 0 ? 1 : ms; a.easing = easing; a.loop = loop;
    a.t0 = g.now_ms; a.dir = 1;
    request_redraw();
}

void el_appear(el e, const char *how, int ms) {
    if (!valid(e) || !how) return;
    std::string h = how;
    if (h == "fade") el_animate(e, "opacity", 0, 255, ms, EL_EASE_OUT, 0);
    else if (h == "slide" || h == "left") el_animate(e, "x", -30, 0, ms, EL_EASE_OUT, 0);
    else if (h == "right") el_animate(e, "x", 30, 0, ms, EL_EASE_OUT, 0);
    else if (h == "zoom") el_animate(e, "scale", 70, 100, ms, EL_EASE_OUT, 0);
    else if (h == "pop") el_animate(e, "scale", 0, 100, ms, EL_OVERSHOOT, 0);
    else if (h == "spin") el_animate(e, "rotate", -120, 0, ms, EL_EASE_OUT, 0);
    else el_animate(e, "opacity", 0, 255, ms, EL_EASE_OUT, 0);
}

void el_transform(el e, int x, int y, int scale, int rotate, int opacity) {
    if (!valid(e)) return;
    Element &el = g.el[e];
    el.tx = x; el.ty = y; el.sc = scale; el.rot = rotate; el.op = opacity;
    request_redraw();
}

int el_animating(el e) { return valid(e) && g.el[e].anim.on; }
void el_stop(el e) { if (valid(e)) g.el[e].anim.on = false; }

void transform_of(int h, double *tx, double *ty, double *sc, double *rot, double *op) {
    Element &e = g.el[h];
    *tx = e.tx; *ty = e.ty; *sc = e.sc; *rot = e.rot; *op = e.op;
    if (!e.anim.on) return;
    double t = (g.now_ms - e.anim.t0) / e.anim.ms;
    if (e.anim.loop == 0) { if (t > 1) t = 1; }
    else if (e.anim.loop == 1) { t = t - std::floor(t); }
    else { double cyc = std::fmod(t, 2.0); t = cyc <= 1 ? cyc : 2 - cyc; }
    double v = e.anim.from + (e.anim.to - e.anim.from) * ease(e.anim.easing, t);
    if (!strcmp(e.anim.prop, "opacity")) *op = v;
    else if (!strcmp(e.anim.prop, "x")) *tx = v;
    else if (!strcmp(e.anim.prop, "y")) *ty = v;
    else if (!strcmp(e.anim.prop, "scale")) *sc = v;
    else if (!strcmp(e.anim.prop, "rotate")) *rot = v;
}

void anim_frame() {
    // завершение одноразовых анимаций
    for (int i = 1; i < (int)g.el.size(); i++) {
        Element &e = g.el[i];
        if (!e.alive || !e.anim.on) continue;
        if (e.anim.loop == 0 && g.now_ms - e.anim.t0 >= e.anim.ms) {
            // зафиксировать конечное значение в базе
            if (!strcmp(e.anim.prop, "opacity")) e.op = e.anim.to;
            else if (!strcmp(e.anim.prop, "x")) e.tx = e.anim.to;
            else if (!strcmp(e.anim.prop, "y")) e.ty = e.anim.to;
            else if (!strcmp(e.anim.prop, "scale")) e.sc = e.anim.to;
            else if (!strcmp(e.anim.prop, "rotate")) e.rot = e.anim.to;
            e.anim.on = false;
        }
    }
    // таймеры
    for (auto &t : g.timers) {
        if (t.dead) continue;
        if (g.now_ms >= t.next) { t.next = g.now_ms + t.ms; if (t.fn) t.fn(0, t.arg); request_redraw(); }
    }
    // уборка мёртвых таймеров
    for (size_t i = 0; i < g.timers.size();) { if (g.timers[i].dead) g.timers.erase(g.timers.begin() + i); else i++; }
}

bool anim_busy() {
    for (int i = 1; i < (int)g.el.size(); i++) if (g.el[i].alive && g.el[i].anim.on) return true;
    for (auto &t : g.timers) if (!t.dead) return true;
    return false;
}

int el_every(int ms, el_fn fn, void *arg) {
    Timer t; t.id = ++g.timer_seq; t.ms = ms <= 0 ? 1 : ms; t.next = g.now_ms + t.ms; t.fn = fn; t.arg = arg; t.dead = false;
    g.timers.push_back(t);
    return t.id;
}
void el_timer_stop(int id) { for (auto &t : g.timers) if (t.id == id) t.dead = true; }

} // namespace eli
