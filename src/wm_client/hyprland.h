#ifndef _WM_CLIENT__HYPRLAND_H_
#define _WM_CLIENT__HYPRLAND_H_

#include "wayland-util.h"
#include <cairo.h>
#include <stdint.h>
#include "../peekaboo.h"

struct hyprland_client {
  uint64_t address;
  struct hyprland_toplevel_export_frame_v1 *toplevel_export_frame;

};

void hyprland_clients_init(struct peekaboo *peekaboo,
                           struct wl_list *wm_clients);

void hyprland_clients_destroy(struct wl_list *wm_clients);

void hyprland_client_focus(struct wm_client *wm_client);

#endif /* _WM_CLIENT__HYPRLAND_H_ */
