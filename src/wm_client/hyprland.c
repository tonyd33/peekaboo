#include "hyprland.h"
#include "../log.h"
#include "../peekaboo.h"
#include "../shm.h"
#include "assert.h"
#include "hyprland-toplevel-export-v1.h"
#include "wm_client.h"
#include <cjson/cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client-core.h>
#include <wayland-util.h>

void noop() {}

/* Used to get the list of hyprland clients. This is really quite a janky
 * solution. We should be writing to the hyprland socket instead, but I'm having
 * trouble figuring out what to send to the socket to receive the clients.
 * Preliminary testing with `echo "/clients" socat - UNIX-CONNECT:/path` has
 * given me an unknown request output.
 * TODO: Write to socket to get list of hyprland clients instead. */
char *run_command(const char *command) {
  FILE *pipe;
  char buffer[128];
  size_t total_size = 0;
  char *result = NULL;

  // Open a pipe to run the command
  pipe = popen(command, "r");
  if (!pipe) {
    perror("popen failed");
    return NULL;
  }

  // Read the command's output into the buffer and concatenate it into result
  while (fgets(buffer, sizeof(buffer), pipe)) {
    size_t len = strlen(buffer);
    char *new_result = realloc(result, total_size + len + 1);
    if (!new_result) {
      perror("realloc failed");
      free(result);
      pclose(pipe);
      return NULL;
    }

    result = new_result;
    strcpy(result + total_size, buffer);
    total_size += len;
  }

  pclose(pipe);
  return result;
}

static void handle_hyprland_toplevel_export_frame_buffer(
    void *data,
    struct hyprland_toplevel_export_frame_v1 *hyprland_toplevel_export_frame,
    uint32_t format, uint32_t width, uint32_t height, uint32_t stride) {
  log_debug("hyprland_toplevel_export_frame buffer event received\n");

  struct peekaboo *peekaboo = data;

  /*
   * I didn't realize this when I started implementing, but presumably, more
   * than one "buffer" event indicates multiple buffer parameters that this
   * export_frame can use, and we can choose which one we want.
   * The current implementation will take only the last buffer event to use
   * for the buffer parameters. I've only seen each export_frame send one
   * buffer event, but we should probably handle multiple buffer events.
   *
   * TODO: Handle multiple buffer events
   */

  struct wm_client *wm_client;
  struct hyprland_client *hyprland_client;
  wl_list_for_each(wm_client, &peekaboo->wm_clients, link) {
    hyprland_client = wm_client->client;
    if (hyprland_client->toplevel_export_frame ==
        hyprland_toplevel_export_frame) {
      /* Fill in the export_frame's fields for copying. */
      wm_client->buffer_params_needs_update =
          wm_client->width != width || wm_client->height != height ||
          wm_client->stride != stride || wm_client->format != format;

      wm_client->width = width;
      wm_client->height = height;
      wm_client->stride = stride;
      wm_client->format = format;
    }
  }
}

void handle_hyprland_toplevel_export_frame_buffer_done(
    void *data,
    struct hyprland_toplevel_export_frame_v1 *hyprland_toplevel_export_frame) {
  log_debug("hyprland_toplevel_export_frame buffer done event received\n");
  struct peekaboo *peekaboo = data;
  struct wm_client *wm_client;
  struct hyprland_client *hyprland_client;
  wl_list_for_each(wm_client, &peekaboo->wm_clients, link) {
    hyprland_client = wm_client->client;

    if (hyprland_client->toplevel_export_frame ==
        hyprland_toplevel_export_frame) {
      uint32_t width = wm_client->width;
      uint32_t height = wm_client->height;
      uint32_t stride = wm_client->stride;
      uint32_t format = wm_client->format;
      uint32_t data_size = wm_client->height * wm_client->stride;

      if (wm_client->buffer_params_needs_update && wm_client->buf != NULL) {
        wl_buffer_destroy(wm_client->wl_buffer);
        munmap(wm_client->buf, wm_client->height * wm_client->stride);

        wm_client->buffer_params_needs_update = false;
      }

      int fd = shm_allocate_file(data_size);
      if (fd < 0) {
        log_error("Failed to allocate file descriptor\n");
        return;
      }

      wm_client->buf =
          mmap(NULL, data_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
      if ((int64_t)wm_client->buf < 0) {
        log_error("Failed to memory map buffer\n");
        return;
      }

      struct wl_shm_pool *wl_shm_pool =
          wl_shm_create_pool(peekaboo->wl_shm, fd, data_size);
      wm_client->wl_buffer = wl_shm_pool_create_buffer(wl_shm_pool, 0, width,
                                                       height, stride, format);
      hyprland_toplevel_export_frame_v1_copy(hyprland_toplevel_export_frame,
                                             wm_client->wl_buffer, false);

      /* Cleanup */
      wl_shm_pool_destroy(wl_shm_pool);
      close(fd);
    }
  }
}

/* Called when copying the export_frame is finished. */
void handle_hyprland_toplevel_export_frame_ready(
    void *data,
    struct hyprland_toplevel_export_frame_v1 *hyprland_toplevel_export_frame,
    uint32_t tv_sec_hi, uint32_t tv_sec_lo, uint32_t tv_nsec) {
  log_debug("hyprland_toplevel_export_frame ready event received\n");
  struct peekaboo *peekaboo = data;

  struct wm_client *wm_client;
  struct hyprland_client *hyprland_client;
  wl_list_for_each(wm_client, &peekaboo->wm_clients, link) {
    hyprland_client = wm_client->client;
    if (hyprland_client->toplevel_export_frame ==
        hyprland_toplevel_export_frame) {
      wm_client->ready = true;

      log_debug("Trying to capture next export_frame\n");
      hyprland_client->toplevel_export_frame =
          hyprland_toplevel_export_manager_v1_capture_toplevel(
              peekaboo->hyprland_toplevel_export_manager, 0,
              hyprland_client->address);
    }
  }

  peekaboo->request_frame(peekaboo);
}

void handle_hyprland_toplevel_export_frame_damage(
    void *data,
    struct hyprland_toplevel_export_frame_v1 *hyprland_toplevel_export_frame,
    uint32_t format, uint32_t width, uint32_t height, uint32_t stride) {}

const struct hyprland_toplevel_export_frame_v1_listener
    hyprland_toplevel_export_frame_listener = {
        .buffer = handle_hyprland_toplevel_export_frame_buffer,
        .damage = handle_hyprland_toplevel_export_frame_damage,
        .flags = (void *)noop,
        .ready = handle_hyprland_toplevel_export_frame_ready,
        .failed = (void *)noop,
        .linux_dmabuf = (void *)noop,
        .buffer_done = handle_hyprland_toplevel_export_frame_buffer_done,
};


void hyprland_clients_init(struct peekaboo *peekaboo,
                           struct wl_list *wm_clients) {
  char *clients_str = run_command("hyprctl clients -j");
  cJSON *clients_json = cJSON_Parse(clients_str);

  if (!cJSON_IsArray(clients_json)) {
    log_warning("Unexpected hyprctl result\n");
    return;
  }

  cJSON *client = clients_json->child;
  char key = 'a';
  while (client != NULL) {
    if (!cJSON_IsObject(client)) {
      log_warning("Unexpected hyprctl result\n");
      return;
    }
    cJSON *hyprland_client_entry = client->child;
    struct wm_client *wm_client = calloc(1, sizeof(struct wm_client));
    struct hyprland_client *hyprland_client =
        calloc(1, sizeof(struct hyprland_client));
    hyprland_client->address = -1;
    wm_client->peekaboo = peekaboo;
    wm_client->wm_client_type = WM_CLIENT_HYPRLAND;
    wm_client->client = hyprland_client;
    wm_client->ready = false;
    // TODO: Assign proper shortcut
    wm_client->shortcut_keys[0] = key++;

    while (hyprland_client_entry != NULL) {
      if (strcmp(hyprland_client_entry->string, "address") == 0 &&
          // We're given a hex string in the form "0xabcdef".
          cJSON_IsString(hyprland_client_entry)) {
        log_debug("Found address at %s\n", hyprland_client_entry->valuestring);
        hyprland_client->address =
            strtoull(hyprland_client_entry->valuestring, NULL, 0);
      } else if (strcmp(hyprland_client_entry->string, "title") == 0 &&
                 cJSON_IsString(hyprland_client_entry)) {
        strncpy(wm_client->title, hyprland_client_entry->valuestring,
                WM_CLIENT_MAX_TITLE_LENGTH - 1);
      }

      hyprland_client_entry = hyprland_client_entry->next;
    }
    log_debug("Registering listener for %s at address 0x%lx\n",
              wm_client->title, hyprland_client->address);
    /* Create an export frame for the client and listen on the export
     * frame for their buffer. */
    hyprland_client->toplevel_export_frame =
        hyprland_toplevel_export_manager_v1_capture_toplevel(
            peekaboo->hyprland_toplevel_export_manager, 0,
            hyprland_client->address);
    hyprland_toplevel_export_frame_v1_add_listener(
        hyprland_client->toplevel_export_frame,
        &hyprland_toplevel_export_frame_listener, peekaboo);

    wl_list_insert(wm_clients, &wm_client->link);
    client = client->next;
  }
  cJSON_free(clients_json);
  free(clients_str);

  /* Don't make roundtrip here. Let caller do it. */
}

void hyprland_clients_destroy(struct wl_list *wm_clients) {
  struct wm_client *wm_client;
  struct wm_client *tmp;
  wl_list_for_each_safe(wm_client, tmp, wm_clients, link) {
    free(wm_client->client);
    free(wm_client);
  }
}

void hyprland_client_focus(struct wm_client *wm_client) {
  char command[WM_CLIENT_MAX_TITLE_LENGTH + 64];
  struct hyprland_client *hyprland_client = wm_client->client;
  sprintf(command, "hyprctl dispatch focuswindow address:0x%lx",
          hyprland_client->address);
  char *res = run_command(command);
  free(res);
}
