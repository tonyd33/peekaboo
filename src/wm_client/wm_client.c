#include "wm_client.h"
#include "../log.h"
#include "hyprland.h"

/*
 * Currently, only hyprland is supported. To support another WM, the following
 * conditions must be met:
 * 1. There is a way to capture every toplevel surface into a buffer
 * 2. (Optional, but nice) There is a way to associate each toplevel surface
 *    to a title
 * 3. There is a way to associate each toplevel in such a way that we can
 *    later "open" or "focus" a toplevel.
 *
 * While there are WM-agnostic protocols for at least condition 1, I have
 * not been able to find any WM-agnostic protocols for condition 2 or 3.
 * The closest I've found is zwlr_foreign_toplevel_manager, but the problem
 * with that is:
 * - (Possibly solvable and not strictly necessary) We don't receive the title
 *   of toplevels unless they're changed.
 * - (Necessary) zwlr_foreign_toplevel_manager doesn't provide a way to focus
 *   the toplevel.
 *
 * So until I find ways to meet the conditions for other WMs, I can only
 * support hyprland, which is what I use anyway.
 */
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

void wm_clients_refresh(struct peekaboo *peekaboo, struct wl_list *wm_clients,
                        enum WM_CLIENT client_type) {
  switch (client_type) {
  case WM_CLIENT_HYPRLAND:
    hyprland_clients_refresh(peekaboo, wm_clients);
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
