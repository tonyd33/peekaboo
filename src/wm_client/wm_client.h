#ifndef _WM_CLIENT__WM_CLIENT_H_
#define _WM_CLIENT__WM_CLIENT_H_

#include "../surface_cache.h"
#include <cairo.h>
#include <stdint.h>
#include <wayland-util.h>

#define WM_CLIENT_MAX_TITLE_LENGTH 512
#define WM_CLIENT_MAX_SHORTCUT_KEYS_LENGTH 512

enum WM_CLIENT { WM_CLIENT_HYPRLAND };

struct wm_client {
  struct wl_list link;

  struct peekaboo *peekaboo;

  enum WM_CLIENT wm_client_type;
  /* wm-specific information on the client. */
  void *client;

  char title[WM_CLIENT_MAX_TITLE_LENGTH];

  struct wl_buffer *wl_buffer;
  void *buf;
  cairo_surface_t *orig_surface;
  struct surface_cache *surface_cache;

  uint32_t width;
  uint32_t height;
  uint32_t stride;
  uint32_t format;
  bool buffer_params_needs_update;

  bool ready;
  char shortcut_keys[WM_CLIENT_MAX_SHORTCUT_KEYS_LENGTH];
  uint32_t shortcut_keys_highlight_len;
  bool hide;
  bool dim;
};

void wm_clients_init(struct peekaboo *peekaboo, struct wl_list *wm_clients,
                     enum WM_CLIENT wm_client_type);

void wm_clients_destroy(struct wl_list *wm_clients,
                        enum WM_CLIENT wm_client_type);

void wm_client_focus(struct wm_client *wm_client);

#endif /* _WM_CLIENT__WM_CLIENT_H_ */
