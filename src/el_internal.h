#pragma once
// Внутреннее устройство Elements. Один заголовок на все модули: структура
// элемента, общий контекст (дерево, тема, ввод, GDI+), помощники рисования.
#ifndef UNICODE
#define UNICODE
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <algorithm>
#include <windows.h>
#include <objidl.h>                                        // IStream — нужен gdiplus.h при WIN32_LEAN_AND_MEAN
namespace Gdiplus { using std::min; using std::max; }   // gdiplus.h использует min/max внутри своего namespace
#include <gdiplus.h>
#include <string>
#include <vector>
#include <cstdint>

namespace eli {

// Публичный API включаем ВНУТРИ namespace: его объявления с linkage "C"
// попадают в область eli, и наши определения тех же функций (тоже внутри eli)
// получают C-компоновку — ровно те символы el_*, что ждут программы.
#include "../include/elements.h"

typedef uint32_t u32;
typedef uint8_t  u8;

// Виды элементов (порядок свой, но согласован внутри библиотеки).
enum Kind {
    K_FREE, K_WINDOW, K_STACK, K_DOCK, K_WRAP, K_GRID, K_SCROLL, K_CANVAS, K_BORDER, K_GROUP, K_TABS, K_TAB,
    K_TEXT, K_BUTTON, K_CHECK, K_RADIO, K_EDIT, K_MEMO, K_PASSWORD, K_COMBO, K_LIST, K_SLIDER, K_PROGRESS,
    K_IMAGE, K_SEP, K_MENUBAR, K_MENU, K_NUMBER, K_TOOLBAR, K_TOOL, K_PAINT, K_DIALOG, K_SPACER
};
enum { A_STRETCH, A_START, A_CENTER, A_END };
enum { D_LEFT, D_TOP, D_RIGHT, D_BOTTOM };

#define EL_INF (1 << 28)
#define GMAX 24

struct GridDef {
    int nc, nr;
    u8  ct[GMAX], rt[GMAX];   // 0 auto, 1 pixels, 2 star
    int cv[GMAX], rv[GMAX];   // пиксели или вес доли *100
    int cs[GMAX], rs[GMAX];   // посчитанные размеры
    int co[GMAX], ro[GMAX];   // смещения
};

struct Anim {
    bool on;
    char prop[12];
    int from, to, ms, easing, loop;
    double t0;
    int dir;                  // для loop==2
};

// Стиль-правило из таблицы (el_stylesheet): селектор + свойства строкой.
struct Rule {
    std::string sel;          // "button", ".card", "#name", "button.card"
    int state;                // 0 обычное, 1 :hover, 2 :pressed
    std::string css;
};

struct Element {
    int kind;
    int parent, first, last, next;
    // раскладка
    int w, h, minw, minh, maxw, maxh;        // -1 — по содержимому
    int ml, mt, mr, mb, pl, pt, pr, pb;
    int ha, va, dock, row, col, rspan, cspan, px, py, gap;
    int hdr_x, hdr_w, strip_h;               // вкладки: прямоугольник ярлыка / высота полосы
    bool hidden, disabled, wrap, horiz;
    int dw, dh;                              // желаемый размер с полями
    int ax, ay, aw, ah;                      // размещён (координаты клиента окна)
    // цвета/рамка/шрифт
    bool has_bg, has_fg, has_bd;
    u32 bg, fg, bd;
    int bw, radius, fsize, bold;             // толщина рамки, скругление (-1 из темы), шрифт
    // текст и привязки
    std::string text, tip, name, cls, inl;
    std::string accel;                       // часть после \t в пункте меню
    const char *live;                        // показывать строку программы как есть
    int *iv; int ival, vmin, vmax;           // привязанный int; значение радио; пределы; выбранная вкладка
    char *buf; int cap;                      // текст edit/memo
    const char *const *items; int count;     // combo/list
    int scroll, hscroll;                     // прокрутка
    int caret, anchor;                       // edit/memo (байты)
    // оформление
    u32 bg2; int grad, bg_a;                 // второй цвет; 1 верт 2 гор 3 диаг; альфа фона (0 — непрозрачно)
    int sh_blur, sh_dx, sh_dy, sh_a; u32 sh_col;
    u32 hbg, pbg, hfg, hbd; bool has_hbg, has_pbg, has_hfg, has_hbd;
    double hv;                               // наведение 0..1 (плавно)
    int talign, mono;
    bool styled_gen_applied;                 // применённое поколение таблицы
    int styled_gen;
    GridDef *grid;
    int ctx;                                 // контекстное меню
    el_fn on_click; void *click_arg;
    el_fn on_change; void *change_arg;
    bool fl_click, fl_change;
    // картинка
    Gdiplus::Bitmap *img;
    // трансформ (анимации)
    int tx, ty, sc, rot, op;
    Anim anim;
    // меню
    int *menu_val;                           // el_menu_check
    bool menu_open;
    bool is_bar;                             // элемент меню — пункт полосы
    // число
    int step;
    // своё рисование
    void (*paint_fn)(el, int, int, void *); void *paint_arg;
    bool needs_redraw;
    // данные / панель инструментов
    void *data;
    bool flat;
    std::string icon; Gdiplus::Bitmap *icon_img; int icon_size;
    // живость
    bool alive;
};

struct KeyEv { int vk; wchar_t ch; int mods; bool down; };
struct Timer { int id; int ms; double next; el_fn fn; void *arg; bool dead; };
struct Hotkey { int mods; int vk; el_fn fn; void *arg; };

struct Popup { int menu; int x, y; int hover; };  // открытое выпадающее меню
struct Effect { std::string name; int amount; };  // постобработка кадра

struct Ctx {
    std::vector<Element> el;                 // индекс 0 не используется (0 = нет)
    int window;
    HWND hwnd;
    bool quit, close_req, inited;
    el_fn on_close; void *on_close_arg;
    // тема
    int dark;
    u32 th[16];
    u32 accent;
    int def_font;
    int corner;
    int theme_idx;
    // ввод
    int mx, my, pmx, pmy;
    int buttons, pbuttons;                   // 1 левая, 2 правая
    int wheel;                               // накоплено за кадр
    int wheel_consumed_by;
    std::vector<KeyEv> keys;
    int focus, hot, active;
    int press_x, press_y;                    // где нажали (для перетаскивания)
    bool dragging;
    double drag_ox, drag_oy;
    // DPI
    double dpi;                              // 1.0 при 96
    // GDI+ (буфер кадра — поверх DIB-секции, чтобы годился и для послойного окна)
    ULONG_PTR gdip_token;
    Gdiplus::Bitmap *back; Gdiplus::Graphics *g; int back_w, back_h;
    HDC memdc; HBITMAP dib; void *dibbits;
    Gdiplus::Graphics *measure_g; Gdiplus::Bitmap *measure_bmp;
    // кадр
    double now_ms, last_ms;
    bool dirty;
    bool close_guard;
    // всплывающие меню
    std::vector<Popup> popups;
    // контекстное меню — отдельные элементы K_MENU, корни
    // подсказка
    int tip_el; double tip_since; bool tip_show;
    // таймеры/анимации/клавиши
    std::vector<Timer> timers; int timer_seq;
    std::vector<Hotkey> hotkeys;
    // таблица стилей
    std::vector<Rule> rules; int rules_gen;
    // нестандартное окно
    bool frameless;
    int  win_shape;                          // 0 rect,1 round,2 ellipse,3 polygon,4 pixels(по альфе)
    int  win_round;                          // радиус скругления (точки)
    std::vector<int> region_pts;             // контур (пары x,y в точках)
    int  aspect_w, aspect_h;
    int  win_opacity;                        // 0..255
    bool layered;
    int  drag_region;                        // элемент-«шапка» для перетаскивания
    int  win_minw, win_minh, win_maxw, win_maxh;
    int  win_canvas;                         // поверхность во весь фон окна (K_PAINT)
    bool shape_dirty;
    bool resizable;
    // диалоги/оверлей
    int modal;                               // активный диалог (0 — нет)
    std::string msg_title, msg_text;
    int msg_kind;                            // 0 нет,1 message,2 ask
    void (*ask_fn)(int, void*); void *ask_arg;
    // тост
    std::string toast_text; double toast_until; double toast_t0;
    // своё рисование — активная поверхность при вызове el_g_*
    int cur_paint; int paint_ox, paint_oy;
    // выпадающий список combo
    int combo_open, combo_hover, combo_scroll;
    // эффекты (постобработка кадра)
    std::vector<Effect> effects;
    std::vector<u32> fx_tmp;
};

extern Ctx g;

// --- доступ к элементам ---
inline Element &E(int h) { return g.el[h]; }
inline bool valid(int h) { return h > 0 && h < (int)g.el.size() && g.el[h].alive; }
int  el_alloc(int kind);
void el_add_child(int parent, int child);

// --- тема ---
void theme_init();
u32  th(int slot);                           // цвет слота темы
void resolve_default(Element &e);            // проставить тему элементу при создании

// --- цвета ---
inline Gdiplus::Color gc(u32 rgb) { return Gdiplus::Color(255, (rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255); }
inline Gdiplus::Color gca(u32 rgb, int a) { return Gdiplus::Color((BYTE)a, (rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255); }
u32 mix(u32 a, u32 b, double t);

// --- шрифты и текст ---
Gdiplus::Font *get_font(int size, int bold, int face);
int  font_face_by_name(const char *name);
int  text_w(const std::wstring &s, int size, int bold, int mono);
void text_size(const std::wstring &s, int size, int bold, int mono, bool wrapW, int maxw, int *w, int *h);
std::wstring wide(const char *s);
std::string  narrow(const std::wstring &s);
int  px(int dip);                            // точки интерфейса -> пиксели (HiDPI)
int  dip(int pxv);

// --- раскладка ---
void measure(int h, int availw, int availh);
void arrange(int h, int x, int y, int w, int ww, int hh);  // slot (x,y,ww,hh)
void layout_root();

// --- рисование дерева ---
void draw_tree();
void draw_element(int h);
void round_rect_path(Gdiplus::GraphicsPath &p, Gdiplus::RectF r, float rad);
void fill_bg(Element &e, Gdiplus::RectF r, u32 color);
Gdiplus::Graphics *G();

// --- ввод и события ---
void input_frame();
void fire_click(int h);
void fire_change(int h);
int  hit_test(int root, int x, int y);
bool is_container(int kind);
bool is_focusable(int kind);
void set_focus(int h);

// --- меню ---
void menu_open_bar(int item);
void menu_close_all();
void menu_frame_input();
void menu_draw();
int  menu_measure_width(int menu);

// --- анимации/таймеры ---
void anim_frame();
bool anim_busy();
double ease(int kind, double t);
void transform_of(int h, double *tx, double *ty, double *sc, double *rot, double *op);

// --- стили ---
void apply_style_string(int h, const char *css);
void restyle_all();
void style_apply_sheet(int h);

// --- эффекты (постобработка) ---
void apply_effects();
bool effects_animated();

// --- диалоги/тосты ---
void overlay_frame_input();
void overlay_draw();
void draw_tooltip();
// --- combo ---
void combo_frame_input();
void combo_draw();

// --- edit/memo помощники ---
void edit_input(int h, const std::vector<KeyEv> &keys);
void edit_draw(int h);
int  caret_from_x(int h, int x);

// --- win32 ---
void ensure_back(int w, int h);
void present();
void win_create(const char *title, int w, int h);
void win_pump();                              // обработать очередь сообщений Windows
void win_wait(int timeout_ms);                // уснуть до сообщения/таймаута (экономия ЦП)
void win_set_title(const std::wstring &t);
void win_client_size(int *w, int *h);
void gdip_init();
void request_redraw();                        // пометить кадр грязным
void win_apply_shape();                       // наложить форму окна (регион/слой)
void win_move_begin();                        // начать перетаскивание окна за «шапку»

} // namespace eli
