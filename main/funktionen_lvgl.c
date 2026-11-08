#include "funktionen_lvgl.h"
#include <string.h>
#include "esp_log.h"
#include <stdio.h>
LV_FONT_DECLARE(lv_font_unscii_8);
static const char *TAG = "ESP32";
extern lv_obj_t *keyboard_container;
extern lv_obj_t *active_field;
lv_obj_t *input_button[20];
int input_count = 0;
int line2 = 0;
int ylinee = 0;
static lv_obj_t *keyboard(const char *button_text,int x, int y, int sx, int sy, int check,lv_event_cb_t callback);
extern void button_event(lv_event_t *e);

const char *gbuchstaben[] = {
    "Q", "W", "E", "R", "T", "Z",
    "U", "I", "O", "P", "<-", "A",
    "S", "D", "F", "G", "H", "J",
    "K", "L", ":", "123", "<>", "Y",
    "X", "C", "V", "!_", "_!", "B",
    "N", "M", "->", "^^", "UP"
};

const char *kbuchstaben[] = {
    "q", "w", "e", "r", "t", "z",
    "u", "i", "o", "p", "<-", "a",
    "s", "d", "f", "g", "h", "j",
    "k", "l", ".", "123", "<>", "y",
    "x", "c", "v", "!_", "_!", "b",
    "n", "m", "->", "^^"
};

const char *zahlen[] = {
    "1", "2", "3", "4", "5", "6",
    "7", "8", "9", "0", "<-", "+",
    "-", "*", "/", "=", "(", ")",
    "{", "}", "[", "]", "ABC", "$",
    "!", "?", "@", "!_!", ".", "<",
    ">", "#", "->", "^^"
};

void inputfields(int x, int y, int xs, int ys, const char *name)
{
    lv_obj_t *button = lv_button_create(lv_screen_active());

    lv_obj_set_size(button, xs, ys);
    lv_obj_set_pos(button, x, y);

    input_button[input_count] = button;
    input_count++;

    lv_obj_t *label_button = lv_label_create(button);

    if (strcmp(name, "befehl") == 0) {
        active_field = label_button;
    }

    lv_label_set_text(label_button, name);
    lv_obj_set_style_text_font(label_button, &lv_font_unscii_8, 0);
    lv_obj_set_style_text_color(label_button, lv_color_hex(0xFFFFFF), 0);

    lv_obj_center(label_button);

    lv_obj_add_event_cb(button, activ_text, LV_EVENT_CLICKED, label_button);
}

void logkonsole(const char *name){
    if (strcmp(name, "Erweiterung") == 0) {
        ESP_LOGI(TAG, "Erweiterung erkannt");
    ylinee = 510 - ((line - 1) * 20);

    for (int i = 0; i < line; i++) {
    label_y[i] = ylinee;
    lv_obj_set_pos(label_button[i], 30, label_y[i]);
    ylinee += 20;
}

    } else if (strcmp(name, "Back") == 0){
        ESP_LOGI(TAG, "Erweiterung rückgängig");
    ylinee = 300 - ((line - 1) * 20);

    for (int i = 0; i < line; i++) {
    label_y[i] = ylinee;
    lv_obj_set_pos(label_button[i], 30, label_y[i]);
    ylinee += 20;
    }
    }else{
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
}}

void chkonsole(const char *name) {
    line2 = line - 1;

    char buffer[256];

    snprintf(buffer, sizeof(buffer), "%s%s",
             lv_label_get_text(label_button[line2]), name);

    ESP_LOGI(TAG, "Neuer Text: %s", buffer);

    lv_label_set_text(label_button[line2], buffer);
}

void create_keyboard(int check, int shift2){
    if (keyboard_container != NULL) {
        lv_obj_delete(keyboard_container);
        keyboard_container = NULL;}
    if (check == 1) {
        lv_obj_set_size(label, 1024, 300);
        lv_obj_set_pos(label, 0, 0);
        if (active_field != NULL){ lv_obj_set_pos(input_button[0], 100, 330);}
        keyboard_container = lv_obj_create(lv_screen_active());
        lv_obj_set_size(keyboard_container, 1024, 280);
        lv_obj_set_pos(keyboard_container, 0, 320);
        lv_obj_set_style_bg_opa(keyboard_container, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(keyboard_container, 0, 0);
        lv_obj_set_style_pad_all(keyboard_container, 0, 0);
        int xh = 0;
        int yh = 0;
        for (int i = 0; i < 34; i++) {
            if (i == 33) {
                xh = 0;
                yh = 0;}else if (i < 11) {
                xh = i * 93;
                yh = 70;}else if (i < 22) {
                xh = (i - 11) * 93;
                yh = 140;}else {
                xh = (i - 22) * 93;
                yh = 210;}
            keyboard(shift2 == 2 ? zahlen[i] :shift2 == 1 ? kbuchstaben[i] :gbuchstaben[i],xh, yh, 93,70, check, button_event);}
    }else {
    lv_obj_set_size(label, 1024, 530);
    lv_obj_set_pos(label, 0, 0);
    keyboard_container = lv_obj_create(lv_screen_active());
    lv_obj_set_size(keyboard_container, 1024, 100);
    lv_obj_set_pos(keyboard_container, 0, 530);
    lv_obj_set_style_bg_opa(keyboard_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(keyboard_container, 0, 0);
    lv_obj_set_style_pad_all(keyboard_container, 0, 0);
    keyboard(gbuchstaben[33], 0, 550, 50, 50, check, button_event);
    lv_obj_set_pos(input_button[0], 100, 530);
    logkonsole("Erweiterung");}}

static lv_obj_t *keyboard(const char *button_text,int x, int y, int sx, int sy, int check,lv_event_cb_t callback){
    lv_obj_t *button;
    if (check == 1) {button = lv_button_create(keyboard_container);
    } else { button = lv_button_create(lv_screen_active());}
    lv_obj_remove_flag(keyboard_container, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(button, lv_color_make(128, 128, 128), 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x000000),LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_size(button, sx, sy);
    lv_obj_set_pos(button, x, y);
    lv_obj_set_style_radius(button, 0, 0);
    lv_obj_set_style_shadow_width(button, 0, 0);
    lv_obj_t *label_button = lv_label_create(button);
    lv_label_set_text(label_button, button_text);
    lv_obj_set_style_text_font(label_button, &lv_font_unscii_8, 0);
    lv_obj_set_style_text_color(label_button, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_opa(label_button, LV_OPA_COVER, 0);
    lv_obj_center(label_button);
    lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED,(void *)button_text);
    return button;
}
