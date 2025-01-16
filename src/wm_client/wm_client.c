#include "wm_client.h"
#include "../log.h"
#include "hyprland.h"

void wm_clients_init(struct peekaboo *peekaboo, struct wl_list *wm_clients,
                     enum WM_CLIENT client_type) {
  switch (client_type) {
  case WM_CLIENT_HYPRLAND:
    hyprland_clients_init(peekaboo, wm_clients);
    break;
  default:
    log_error("Unknown client type\n");
    break;
  }
}

void wm_clients_destroy(struct wl_list *wm_clients,
                        enum WM_CLIENT client_type) {

  switch (client_type) {
  case WM_CLIENT_HYPRLAND:
    hyprland_clients_destroy(wm_clients);
    break;
  default:
    log_error("Unknown client type\n");
    break;
  }
}

void wm_client_focus(struct wm_client *wm_client) {
  switch (wm_client->wm_client_type) {
  case WM_CLIENT_HYPRLAND:
    hyprland_client_focus(wm_client);
    break;
  default:
    log_error("Unknown client type\n");
    break;
  }
}
