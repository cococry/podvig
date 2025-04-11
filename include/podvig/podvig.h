#pragma once 

#include <leif/leif.h>  

#define PV_CENTERED_X(s, w) ((pv_monitor_by_idx((s), pv_monitor_focused_idx((s))).size.x - (w)) / 2.0f)
#define PV_CENTERED_Y(s, h) ((pv_monitor_by_idx((s), pv_monitor_focused_idx((s))).size.y - (h)) / 2.0f)

typedef struct {
  uint32_t width, height;
  int32_t x, y;
  char* title;
  bool transparent_framebuffer;
  bool manage_interactive;
  bool hidden;
} pv_widget_data_t;

typedef enum {
  PV_WIDGET_FLAG_WIDTH = 1 << 0,
  PV_WIDGET_FLAG_HEIGHT = 1 << 1,
  PV_WIDGET_FLAG_POSX = 1 << 2,
  PV_WIDGET_FLAG_POSY = 1 << 3,
  PV_WIDGET_FLAG_TITLE = 1 << 4,
  PV_WIDGET_FLAG_TRANSPARENT_FRAMEBUFFER = 1 << 5,
  PV_WIDGET_FLAG_MANAGE_INTERACTIVE = 1 << 6,
  PV_WIDGET_FLAG_HIDDEN = 1 << 7,
  PV_WIDGET_FLAG_ALWAYS_ONTOP = 1 << 8,
} pv_widget_data_flag_t;

typedef void (*pv_widget_ui_layout_func_t)(lf_ui_state_t* ui);

typedef struct {
  lf_ui_state_t* ui;
  pv_widget_data_t data;
} pv_widget_t;

typedef struct {
  char* key;
  pv_widget_t value;
} _pv_widget_hm_item_t;

typedef struct {
  _pv_widget_hm_item_t* widgets; 
} pv_state_t;

pv_state_t* pv_init(void);
pv_widget_t pv_widget(pv_state_t* s, const char* name, pv_widget_ui_layout_func_t layout_cb,
    float x, float y, float w, float h);

pv_widget_t pv_widget_ex(
    pv_state_t* s, const char* name, 
    pv_widget_ui_layout_func_t layout_cb, 
    pv_widget_data_t* data, uint32_t flags);

void pv_widget_hide(pv_widget_t* widget);

void pv_widget_show(pv_widget_t* widget);

int32_t pv_monitor_focused_idx(pv_state_t* s);

lf_container_t pv_monitor_by_idx(pv_state_t* s, uint32_t idx);

pv_widget_t* pv_widget_by_name(pv_state_t* s, const char* name);

void pv_run(pv_state_t* s);

