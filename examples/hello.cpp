// Самый простой пример Elements: поле, кнопка, подпись.
#include <elements.h>
#include <stdio.h>

char name[64] = "Мир";
char out[128] = "Нажмите кнопку.";

void hi(el e, void *arg) { sprintf(out, "Привет, %s!", name[0] ? name : "друг"); }

int main(void)
{
    el w = el_window("Привет, Elements", 420, 230);
    el s = el_stack(w, 0);
    el_padding(s, 22, 22, 22, 22);
    el_gap(s, 12);

    el_text(s, "Как вас зовут?");
    el e = el_edit(s, name, sizeof(name));
    el_tooltip(e, "Введите имя и нажмите кнопку");
    el_on_click(e, hi, 0);                 // Enter в поле — тоже «привет»
    el_on_click(el_button(s, "Поздороваться"), hi, 0);

    el t = el_text(s, "");
    el_text_bind(t, out);
    el_font(t, 17, 1);

    el_run();
    return 0;
}
