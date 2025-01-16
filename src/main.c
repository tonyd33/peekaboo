#include "fractional-scale-v1.h"
#include "hyprland-toplevel-export-v1.h"
#include "log.h"
#include "peekaboo.h"
#include "preview.h"
#include "src/surface.h"
#include "string.h"
#include "viewporter.h"
#include "wlr-foreign-toplevel-management-unstable-v1.h"
#include "wlr-layer-shell-unstable-v1.h"
#include "wlr-screencopy-unstable-v1.h"
#include "wm_client/wm_client.h"
#include "xdg-output-unstable-v1.h"
#include <stddef.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client-core.h>
#include <wayland-client-protocol.h>
#include <wayland-client.h>
#include <wayland-util.h>
#include <xkbcommon/xkbcommon-keysyms.h>
#include <xkbcommon/xkbcommon.h>

#define EXPECT_NON_NULL(value, name)                                           \
  {                                                                            \
    if ((value) == NULL) {                                                     \
      log_error("Failed to get " #name ".\n");                                 \
      return 1;                                                                \
    }                                                                          \
  }

#undef MAX
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

#undef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))

static void send_frame(struct peekaboo *peekaboo) {
  // TODO: Implement fractional scaling
  int32_t scale_120 = 120;

  struct surface_buffer *surface_buffer =
      get_next_buffer(peekaboo->wl_shm, &peekaboo->surface_buffer_pool,
                      peekaboo->surface_width * scale_120 / 120,
                      peekaboo->surface_height * scale_120 / 120);
  if (surface_buffer == NULL) {
    return;
  }
  surface_buffer->state = SURFACE_BUFFER_BUSY;

  render(peekaboo, surface_buffer);

  wl_surface_set_buffer_scale(peekaboo->wl_surface, 1);

  wl_surface_attach(peekaboo->wl_surface, surface_buffer->wl_buffer, 0, 0);
  /* wp_viewport_set_destination(peekaboo->wp_viewport, peekaboo->surface_width,
   */
  /* peekaboo->surface_height); */
  wl_surface_damage(peekaboo->wl_surface, 0, 0, peekaboo->surface_width,
                    peekaboo->surface_height);
  wl_surface_commit(peekaboo->wl_surface);
}

static void noop(void) {}

static void surface_callback_done(void *data, struct wl_callback *callback,
                                  uint32_t callback_data) {
  log_debug("Surface callback done\n");
  struct peekaboo *peekaboo = data;
  send_frame(peekaboo);

  wl_callback_destroy(peekaboo->wl_surface_callback);
  peekaboo->wl_surface_callback = NULL;
}

const struct wl_callback_listener surface_callback_listener = {
    .done = surface_callback_done,
};

static void request_frame(struct peekaboo *peekaboo) {
  if (peekaboo->wl_surface_callback != NULL) {
    return;
  }

  peekaboo->wl_surface_callback = wl_surface_frame(peekaboo->wl_surface);
  wl_callback_add_listener(peekaboo->wl_surface_callback,
                           &surface_callback_listener, peekaboo);
  wl_surface_commit(peekaboo->wl_surface);
}

static void handle_keyboard_keymap(void *data, struct wl_keyboard *keyboard,
                                   uint32_t format, int fd, uint32_t size) {
  struct seat *seat = data;
  if (seat->xkb_state != NULL) {
    xkb_state_unref(seat->xkb_state);
    seat->xkb_state = NULL;
  }
  if (seat->xkb_keymap != NULL) {
    xkb_keymap_unref(seat->xkb_keymap);
    seat->xkb_keymap = NULL;
  }

  switch (format) {
  case WL_KEYBOARD_KEYMAP_FORMAT_NO_KEYMAP:
    seat->xkb_keymap = xkb_keymap_new_from_names(seat->xkb_context, NULL,
                                                 XKB_KEYMAP_COMPILE_NO_FLAGS);
    break;

  case WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1:;
    void *buffer = mmap(NULL, size - 1, PROT_READ, MAP_PRIVATE, fd, 0);
    if (buffer == MAP_FAILED) {
      log_error("Could not mmap keymap data.");
      return;
    }

    seat->xkb_keymap = xkb_keymap_new_from_buffer(
        seat->xkb_context, buffer, size - 1, XKB_KEYMAP_FORMAT_TEXT_V1,
        XKB_KEYMAP_COMPILE_NO_FLAGS);

    munmap(buffer, size - 1);
    close(fd);
    break;
  }

  seat->xkb_state = xkb_state_new(seat->xkb_keymap);
}

static void handle_keyboard_modifiers(void *data, struct wl_keyboard *keyboard,
                                      uint32_t serial, uint32_t mods_depressed,
                                      uint32_t mods_latched,
                                      uint32_t mods_locked, uint32_t group) {
  struct seat *seat = data;
  xkb_state_update_mask(seat->xkb_state, mods_depressed, mods_latched,
                        mods_locked, 0, 0, group);
}

static void handle_keyboard_key(void *data, struct wl_keyboard *wl_keyboard,
                                uint32_t serial, uint32_t time, uint32_t key,
                                uint32_t state) {
  struct seat *seat = data;

  const xkb_keycode_t key_code = key + 8;
  if (!xkb_keycode_is_legal_x11(key_code)) {
    return;
  }

  const xkb_keysym_t key_sym =
      xkb_state_key_get_one_sym(seat->xkb_state, key_code);
  char ch = xkb_state_key_get_utf32(seat->xkb_state, key_code);

  bool needs_redraw = handle_key(seat->peekaboo, key_sym, ch);
  if (needs_redraw) {
    request_frame(seat->peekaboo);
  }
}

static const struct wl_keyboard_listener wl_keyboard_listener = {
    .keymap = handle_keyboard_keymap,
    .enter = (void *)noop,
    .leave = (void *)noop,
    .key = handle_keyboard_key,
    .modifiers = handle_keyboard_modifiers,
    .repeat_info = (void *)noop,
};

static void handle_seat_capabilities(void *data, struct wl_seat *wl_seat,
                                     uint32_t capabilities) {
  struct seat *seat = data;
  if (capabilities & WL_SEAT_CAPABILITY_KEYBOARD) {
    seat->wl_keyboard = wl_seat_get_keyboard(seat->wl_seat);
    wl_keyboard_add_listener(seat->wl_keyboard, &wl_keyboard_listener, seat);
  }
}

static void handle_output_scale(void *data, struct wl_output *wl_output,
                                int32_t scale) {
  struct output *output = data;
  output->scale = scale;
}

const static struct wl_output_listener output_listener = {
    .name = (void (*)(void *, struct wl_output *, const char *))noop,
    .geometry =
        (void (*)(void *, struct wl_output *, int32_t, int32_t, int32_t,
                  int32_t, int32_t, const char *, const char *, int32_t))noop,
    .mode = (void (*)(void *, struct wl_output *, uint32_t, int32_t, int32_t,
                      int32_t))noop,
    .scale = handle_output_scale,
    .description = (void (*)(void *, struct wl_output *, const char *))noop,
    .done = (void (*)(void *, struct wl_output *))noop,
};

const struct wl_seat_listener wl_seat_listener = {
    .name = (void *)noop,
    .capabilities = handle_seat_capabilities,
};

static void handle_layer_surface_configure(
    void *data, struct zwlr_layer_surface_v1 *layer_surface, uint32_t serial,
    uint32_t width, uint32_t height) {
  struct peekaboo *peekaboo = data;
  peekaboo->surface_width = width;
  peekaboo->surface_height = height;
  zwlr_layer_surface_v1_ack_configure(layer_surface, serial);

  log_debug("Configuring layer_surface\n");

  if (peekaboo->running) {
    send_frame(peekaboo);
  }
}

static void
handle_layer_surface_closed(void *data,
                            struct zwlr_layer_surface_v1 *layer_surface) {
  struct peekaboo *peekaboo = data;
  peekaboo->running = false;
}

const struct zwlr_layer_surface_v1_listener wl_layer_surface_listener = {
    .configure = handle_layer_surface_configure,
    .closed = handle_layer_surface_closed,
};

/* Upon receiving a toplevel, append it to our list. */
void handle_zwlr_foreign_toplevel_manager_toplevel(
    void *data,
    struct zwlr_foreign_toplevel_manager_v1 *zwlr_foreign_toplevel_manager_v1,
    struct zwlr_foreign_toplevel_handle_v1 *toplevel) {
  struct peekaboo *peekaboo = data;
  struct toplevel_handle *toplevel_handle =
      calloc(1, sizeof(struct toplevel_handle));
  toplevel_handle->zwlr_foreign_toplevel_handle = toplevel;

  wl_list_insert(&peekaboo->toplevel_handles, &toplevel_handle->link);
}

const struct zwlr_foreign_toplevel_manager_v1_listener
    zwlr_foreign_toplevel_manager_listener = {
        .toplevel = handle_zwlr_foreign_toplevel_manager_toplevel,
        .finished = (void *)noop,
};

static void handle_xdg_output_logical_position(
    void *data, struct zxdg_output_v1 *xdg_output, int32_t x, int32_t y) {
  struct output *output = data;
  output->x = x;
  output->y = y;
}

static void handle_xdg_output_logical_size(void *data,
                                           struct zxdg_output_v1 *xdg_output,
                                           int32_t w, int32_t h) {
  struct output *output = data;
  output->width = w;
  output->height = h;
}

static void handle_xdg_output_name(void *data,
                                   struct zxdg_output_v1 *xdg_output,
                                   const char *name) {
  struct output *output = data;
  output->name = strdup(name);
}

const static struct zxdg_output_v1_listener xdg_output_listener = {
    .logical_position = handle_xdg_output_logical_position,
    .logical_size = handle_xdg_output_logical_size,
    .done = (void *)noop,
    .name = handle_xdg_output_name,
    .description = (void *)noop,
};

static void registry_global(void *data, struct wl_registry *registry,
                            uint32_t name, const char *interface,
                            uint32_t version) {
  struct peekaboo *peekaboo = data;
  /* log_debug("Registry %u: %s v%u.\n", name, interface, version); */
  /* wl_compositor */
  if (!strcmp(interface, wl_compositor_interface.name)) {
    peekaboo->wl_compositor =
        wl_registry_bind(registry, name, &wl_compositor_interface, 4);
    log_debug("Bound to compositor %u.\n", name);
  }
  /* wl_shm */
  else if (!strcmp(interface, wl_shm_interface.name)) {
    peekaboo->wl_shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
  }
  /* zwlr_layer_shell */
  else if (!strcmp(interface, zwlr_layer_shell_v1_interface.name)) {
    peekaboo->wl_layer_shell =
        wl_registry_bind(registry, name, &zwlr_layer_shell_v1_interface, 2);
    log_debug("Bound to wl_layer_shell %u.\n", name);
  }
  /* wl_seat */
  else if (!strcmp(interface, wl_seat_interface.name)) {
    struct seat *seat = calloc(1, sizeof(struct seat));
    seat->wl_seat = wl_registry_bind(registry, name, &wl_seat_interface, 7);
    seat->wl_keyboard = NULL;
    seat->xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    seat->xkb_state = NULL;
    seat->xkb_keymap = NULL;
    seat->peekaboo = peekaboo;

    wl_seat_add_listener(seat->wl_seat, &wl_seat_listener, seat);
    wl_list_insert(&peekaboo->seats, &seat->link);

    log_debug("Bound to wl_seat %u.\n", name);
  }
  /* wl_output */
  else if (strcmp(interface, wl_output_interface.name) == 0) {
    struct wl_output *wl_output =
        wl_registry_bind(registry, name, &wl_output_interface, 3);
    struct output *output = calloc(1, sizeof(struct output));
    output->wl_output = wl_output;
    output->scale = 1;

    /* TODO: Select output correctly */
    peekaboo->wl_output = wl_output;

    wl_output_add_listener(output->wl_output, &output_listener, output);
    wl_list_insert(&peekaboo->outputs, &output->link);
    log_debug("Bound to wl_output %u.\n", name);
  }
  /* zxdg_output_manager */
  else if (!strcmp(interface, zxdg_output_manager_v1_interface.name)) {
    peekaboo->xdg_output_manager =
        wl_registry_bind(registry, name, &zxdg_output_manager_v1_interface, 2);
    log_debug("Bound to xdg_output_manager %u.\n", name);
  }
  /* wp_viewporter */
  else if (!strcmp(interface, wp_viewporter_interface.name)) {
    peekaboo->wp_viewporter =
        wl_registry_bind(registry, name, &wp_viewporter_interface, 1);
    log_debug("Bound to wp_viewporter %u.\n", name);
  }
  /* hyprland_toplevel_export_manager */
  else if (!strcmp(interface,
                   hyprland_toplevel_export_manager_v1_interface.name)) {
    peekaboo->hyprland_toplevel_export_manager = wl_registry_bind(
        registry, name, &hyprland_toplevel_export_manager_v1_interface, 2);
    log_debug("Bound to hyprland_toplevel_export_manager %u.\n", name);
  }
}

static void registry_global_remove(void *data, struct wl_registry *wl_registry,
                                   uint32_t name) {}

static const struct wl_registry_listener wl_registry_listener = {
    .global = registry_global,
    .global_remove = registry_global_remove,
};

int main(int argc, char **argv) {
  struct peekaboo peekaboo = {
      .config =
          {
              .preview_window_width = 800,
              .preview_window_height = 600,
              .preview_window_padding_x = 5,
              .preview_window_padding_y = 5,
              .preview_window_margin_x = 10,
              .preview_window_margin_y = 10,
              .preview_window_border_stroke_size = 4,
              .preview_window_background_color = 0x84a6c9,
          },
      .wl_output = NULL,
      .wl_display = NULL,
      .wl_registry = NULL,
      .wl_compositor = NULL,
      .wl_shm = NULL,
      .wl_layer_shell = NULL,
      .wl_surface = NULL,
      .request_frame = request_frame,
      .running = true,
      .selected_client = NULL,
  };

  wl_list_init(&peekaboo.outputs);
  wl_list_init(&peekaboo.seats);
  wl_list_init(&peekaboo.toplevel_handles);
  wl_list_init(&peekaboo.wm_clients);

  /* Prepare for first roundtrip. */

  /* 1. Connect to registry and add listeners. */
  peekaboo.wl_display = wl_display_connect(NULL);
  EXPECT_NON_NULL(peekaboo.wl_display, "Wayland compositor");

  peekaboo.wl_registry = wl_display_get_registry(peekaboo.wl_display);
  EXPECT_NON_NULL(peekaboo.wl_registry, "Wayland registry");

  wl_registry_add_listener(peekaboo.wl_registry, &wl_registry_listener,
                           &peekaboo);

  /* First roundtrip. */
  log_debug("Starting first roundtrip\n");
  log_indent();
  wl_display_roundtrip(peekaboo.wl_display);
  log_unindent();
  log_debug("Finished first roundtrip\n");

  EXPECT_NON_NULL(peekaboo.wl_compositor, "wl_compositor");
  EXPECT_NON_NULL(peekaboo.wl_shm, "wl_shm");
  EXPECT_NON_NULL(peekaboo.wl_layer_shell, "zwlr_layer_shell_v1");
  EXPECT_NON_NULL(peekaboo.xdg_output_manager, "xdg_output_manager");
  EXPECT_NON_NULL(peekaboo.wp_viewporter, "wp_viewporter");
  EXPECT_NON_NULL(peekaboo.wl_output, "wl_output");
  EXPECT_NON_NULL(peekaboo.hyprland_toplevel_export_manager,
                  "hyprland_toplevel_export_manager");

  /* Prepare second roundtrip. */

  /* 2. For every output, add listeners. */
  struct output *output;
  wl_list_for_each(output, &peekaboo.outputs, link) {
    output->xdg_output = zxdg_output_manager_v1_get_xdg_output(
        peekaboo.xdg_output_manager, output->wl_output);
    zxdg_output_v1_add_listener(output->xdg_output, &xdg_output_listener,
                                output);
  }

  /* Second roundtrip */
  log_debug("Starting second roundtrip\n");
  log_indent();
  wl_display_roundtrip(peekaboo.wl_display);
  log_unindent();
  log_debug("Finished second roundtrip\n");

  /* Initialize a list of clients connected to the WM.
   *
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
  wm_clients_init(&peekaboo, &peekaboo.wm_clients, WM_CLIENT_HYPRLAND);

  /* Third roundtrip. */
  log_debug("Starting third roundtrip\n");
  log_indent();
  wl_display_roundtrip(peekaboo.wl_display);
  log_unindent();
  log_debug("Finished third roundtrip\n");

  /* In this roundtrip, what should've happened for hyprland's export_frames
   * was:
   * 1. For each export_frame, we receive "buffer" event(s) informing us of the
   *    buffer parameters like width, height, etc. that the export frames can
   *    support. We currently only take the last event.
   * 2. We receive a "buffer_done" event when there are no more "buffer" events.
   *    Then, we request a copy on the export_frame.
   * 3. We receive a "ready" event when the copy is finished and render a frame,
   *    allowing unready export_frames to only have a background color.
   */

  surface_buffer_pool_init(&peekaboo.surface_buffer_pool);
  peekaboo.wl_surface = wl_compositor_create_surface(peekaboo.wl_compositor);
  /* wl_surface_add_listener(peekaboo.wl_surface, &surface_listener, &peekaboo);
   */
  peekaboo.wl_layer_surface = zwlr_layer_shell_v1_get_layer_surface(
      peekaboo.wl_layer_shell, peekaboo.wl_surface, peekaboo.wl_output,
      ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY, "peekaboo");
  zwlr_layer_surface_v1_add_listener(peekaboo.wl_layer_surface,
                                     &wl_layer_surface_listener, &peekaboo);
  zwlr_layer_surface_v1_set_exclusive_zone(peekaboo.wl_layer_surface, -1);
  zwlr_layer_surface_v1_set_anchor(peekaboo.wl_layer_surface,
                                   ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT |
                                       ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT |
                                       ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
                                       ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM);
  zwlr_layer_surface_v1_set_keyboard_interactivity(peekaboo.wl_layer_surface,
                                                   true);

  wl_surface_commit(peekaboo.wl_surface);

  while (peekaboo.running && wl_display_dispatch(peekaboo.wl_display) != -1) {
  }

  if (peekaboo.selected_client != NULL) {
    wm_client_focus(peekaboo.selected_client);
  }

  /* Cleanup */
  wm_clients_destroy(&peekaboo.wm_clients, WM_CLIENT_HYPRLAND);
  surface_buffer_pool_destroy(&peekaboo.surface_buffer_pool);

  wl_display_disconnect(peekaboo.wl_display);

  return 0;
}
