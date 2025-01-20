#include "config.h"
#include "log.h"
#include "peekaboo.h"
#include "preview.h"
#include "styles.h"
#include "string.h"
#include "surface.h"
#include "util.h"
#include "wm_client/wm_client.h"
#include <fractional-scale-v1.h>
#include <getopt.h>
#include <hyprland-toplevel-export-v1.h>
#include <stddef.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include <viewporter.h>
#include <wayland-client-core.h>
#include <wayland-client-protocol.h>
#include <wayland-client.h>
#include <wayland-util.h>
#include <wlr-foreign-toplevel-management-unstable-v1.h>
#include <wlr-layer-shell-unstable-v1.h>
#include <xdg-output-unstable-v1.h>
#include <xkbcommon/xkbcommon-keysyms.h>
#include <xkbcommon/xkbcommon.h>

#define EXPECT_NON_NULL(value, name)                                           \
  {                                                                            \
    if ((value) == NULL) {                                                     \
      log_error("Failed to get " #name ".\n");                                 \
      exit(EXIT_FAILURE);                                                      \
    }                                                                          \
  }

#ifdef DEBUG
uint32_t launch_time_ms = 0;
#endif

static void noop(void) {}
const struct wl_callback_listener surface_callback_listener;

static void send_frame(struct peekaboo *peekaboo) {
  int32_t scale_120 = peekaboo->fractional_scale;
  if (scale_120 == 0) {
    // Falling back to the output scale if fractional scale is not received.
    scale_120 =
        (peekaboo->current_output == NULL ? 1
                                          : peekaboo->current_output->scale) *
        120;
  }

  struct surface_buffer *surface_buffer = get_next_buffer(
      &peekaboo->config, peekaboo->wl_shm, &peekaboo->surface_buffer_pool,
      peekaboo->surface_width * scale_120 / 120,
      peekaboo->surface_height * scale_120 / 120);
  if (surface_buffer == NULL) {
    return;
  }

  render(peekaboo, surface_buffer);

  wl_surface_set_buffer_scale(peekaboo->wl_surface, 1);
  wp_viewport_set_destination(peekaboo->wp_viewport, peekaboo->surface_width,
                              peekaboo->surface_height);

  wl_surface_attach(peekaboo->wl_surface, surface_buffer->wl_buffer, 0, 0);
  wl_surface_damage_buffer(peekaboo->wl_surface, 0, 0, peekaboo->surface_width,
                           peekaboo->surface_height);
  wl_surface_commit(peekaboo->wl_surface);

#ifdef DEBUG
  log_debug("Frame sent after %ums\n", gettime_ms() - launch_time_ms);
#endif
}

static void request_frame(struct peekaboo *peekaboo) {
  if (peekaboo->wl_surface_callback != NULL) {
    return;
  }

  peekaboo->wl_surface_callback = wl_surface_frame(peekaboo->wl_surface);
  wl_callback_add_listener(peekaboo->wl_surface_callback,
                           &surface_callback_listener, peekaboo);
  wl_surface_commit(peekaboo->wl_surface);
}

// wl_callback {{{
static void surface_callback_done(void *data, struct wl_callback *callback,
                                  uint32_t callback_data) {
  struct peekaboo *peekaboo = data;
  send_frame(peekaboo);

  wl_callback_destroy(peekaboo->wl_surface_callback);
  peekaboo->wl_surface_callback = NULL;
}

const struct wl_callback_listener surface_callback_listener = {
    .done = surface_callback_done,
};
// }}}

// wl_keyboard {{{
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
                                uint32_t key_state) {
  struct seat *seat = data;

  const xkb_keycode_t key_code = key + 8;
  if (!xkb_keycode_is_legal_x11(key_code)) {
    return;
  }

  const xkb_keysym_t key_sym =
      xkb_state_key_get_one_sym(seat->xkb_state, key_code);
  char ch = xkb_state_key_get_utf32(seat->xkb_state, key_code);

  // Only handle keydown. Don't handle key repeat
  if (key_state != WL_KEYBOARD_KEY_STATE_PRESSED) {
    return;
  }

  bool redraw = handle_key(seat->peekaboo, key_sym, ch);
  if (redraw) {
    request_frame(seat->peekaboo);
  }
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
static const struct wl_keyboard_listener wl_keyboard_listener = {
    .keymap = handle_keyboard_keymap,
    .enter = (void *)noop,
    .leave = (void *)noop,
    .key = handle_keyboard_key,
    .modifiers = handle_keyboard_modifiers,
    .repeat_info = (void *)noop,
};
#pragma GCC diagnostic pop
// }}}

// wl_output {{{
static void handle_output_scale(void *data, struct wl_output *wl_output,
                                int32_t scale) {
  struct output *output = data;
  output->scale = scale;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
static const struct wl_output_listener output_listener = {
    .name = (void *)noop,
    .geometry = (void *)noop,
    .mode = (void *)noop,
    .scale = handle_output_scale,
    .description = (void *)noop,
    .done = (void *)noop,
};
#pragma GCC diagnostic pop
// }}}

// wl_seat {{{
static void handle_seat_capabilities(void *data, struct wl_seat *wl_seat,
                                     uint32_t capabilities) {
  struct seat *seat = data;
  if (capabilities & WL_SEAT_CAPABILITY_KEYBOARD) {
    seat->wl_keyboard = wl_seat_get_keyboard(seat->wl_seat);
    wl_keyboard_add_listener(seat->wl_keyboard, &wl_keyboard_listener, seat);
  }
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
const struct wl_seat_listener wl_seat_listener = {
    .name = (void *)noop,
    .capabilities = handle_seat_capabilities,
};
#pragma GCC diagnostic pop
// }}}

// zwlr_layer_surface {{{
static void handle_layer_surface_configure(
    void *data, struct zwlr_layer_surface_v1 *layer_surface, uint32_t serial,
    uint32_t width, uint32_t height) {
#ifdef DEBUG
  log_debug("Surface configured after %ums\n", gettime_ms() - launch_time_ms);
#endif
  struct peekaboo *peekaboo = data;
  peekaboo->surface_width = width;
  peekaboo->surface_height = height;
  zwlr_layer_surface_v1_ack_configure(layer_surface, serial);

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

static const struct zwlr_layer_surface_v1_listener wl_layer_surface_listener = {
    .configure = handle_layer_surface_configure,
    .closed = handle_layer_surface_closed,
};
// }}}

// wp_fractional_scale {{{
static void
fractional_scale_preferred(void *data,
                           struct wp_fractional_scale_v1 *fractional_scale,
                           uint32_t scale) {
  struct peekaboo *peekaboo = data;
  uint32_t old_scale = peekaboo->fractional_scale;
  peekaboo->fractional_scale = scale;

  if (old_scale != 0 && old_scale != scale) {
    request_frame(peekaboo);
  }
}

static const struct wp_fractional_scale_v1_listener fractional_scale_listener =
    {
        .preferred_scale = fractional_scale_preferred,
};
// }}}

// xdg_output {{{
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

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
static const struct zxdg_output_v1_listener xdg_output_listener = {
    .logical_position = handle_xdg_output_logical_position,
    .logical_size = handle_xdg_output_logical_size,
    .done = (void *)noop,
    .name = handle_xdg_output_name,
    .description = (void *)noop,
};
#pragma GCC diagnostic pop
// }}}

// wl_output {{{
static struct output *find_output_from_wl_output(struct wl_list *outputs,
                                                 struct wl_output *wl_output) {
  struct output *output;
  wl_list_for_each(output, outputs, link) {
    if (wl_output == output->wl_output) {
      return output;
    }
  }

  return NULL;
}

static void handle_surface_enter(void *data, struct wl_surface *surface,
                                 struct wl_output *wl_output) {
  struct peekaboo *peekaboo = data;
  struct output *output =
      find_output_from_wl_output(&peekaboo->outputs, wl_output);
  peekaboo->current_output = output;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
static const struct wl_surface_listener surface_listener = {
    .enter = handle_surface_enter,
    .leave = (void *)noop,
    .preferred_buffer_transform = (void *)noop,
    .preferred_buffer_scale = (void *)noop,
};
#pragma GCC diagnostic pop
// }}}

// wl_registry {{{
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
        wl_registry_bind(registry, name, &zwlr_layer_shell_v1_interface, 5);
    log_debug("Bound to zwlr_layer_shell %u.\n", name);
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

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
static const struct wl_registry_listener wl_registry_listener = {
    .global = registry_global,
    .global_remove = (void *)noop,
};
#pragma GCC diagnostic pop
// }}}

static void usage(bool err) {
  fprintf(
      err ? stderr : stdout, "%s",
      "Usage: peekaboo [options]\n"
      "\n"
      "Basic options:\n"
      "  -h, --help                           Print this message and exit.\n"
      "  -c, --config <path>                  Specify a config file.\n");
}

/* Option parsing with getopt. */
const struct option long_options[] = {{"help", no_argument, NULL, 'h'},
                                      {"config", required_argument, NULL, 'c'},
                                      {NULL, 0, NULL, 0}};
const char *short_options = "hc:";

static void parse_args(struct peekaboo *peekaboo, int argc, char **argv) {
  int option_index = 0;

  /* Handle errors ourselves. */
  opterr = 0;

  optind = 1;
  int opt;
  while ((opt = getopt_long(argc, argv, short_options, long_options,
                            &option_index)) != -1) {
    if (opt == 'h') {
      usage(false);
      exit(EXIT_SUCCESS);
    } else if (opt == 'c') {
      peekaboo->config_path =
          realloc(peekaboo->config_path, strlen(optarg) + 1),
      strcpy(peekaboo->config_path, optarg);
    } else {
      usage(true);
      exit(EXIT_FAILURE);
    }
  }
}

int main(int argc, char **argv) {
#ifdef DEBUG
  launch_time_ms = gettime_ms();
#endif
  struct peekaboo peekaboo = {
      .config =
          {
              .client_filter_behavior = CLIENT_FILTER_BEHAVIOR_DIM,
              .font = "Sans",
              .font_size = 16,
              .preview = {.style =
                              {
                                  .background_color = 0x000000ff,
                              }},
              .preview_title = {.style = {.align = {.horizontal = ALIGN_CENTER,
                                                    .vertical = ALIGN_END}}},
              .shortcut = {.style =
                               {
                                   .foreground_color = 0xffffffff,
                                   .highlight_color = 0xff0000ff,
                                   .background_color = 0x000000ff,
                               }},
          },
      .request_frame = request_frame,
      .running = true,
      .selected_client = NULL,
  };
  parse_args(&peekaboo, argc, argv);
  if (!config_load(&peekaboo.config, &peekaboo.config_path)) {
    log_warning("Configuration files had errors, but will try to continue.\n");
  }

#ifdef DEBUG
  log_debug("Loaded config after %ums\n", gettime_ms() - launch_time_ms);
#endif

  wl_list_init(&peekaboo.outputs);
  wl_list_init(&peekaboo.seats);
  wl_list_init(&peekaboo.wm_clients);

  /* Prepare for first roundtrip. */

  /* Connect to registry and add listeners. */
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
  EXPECT_NON_NULL(peekaboo.hyprland_toplevel_export_manager,
                  "hyprland_toplevel_export_manager");

  /* Prepare second roundtrip. */
  {
    /* For every output, add listeners to get logical sizes. */
    struct output *output;
    wl_list_for_each(output, &peekaboo.outputs, link) {
      output->xdg_output = zxdg_output_manager_v1_get_xdg_output(
          peekaboo.xdg_output_manager, output->wl_output);
      zxdg_output_v1_add_listener(output->xdg_output, &xdg_output_listener,
                                  output);
    }
  }

  /* Second roundtrip */
  log_debug("Starting second roundtrip\n");
  log_indent();
  wl_display_roundtrip(peekaboo.wl_display);
  log_unindent();
  log_debug("Finished second roundtrip\n");

  /* Initialize a list of clients connected to the WM. */
  wm_clients_init(&peekaboo, &peekaboo.wm_clients, WM_CLIENT_HYPRLAND);

  /* In the next roundtrip/dispatch, what should happen for hyprland's
   * export_frames is:
   * 1. For each export_frame, we receive "buffer" event(s) informing us of
   * the buffer parameters like width, height, etc. that the export frames can
   *    support. We currently only take the last event.
   * 2. We receive a "buffer_done" event when there are no more "buffer"
   * events. Then, we request a copy on the export_frame.
   * 3. We receive a "ready" event when the copy is finished, allowing
   *    previously unready preview windows to display a buffer.
   */

  surface_buffer_pool_init(&peekaboo.surface_buffer_pool);
  peekaboo.wl_surface = wl_compositor_create_surface(peekaboo.wl_compositor);
  wl_surface_add_listener(peekaboo.wl_surface, &surface_listener, &peekaboo);

  peekaboo.wl_layer_surface = zwlr_layer_shell_v1_get_layer_surface(
      peekaboo.wl_layer_shell, peekaboo.wl_surface,
      peekaboo.current_output == NULL ? NULL
                                      : peekaboo.current_output->wl_output,
      ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY, "selection");
  zwlr_layer_surface_v1_add_listener(peekaboo.wl_layer_surface,
                                     &wl_layer_surface_listener, &peekaboo);
  zwlr_layer_surface_v1_set_exclusive_zone(peekaboo.wl_layer_surface, -1);
  zwlr_layer_surface_v1_set_anchor(peekaboo.wl_layer_surface,
                                   ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT |
                                       ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT |
                                       ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
                                       ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM);
  zwlr_layer_surface_v1_set_keyboard_interactivity(
      peekaboo.wl_layer_surface,
      /* We need this otherwise the focus windows dispatch won't actually
       * focus the keyboard on the new window. */
      ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_ON_DEMAND);

  struct wp_fractional_scale_v1 *fractional_scale = NULL;
  if (peekaboo.fractional_scale_mgr) {
    fractional_scale = wp_fractional_scale_manager_v1_get_fractional_scale(
        peekaboo.fractional_scale_mgr, peekaboo.wl_surface);
    wp_fractional_scale_v1_add_listener(fractional_scale,
                                        &fractional_scale_listener, &peekaboo);
  }

  peekaboo.wp_viewport =
      wp_viewporter_get_viewport(peekaboo.wp_viewporter, peekaboo.wl_surface);

#ifdef DEBUG
  log_debug("About to do first render after %ums\n",
            gettime_ms() - launch_time_ms);
#endif
  wl_surface_commit(peekaboo.wl_surface);

  while (peekaboo.running && wl_display_dispatch(peekaboo.wl_display) != -1) {
  }

  if (peekaboo.selected_client != NULL) {
    wm_client_focus(peekaboo.selected_client);
  }

  /* Cleanup only when debugging to make sure we've handled everything
   * correctly.*/
  // {{{
#ifdef DEBUG
  /* Free outputs */
  {
    struct output *output;
    struct output *tmp;
    wl_list_for_each_safe(output, tmp, &peekaboo.outputs, link) {
      wl_output_destroy(output->wl_output);
      zxdg_output_v1_destroy(output->xdg_output);
      wl_list_remove(&output->link);
      free(output->name);
      free(output);
    }
  }

  /* Free seats */
  {
    struct seat *seat;
    struct seat *tmp;
    wl_list_for_each_safe(seat, tmp, &peekaboo.seats, link) {
      if (seat->wl_keyboard != NULL) {
        wl_keyboard_destroy(seat->wl_keyboard);
      }

      if (seat->xkb_state != NULL) {
        xkb_state_unref(seat->xkb_state);
      }
      if (seat->xkb_keymap != NULL) {
        xkb_keymap_unref(seat->xkb_keymap);
      }
      xkb_context_unref(seat->xkb_context);

      wl_seat_destroy(seat->wl_seat);
      wl_list_remove(&seat->link);
      free(seat);
    }
  }

  wm_clients_destroy(&peekaboo.wm_clients, WM_CLIENT_HYPRLAND);
  surface_buffer_pool_destroy(&peekaboo.surface_buffer_pool);
  if (peekaboo.wl_surface_callback) {
    wl_callback_destroy(peekaboo.wl_surface_callback);
  }
  wl_surface_destroy(peekaboo.wl_surface);

  wp_viewport_destroy(peekaboo.wp_viewport);
  wp_viewporter_destroy(peekaboo.wp_viewporter);

  hyprland_toplevel_export_manager_v1_destroy(
      peekaboo.hyprland_toplevel_export_manager);

  zxdg_output_manager_v1_destroy(peekaboo.xdg_output_manager);
  zwlr_layer_surface_v1_destroy(peekaboo.wl_layer_surface);

  wl_shm_destroy(peekaboo.wl_shm);
  zwlr_layer_shell_v1_destroy(peekaboo.wl_layer_shell);
  wl_compositor_destroy(peekaboo.wl_compositor);
  wl_registry_destroy(peekaboo.wl_registry);

  if (peekaboo.config_path != NULL) {
    free(peekaboo.config_path);
  }

  log_debug("Cleanup finished\n");
#endif /* DEBUG */
       // }}}

  wl_display_disconnect(peekaboo.wl_display);

  return 0;
}
// vim:foldmethod=marker
