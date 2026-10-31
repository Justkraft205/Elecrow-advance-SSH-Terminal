
#ifndef FUNKTIONEN_LVGL_H
#define FUNKTIONEN_LVGL_H

#include "lvgl.h"

extern lv_obj_t *active_field;
extern lv_obj_t *keyboard_container;

extern lv_obj_t *label_button[100];
extern lv_obj_t *label;
static int line;
static int label_y[100];

void activ_text(lv_event_t *e);

void inputfields(int x, int y, int xs, int ys, const char *name);
void logkonsole(const char *name);

#endif
