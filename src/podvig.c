#include "../include/podvig/podvig.h"
#include <X11/X.h>
#include <leif/ez_api.h>
#ifdef LF_X11
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#endif
#include <leif/animation.h>
#include <leif/color.h>
#include <leif/leif.h>
#include <leif/ui_core.h>
#include <leif/util.h>
#include <leif/widget.h>
#include <leif/win.h>

#define STB_DS_IMPLEMENTATION
#include "../vendor/stb_ds.h"

#define INTERSECT(x,y,w,h,r)  (MAX(0, MIN((x)+(w),(r).x_org+(r).width)  - MAX((x),(r).x_org))) 

static void evcallback(void* ev, lf_ui_state_t* ui);

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

pv_widget_t* 
pv_widget(pv_state_t* s, const char* name, pv_widget_ui_layout_func_t layout_cb,
          float x, float y, float w, float h) {
  pv_widget_data_t data = (pv_widget_data_t){
    .x = x, 
    .y = y,
    .width = w,
    .height = h,
    .always_ontop = true, 
    .borderwidth = 0, 
    .bordercolor = 0x0,
    .close_cb = NULL
  };

  return pv_widget_ex(s, name, layout_cb, &data, 
                      PV_WIDGET_FLAG_ALWAYS_ONTOP |
                      PV_WIDGET_FLAG_POSX         | 
                      PV_WIDGET_FLAG_POSY         | 
                      PV_WIDGET_FLAG_WIDTH        | 
                      PV_WIDGET_FLAG_HEIGHT       |
                      PV_WIDGET_FLAG_BORDER_WIDTH | 
                      PV_WIDGET_FLAG_BORDER_COLOR  
                      );
}

pv_widget_t*
pv_widget_ex(pv_state_t* s, const char* name, pv_widget_ui_layout_func_t layout_cb, pv_widget_data_t* data, uint32_t flags) {
  pv_widget_t* widget = malloc(sizeof(*widget));
  memset(widget, 0, sizeof(*widget));
  uint32_t winx = 0, winy = 0, winw = 256, winh = 256, borderwidth = 0,
  bordercolor = 0x0; // 256 Is default size of widgets 
  bool transparent_framebuffer = true;
  
  bool hidden = false;
  uint32_t winflags = LF_WINDOWING_FLAG_X11_OVERRIDE_REDIRECT;
  if(data) {
    if(lf_flag_exists(&flags, PV_WIDGET_FLAG_POSX))   winx = data->x;
    if(lf_flag_exists(&flags, PV_WIDGET_FLAG_POSY))   winy = data->y;
    if(lf_flag_exists(&flags, PV_WIDGET_FLAG_WIDTH))  winw = data->width;
    if(lf_flag_exists(&flags, PV_WIDGET_FLAG_HEIGHT)) winh = data->height;
    if(lf_flag_exists(&flags, PV_WIDGET_FLAG_HEIGHT)) winh = data->height;
    if(lf_flag_exists(&flags, PV_WIDGET_FLAG_BORDER_WIDTH)) borderwidth = data->borderwidth;
    if(lf_flag_exists(&flags, PV_WIDGET_FLAG_BORDER_COLOR)) bordercolor = data->bordercolor;
    if(lf_flag_exists(&flags, PV_WIDGET_FLAG_TRANSPARENT_FRAMEBUFFER)) transparent_framebuffer = data->transparent_framebuffer;
    if(lf_flag_exists(&flags, PV_WIDGET_FLAG_HIDDEN)) hidden = true; 
    if(lf_flag_exists(&flags, PV_WIDGET_FLAG_MANAGE_INTERACTIVE)) {
      if(data->manage_interactive) {
        lf_flag_unset(&winflags, LF_WINDOWING_FLAG_X11_OVERRIDE_REDIRECT);
      }
    }
  }
  if(flags & PV_WIDGET_FLAG_ALWAYS_ONTOP && data->always_ontop)
    lf_ui_core_set_window_hint(LF_WINDOWING_HINT_ABOVE, true);
  lf_ui_core_set_window_hint(LF_WINDOWING_HINT_POS_X, winx);
  lf_ui_core_set_window_hint(LF_WINDOWING_HINT_POS_Y, winy);
  lf_ui_core_set_window_hint(LF_WINDOWING_HINT_BORDER_WIDTH, borderwidth);
  lf_ui_core_set_window_hint(LF_WINDOWING_HINT_BORDER_COLOR, bordercolor);
  lf_ui_core_set_window_hint(LF_WINDOWING_HINT_TRANSPARENT_FRAMEBUFFER, transparent_framebuffer);
  lf_ui_core_set_window_flags(winflags);
  lf_window_t win = lf_ui_core_create_window(
    winw, winh, 
    flags && lf_flag_exists(&flags, PV_WIDGET_FLAG_TITLE) ? data->title : "pv_widget");

  widget->ui = lf_ui_core_init(win);
  widget->ui->user_data = widget;
  widget->ui->needs_render = true;
  widget->data = *data;
  widget->data.anim = PV_WIDGET_ANIMATION_NONE;

  widget->root_div = lf_div(widget->ui);
  widget->root_div->base.scrollable = false;
  widget->root_div->base.props.color = LF_NO_COLOR;
  widget->data.root_div_color = widget->root_div->base.props.color;
  widget->name = strdup(name);
  
  lf_win_set_event_cb(win, evcallback);

  if(layout_cb) {
    lf_component(widget->ui, layout_cb);
  }
  lf_div_end(widget->ui);
  if(hidden) {
    lf_win_hide(win);
  }
    
  shput(s->widgets, name, *widget);

  return widget;
}

void hidewindow(lf_ui_state_t* ui, lf_timer_t* timer) {
  (void)ui;
  (void)timer;
  lf_win_hide(ui->win);
}

void finishcb(lf_animation_t* anim, void* data) {
  hidewindow((lf_ui_state_t*)anim->user_data, NULL);
}

void
pv_widget_hide(pv_widget_t* widget) {
  if(!widget || widget->data.hidden) return;
  widget->data.hidden = true;
  if(widget->data.anim != PV_WIDGET_ANIMATION_NONE) {
    lf_animation_t* anim = 
      lf_widget_set_fixed_height(widget->ui, &widget->root_div->base, 0.0f);
    anim->finish_cb = finishcb;
    anim->user_data = widget->ui;
    widget->data.root_div_color = widget->root_div->base.props.color;
    lf_widget_set_prop_color(widget->ui, &widget->root_div->base, &widget->root_div->base.props.color, LF_NO_COLOR);
  } else {
    lf_win_hide(widget->ui->win);
  }
}

typedef struct {
  lf_color_t color, text_color, border_color;
} widget_colors_t;
void 
showallwidgets(pv_widget_t* pvwidget, lf_ui_state_t* ui, lf_widget_t* widget) {
  if(pvwidget->data.anim == PV_WIDGET_ANIMATION_NONE) return;
  widget->props.color = LF_NO_COLOR;
  widget->props.text_color = LF_NO_COLOR;
  widget->props.border_color = LF_NO_COLOR;
  widget_colors_t colors = *(widget_colors_t*)widget->user_data;
  lf_style_widget_prop_color(ui, widget, color, colors.color); 
  lf_style_widget_prop_color(ui, widget, text_color, colors.text_color);
  lf_style_widget_prop_color(ui, widget, border_color, colors.border_color); 
  for(uint32_t i = 0; i < widget->num_childs; i++) {
    showallwidgets(pvwidget, ui, widget->childs[i]);
  }
}


void 
pv_widget_show(pv_widget_t* widget) {
  if(!widget || !widget->data.hidden) return;
  if(widget->data.open_cb) {
    widget->data.open_cb(widget);
  }
  lf_win_show(widget->ui->win);
  widget->data.hidden = false;
  lf_widget_set_fixed_height(widget->ui, &widget->root_div->base, lf_win_get_size(widget->ui->win).y);
  lf_widget_set_prop_color(widget->ui, &widget->root_div->base, &widget->root_div->base.props.color, widget->data.root_div_color); 
  showallwidgets(widget, widget->ui, widget->ui->root);

  int grab_event_mask = ButtonPressMask | ButtonReleaseMask |
    PointerMotionMask | EnterWindowMask | LeaveWindowMask;
  if (XGrabPointer(lf_win_get_x11_display(), 
                   widget->ui->win, False, grab_event_mask,
                   GrabModeAsync, GrabModeAsync,
                   None, None, CurrentTime) != GrabSuccess) {
    fprintf(stderr, "podvig: failed to grab pointer for popup.\n");
    return;
  }


  XFlush(lf_win_get_x11_display());
}


#ifdef LF_X11
void 
pv_widget_set_popup_of(pv_state_t* s, pv_widget_t* popup, lf_window_t parent) {
  Display* dsp = lf_win_get_x11_display();
  XSetTransientForHint(dsp, popup->ui->win, parent);
  Atom window_type_atom = XInternAtom(dsp, "_NET_WM_WINDOW_TYPE", False);
  Atom popup_atom = XInternAtom(dsp, "_NET_WM_WINDOW_TYPE_POPUP_MENU", False);

  XChangeProperty(dsp, popup->ui->win, window_type_atom,
                  XA_ATOM, 32, PropModeReplace,
                  (unsigned char *)&popup_atom, 1);
  pv_widget_t* ref = &shget(s->widgets, popup->name); 
  popup->data.is_popup = true;
  ref->data.is_popup = true;
}
#endif

void 
hideallwidgets(lf_widget_t* widget) {
  widget->props.color = LF_NO_COLOR;
  widget->props.text_color = LF_NO_COLOR;
  widget->props.border_color = LF_NO_COLOR;
  for(uint32_t i = 0; i < widget->num_childs; i++) {
    hideallwidgets(widget->childs[i]);
  }
}
void 
storewidgetcolors(lf_widget_t* widget) {
  if(!widget->user_data) {
    widget_colors_t* colors =  malloc(sizeof(widget_colors_t));
    colors->color = widget->props.color;
    colors->text_color = widget->props.text_color;
    colors->border_color = widget->props.border_color;
    widget->user_data = colors;
  }
  for(uint32_t i = 0; i < widget->num_childs; i++) {
    storewidgetcolors(widget->childs[i]);
  }
}

void 
pv_widget_set_animation(pv_widget_t* widget,  pv_widget_animation_t anim, float anim_time,
    lf_animation_func_t anim_func) {
  if(!widget) return;
  widget->data.anim = anim;
  widget->data.anim_time = anim_time;
  widget->data.anim_func = anim_func;

  lf_widget_set_transition_props(&widget->root_div->base, anim_time, anim_func);
  lf_style_widget_prop_color(widget->ui, &widget->root_div->base, color,  widget->ui->root->props.color);
  storewidgetcolors(widget->ui->root);
  hideallwidgets(widget->ui->root);
  if(anim == PV_WIDGET_ANIMATION_SLIDE_OUT_VERT) {
    lf_widget_set_fixed_height(widget->ui, &widget->root_div->base, 0.0f);
    lf_widget_set_padding(widget->ui, &widget->root_div->base, 0.0f);
  }
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
  while(pv_update(s));
}

void evcallback(void* ev, lf_ui_state_t* ui) {
  XEvent* xev = (XEvent*)ev;
  switch(xev->type) {
    case ButtonPress: 
      {
        pv_widget_t* widget = (pv_widget_t*)ui->user_data;
        if(!widget) break;
        if(widget->data.hidden) break;
        if(widget->data.is_popup) {
          Window root_return, child_return;
          int root_x, root_y;
          int win_x, win_y;
          unsigned int mask;

          XQueryPointer(
            lf_win_get_x11_display(), DefaultRootWindow(lf_win_get_x11_display()), 
            &root_return, &child_return,
            &root_x, &root_y, &win_x, &win_y, &mask);

            if (root_x < widget->data.x || root_x > widget->data.x + widget->data.width ||
              root_y < widget->data.y || root_y > widget->data.y + widget->data.height) {
            pv_widget_hide(widget);
            if(widget->data.close_cb) {
              widget->data.close_cb(widget);
            }
              XUngrabPointer(lf_win_get_x11_display(), CurrentTime);
            }
        }
        break;
      }
  }
}

bool 
pv_update(pv_state_t* s) {
  bool running = true;
    for(uint32_t i = 0; i < (uint32_t)shlenu(s->widgets); i++) {
      pv_widget_t widget = s->widgets[i].value; 
      running = widget.ui->running; 
      lf_ui_core_next_event(widget.ui);
    }
  return running;
}
