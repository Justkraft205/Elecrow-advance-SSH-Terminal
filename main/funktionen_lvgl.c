#include "funktionen_lvgl.h"

LV_FONT_DECLARE(lv_font_unscii_8);

extern lv_obj_t *keyboard_container;
extern lv_obj_t *active_field;
int line2 = 0;

void inputfields(int x, int y, int xs, int ys, const char *name)
{
    lv_obj_t *button = lv_button_create(lv_screen_active());
    lv_obj_set_size(button, xs, ys);
    lv_obj_set_pos(button, x, y);

    if (keyboard_container != NULL) {
        lv_obj_move_background(keyboard_container);
    }

    lv_obj_move_foreground(button);

    lv_obj_t *label_button = lv_label_create(button);
        if (strcmp(name, "befehl") == 0) {
    active_field = label_button;
}
    lv_label_set_text(label_button, name);
    lv_obj_set_style_text_font(label_button, &lv_font_unscii_8, 0);
    lv_obj_set_style_text_color(label_button, lv_color_hex(0xFFFFFF), 0);

    lv_obj_move_foreground(label_button);
    lv_obj_center(label_button);

    lv_obj_add_event_cb(button, activ_text, LV_EVENT_CLICKED, label_button);
}

void logkonsole(const char *name){
    if (line >= 100) return;
    for (int i = 0; i < line; i++) {
        label_y[i] -= 20;
        lv_obj_set_pos(label_button[i], 30, label_y[i]);}
    label_button[line] = lv_label_create(label);
    lv_obj_set_layout(label, LV_LAYOUT_NONE);
    lv_label_set_text(label_button[line], name);
    lv_obj_set_style_text_font(label_button[line], &lv_font_unscii_8, 0);
    lv_obj_set_style_text_color(label_button[line], lv_color_black(), 0);
    lv_obj_set_size(label_button[line], 900, 20);
    lv_label_set_long_mode(label_button[line], LV_LABEL_LONG_CLIP);
    label_y[line] = 300;
    lv_obj_set_pos(label_button[line], 30, label_y[line]);
    line++;
}

void chkonsole(const char *name){
    line2  = line -1;
lv_label_set_text(label_button[line2],name);
}
