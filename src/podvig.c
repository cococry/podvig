#include "../include/podvig/podvig.h"
#include <X11/Xlib.h>
#include <leif/leif.h>
#include <leif/ui_core.h>
#include <leif/util.h>
#include <leif/win.h>

#define STB_DS_IMPLEMENTATION
#include "../vendor/stb_ds.h"

#define INTERSECT(x,y,w,h,r)  (MAX(0, MIN((x)+(w),(r).x_org+(r).width)  - MAX((x),(r).x_org))) 

pv_state_t* 
pv_init(void) {
  pv_state_t* s = malloc(sizeof(*s));
  if(!s) {
    fprintf(stderr, "podvig: memory allocated for state failed.\n");
    return NULL;
  }
  s->widgets = NULL;
  if(lf_windowing_init() != 0) return NULL;
  return s;
}

pv_widget_t 
pv_widget(pv_state_t* s, const char* name, pv_widget_ui_layout_func_t layout_cb,
          float x, float y, float w, float h) {
  int32_t focused_mon_idx = pv_monitor_focused_idx(s);
  lf_container_t focused_mon = pv_monitor_by_idx(s, focused_mon_idx);
  pv_widget_data_t data = (pv_widget_data_t){
    .x = focused_mon.pos.x + x, 
    .y = focused_mon.pos.y + y,
    .width = w,
    .height = h,
  };

  return pv_widget_ex(s, name, layout_cb, &data, 
                      PV_WIDGET_FLAG_ALWAYS_ONTOP |
                      PV_WIDGET_FLAG_POSX | 
                      PV_WIDGET_FLAG_POSY | 
                      PV_WIDGET_FLAG_WIDTH | 
                      PV_WIDGET_FLAG_HEIGHT 
                      );
}

pv_widget_t 
pv_widget_ex(pv_state_t* s, const char* name, pv_widget_ui_layout_func_t layout_cb, pv_widget_data_t* data, uint32_t flags) {
  pv_widget_t widget = {0};
  uint32_t winx = 0, winy = 0, winw = 256, winh = 256; // 256 Is default size of widgets 
  bool transparent_framebuffer = true;
  bool hidden = false;
  uint32_t winflags = LF_WINDOWING_FLAG_X11_OVERRIDE_REDIRECT;
  if(data) {
    if(lf_flag_exists(&flags, PV_WIDGET_FLAG_POSX))   winx = data->x;
    if(lf_flag_exists(&flags, PV_WIDGET_FLAG_POSY))   winy = data->y;
    if(lf_flag_exists(&flags, PV_WIDGET_FLAG_WIDTH))  winw = data->width;
    if(lf_flag_exists(&flags, PV_WIDGET_FLAG_HEIGHT)) winh = data->height;
    if(lf_flag_exists(&flags, PV_WIDGET_FLAG_TRANSPARENT_FRAMEBUFFER)) transparent_framebuffer = data->transparent_framebuffer;
    if(lf_flag_exists(&flags, PV_WIDGET_FLAG_HIDDEN)) hidden = true; 
    if(lf_flag_exists(&flags, PV_WIDGET_FLAG_MANAGE_INTERACTIVE)) {
      if(data->manage_interactive) {
        lf_flag_unset(&winflags, LF_WINDOWING_FLAG_X11_OVERRIDE_REDIRECT);
      }
    }
  }
  if(flags & PV_WIDGET_FLAG_ALWAYS_ONTOP)
    lf_ui_core_set_window_hint(LF_WINDOWING_HINT_ABOVE, true);
  lf_ui_core_set_window_hint(LF_WINDOWING_HINT_POS_X, winx);
  lf_ui_core_set_window_hint(LF_WINDOWING_HINT_POS_Y, winy);
  lf_ui_core_set_window_hint(LF_WINDOWING_HINT_TRANSPARENT_FRAMEBUFFER, transparent_framebuffer);
  lf_ui_core_set_window_flags(winflags);
  lf_window_t win = lf_ui_core_create_window(
    winw, winh, 
    flags && lf_flag_exists(&flags, PV_WIDGET_FLAG_TITLE) ? data->title : "pv_widget");

  widget.ui = lf_ui_core_init(win);
  widget.ui->needs_render = true;
  widget.data = *data;
  if(layout_cb) {
    layout_cb(widget.ui);
  }
  if(hidden) {
    lf_win_hide(win);
  }
    
  shput(s->widgets, name, widget);

  return widget;
}

void 
pv_widget_hide(pv_widget_t* widget) {
  if(!widget || widget->data.hidden) return;
  lf_win_hide(widget->ui->win);
  widget->data.hidden = true;
}

void 
pv_widget_show(pv_widget_t* widget) {
  if(!widget || !widget->data.hidden) return;
  lf_win_show(widget->ui->win);
  widget->data.hidden = false;
}

#ifdef LF_X11
#include <X11/extensions/Xinerama.h>
int32_t 
pv_monitor_focused_idx(pv_state_t* s) {
  Window focus;
  int32_t focusstate;
  Window rootret;
  Display* dsp = lf_win_get_x11_display();
  Window root = DefaultRootWindow(dsp); 
  int32_t nmons;
  int32_t greatestarea = 0;
  uint32_t focusedmon = 0;

  XineramaScreenInfo* screeninfo = XineramaQueryScreens(dsp, &nmons);
  XGetInputFocus(dsp, &focus, &focusstate);
  if (focus != root && focus != PointerRoot && focus != None) {
    Window* childs;
    uint32_t nchilds;
    Window focusparent;
    do {
      focusparent = focus;
      if (XQueryTree(dsp, focus, &rootret, &focus, &childs, &nchilds) && childs)
        XFree(childs);
    } while (focus != root && focus != focusparent);
    XWindowAttributes attr;
    if (!XGetWindowAttributes(dsp, focusparent, &attr)) {
      fprintf(stderr, "podvig: cannot retrieve xinerama screens.\n");
      return -1; 
    }
    for (uint32_t i = 0; i < nmons; i++) {
      int32_t area  = INTERSECT(attr.x, attr.y, attr.width, attr.height, screeninfo[i]);
      if (area > greatestarea) {
        greatestarea = area;
        focusedmon = i;
      }
    }
  }

  uint32_t maskreturn;
  int32_t cursorx, cursory;
  if (!greatestarea && XQueryPointer(
    dsp, root, &rootret, &rootret,
    &cursorx, &cursory, &focusstate, &focusstate, &maskreturn)) {
    for (uint32_t i = 0; i < nmons; i++) {
      if (INTERSECT(cursorx, cursory, 1, 1, screeninfo[i]) != 0) {
        focusedmon = i;
        break;
      }
    }
  }
  return focusedmon;
}

lf_container_t 
pv_monitor_by_idx(pv_state_t* s, uint32_t idx) {
  Display* dsp = lf_win_get_x11_display();
  int32_t nmons;
  XineramaScreenInfo* screeninfo = XineramaQueryScreens(dsp, &nmons);
  return (lf_container_t){
    .pos = (vec2s){
      .x = screeninfo[idx].x_org,
      .y = screeninfo[idx].y_org,
    },
    .size = (vec2s){
      .x = screeninfo[idx].width,
      .y = screeninfo[idx].height
    }
  };
}
#endif

pv_widget_t*
pv_widget_by_name(pv_state_t* s, const char* name) {
  return &shget(s->widgets, name);
}

void 
pv_run(pv_state_t* s) {
  bool running = true;
  while(running) {
    for(uint32_t i = 0; i < (uint32_t)shlenu(s->widgets); i++) {
      pv_widget_t widget = s->widgets[i].value; 
      running = widget.ui->running; 
      lf_ui_core_next_event(widget.ui);
    }
  }
}
