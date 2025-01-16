#ifndef _PEEKABOO_H_
#define _PEEKABOO_H_

#include "hyprland-toplevel-export-v1.h"
#include "wayland-client-core.h"
#include "wlr-screencopy-unstable-v1.h"
#include "surface.h"

#define MAX_INPUT_LENGTH 512

struct output {
    struct wl_list         link; // type: struct output
    struct wl_output      *wl_output;
    struct zxdg_output_v1 *xdg_output;
    char                  *name;
    int32_t                scale;
    int32_t                width;
    int32_t                height;
    int32_t                x;
    int32_t                y;
};

struct seat {
    struct wl_list     link; // type: struct seat
    struct wl_seat     *wl_seat;
    struct wl_keyboard *wl_keyboard;
    struct xkb_context *xkb_context;
    struct xkb_keymap  *xkb_keymap;
    struct xkb_state   *xkb_state;
    struct peekaboo    *peekaboo;
};

struct toplevel_handle {
  struct wl_list                         link; // type: struct toplevel_handle
  struct zwlr_foreign_toplevel_handle_v1 *zwlr_foreign_toplevel_handle;
};

struct config {
  int32_t preview_window_width;
  int32_t preview_window_height;
  int32_t preview_window_margin_x;
  int32_t preview_window_margin_y;
  int32_t preview_window_padding_x;
  int32_t preview_window_padding_y;
  int32_t preview_window_border_stroke_size;
  uint32_t preview_window_background_color;
};

struct peekaboo {
  struct config                              config;
  struct wl_display                          *wl_display;
  struct wl_registry                         *wl_registry;
  struct wl_compositor                       *wl_compositor;
  struct zwlr_layer_shell_v1                 *wl_layer_shell;
  struct zwlr_virtual_pointer_manager_v1     *wl_virtual_pointer_mgr;
  struct wp_viewporter                       *wp_viewporter;
  struct wp_viewport                         *wp_viewport;
  struct wp_fractional_scale_manager_v1      *fractional_scale_mgr;
  struct wl_surface                          *wl_surface;
  struct wl_callback                         *wl_surface_callback;
  struct zwlr_layer_surface_v1               *wl_layer_surface;
  struct zxdg_output_manager_v1              *xdg_output_manager;
  struct hyprland_toplevel_export_manager_v1 *hyprland_toplevel_export_manager;
  struct wl_buffer                           *wl_buffer;
  struct wl_output                           *wl_output;

  struct wl_shm                              *wl_shm;
  struct wl_shm_pool                         *wl_shm_pool;
  void                                       *shm_buf;
  int                                        shm_fd;
  size_t                                     shm_size;

  struct wl_list                             outputs;
  struct wl_list                             seats;
  struct wl_list                             toplevel_handles;
  struct wl_list                             wm_clients;


  struct surface_buffer_pool                 surface_buffer_pool;
  uint32_t                                   surface_height;
  uint32_t                                   surface_width;

  void                                       (*request_frame)(struct peekaboo* peekaboo);

  char                                       input[MAX_INPUT_LENGTH];
  size_t                                     input_size;
  bool                                       running;
  struct wm_client                           *selected_client;
};

#endif /* _PEEKABOO_H_ */
