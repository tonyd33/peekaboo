// vim:foldmethod=marker
#include "hyprland.h"
#include "../log.h"
#include "../peekaboo.h"
#include "../shm.h"
#include "assert.h"
#include "cairo.h"
#include "hyprland-toplevel-export-v1.h"
#include "src/surface_cache.h"
#include "wm_client.h"
#include <cjson/cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <wayland-client-core.h>
#include <wayland-util.h>

static void noop() {}

char *send_hyprland_socket(const char *command) {
  int sockfd;
  struct sockaddr_un addr;

  char socket_path[256] = {0};
  const char *xdg_runtime_dir = getenv("XDG_RUNTIME_DIR");
  const char *hyprland_instance_signature =
      getenv("HYPRLAND_INSTANCE_SIGNATURE");
  sprintf(socket_path, "%s/hypr/%s/.socket.sock", xdg_runtime_dir,
          hyprland_instance_signature);

  log_debug("Connecting to unix socket at: %s\n", socket_path);

  sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (sockfd == -1) {
    perror("socket");
    exit(EXIT_FAILURE);
  }

  memset(&addr, 0, sizeof(struct sockaddr_un));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

  if (connect(sockfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_un)) ==
      -1) {
    perror("connect");
    close(sockfd);
    exit(EXIT_FAILURE);
  }

  if (write(sockfd, command, strlen(command)) == -1) {
    perror("write");
    close(sockfd);
    exit(EXIT_FAILURE);
  }

  ssize_t num_bytes;
  char buffer[1024];
  char *response = NULL;
  size_t response_size = 0;

  // Read the response in chunks
  while ((num_bytes = read(sockfd, buffer, 1024)) > 0) {
    char *temp = realloc(response,
                         response_size + num_bytes + 1); // Allocate more memory
    if (!temp) {
      perror("realloc");
      free(response);
      close(sockfd);
      exit(EXIT_FAILURE);
    }
    response = temp;
    memcpy(response + response_size, buffer, num_bytes); // Append new data
    response_size += num_bytes;
  }

  if (num_bytes == -1) {
    perror("read");
    free(response);
    close(sockfd);
    exit(EXIT_FAILURE);
  }

  response[response_size] = '\0';
  close(sockfd);

  return response;
}

// hyprland_toplevel_export_frame {{{
static void handle_hyprland_toplevel_export_frame_buffer(
    void *data,
    struct hyprland_toplevel_export_frame_v1 *hyprland_toplevel_export_frame,
    uint32_t format, uint32_t width, uint32_t height, uint32_t stride) {
  struct peekaboo *peekaboo = data;

  /*
   * I didn't realize this when I started implementing, but presumably, more
   * than one "buffer" event indicates multiple buffer parameters that this
   * export_frame can use, and we can choose which one we want.
   * The current implementation will take only the last buffer event to use
   * for the buffer parameters. I've only seen each export_frame send one
   * buffer event, but we should probably handle multiple buffer events.
   *
   * TODO: (Maybe) Handle multiple buffer events
   */

  struct wm_client *wm_client;
  struct hyprland_client *hyprland_client;
  wl_list_for_each(wm_client, &peekaboo->wm_clients, link) {
    hyprland_client = wm_client->client;
    if (hyprland_client->toplevel_export_frame ==
        hyprland_toplevel_export_frame) {

      /* Do I need to destroy the wl_buffer before unmapping buf? I don't know,
       * but I'll do it anyway. */
      if (wm_client->wl_buffer != NULL) {
        wl_buffer_destroy(wm_client->wl_buffer);
        wm_client->wl_buffer = NULL;
      }

      /* If we're updating an existing client, this is really the best time to
       * free the buffer because after we set the (possibly new) buffer
       * parameters, we'll lose track of what to unmap. */
      if (wm_client->buf != NULL) {
        munmap(wm_client->buf, wm_client->height * wm_client->stride);
        wm_client->buf = NULL;
      }

      /* Fill in the export_frame's fields for copying. */
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

      /* Ideally, I'd like to just reuse the same shm pool and fd, but I've
       * had a lot of trouble getting it to work.
       * TODO: Use a single wl_shm_pool. */
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
  struct peekaboo *peekaboo = data;

  struct wm_client *wm_client;
  struct hyprland_client *hyprland_client;
  wl_list_for_each(wm_client, &peekaboo->wm_clients, link) {
    hyprland_client = wm_client->client;
    if (hyprland_client->toplevel_export_frame ==
        hyprland_toplevel_export_frame) {
      if (wm_client->surface_cache) {
        surface_cache_destroy(wm_client->surface_cache);
      }
      if (wm_client->orig_surface) {
        cairo_surface_destroy(wm_client->orig_surface);
      }

      // TODO: Check the format is compatible
      wm_client->orig_surface = cairo_image_surface_create_for_data(
          wm_client->buf, CAIRO_FORMAT_ARGB32, wm_client->width,
          wm_client->height, wm_client->stride);
      wm_client->surface_cache = surface_cache_init(wm_client->orig_surface);
      wm_client->ready = true;

      peekaboo->request_frame(peekaboo);
      /* Refreshing currently doesn't work. We may try to render a frame while
       * requesting the new buffer. In doing so, we release or otherwise modify
       * resources needed for rendering the frame, causing flickering at best,
       * and a segfault at worst. If we want to implement this a way to refresh
       * windows, we should implement double-buffering on the wm_client
       * buffers. Or, maybe if we memcpy the shm buffer into our own buffer? */
      /* hyprland_clients_refresh(peekaboo, &peekaboo->wm_clients); */
    }
  }
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
const struct hyprland_toplevel_export_frame_v1_listener
    hyprland_toplevel_export_frame_listener = {
        .buffer = handle_hyprland_toplevel_export_frame_buffer,
        .damage = (void *)noop,
        .flags = (void *)noop,
        .ready = handle_hyprland_toplevel_export_frame_ready,
        .failed = (void *)noop,
        .linux_dmabuf = (void *)noop,
        .buffer_done = handle_hyprland_toplevel_export_frame_buffer_done,
};
#pragma GCC diagnostic pop
// }}}

static const char character_pool[] = {'f', 'j', 'd', 'k', 's', 'l', 'a', ';'};

/* Generate a set of characters to be pressed.
 * Essentially does base conversion with the character_pool acting as the base,
 * but with the additional constraint that every string generated with
 * 0 <= index <= total while be mutually prefix-free. In other words, for a
 * fixed `total`, we won't generate strings like 'f' and 'ff' together. */
void generate_key_shortcut(uint32_t index, uint32_t total, char *into) {
  uint32_t into_index = 0;
  uint32_t character_pool_size = sizeof(character_pool);
  /* Honestly, I'm not sure how this works or even _that_ this works. I just
   * decided to try this line while brainstorming how to implement and it
   * seems to give pretty good shortcuts. It doesn't pack sequences as tightly
   * as possible though.
   * TODO: Improve this algorithm. */
  index += total;
  do {
    into[into_index++] = character_pool[index % character_pool_size];
    index /= character_pool_size;
  } while (index > 0);
}

void hyprland_clients_init(struct peekaboo *peekaboo,
                           struct wl_list *wm_clients) {
  char *clients_str = send_hyprland_socket("-j/clients");
  cJSON *clients_json = cJSON_Parse(clients_str);

  if (!cJSON_IsArray(clients_json)) {
    log_warning("Unexpected hyprctl result\n");
    return;
  }

  cJSON *client = clients_json->child;
  uint32_t num_clients = 0;
  struct wm_client *wm_client;
  struct hyprland_client *hyprland_client;
  while (client != NULL) {
    if (!cJSON_IsObject(client)) {
      log_warning("Unexpected hyprctl result\n");
      return;
    }
    cJSON *hyprland_client_entry = client->child;
    wm_client = calloc(1, sizeof(struct wm_client));
    hyprland_client = calloc(1, sizeof(struct hyprland_client));
    hyprland_client->address = -1;
    wm_client->peekaboo = peekaboo;
    wm_client->wm_client_type = WM_CLIENT_HYPRLAND;
    wm_client->client = hyprland_client;
    wm_client->ready = false;

    while (hyprland_client_entry != NULL) {
      if (strcmp(hyprland_client_entry->string, "address") == 0 &&
          // We're given a hex string in the form "0xabcdef".
          cJSON_IsString(hyprland_client_entry)) {
        hyprland_client->address =
            strtoull(hyprland_client_entry->valuestring, NULL, 0);
      } else if (strcmp(hyprland_client_entry->string, "title") == 0 &&
                 cJSON_IsString(hyprland_client_entry)) {
        strncpy(wm_client->title, hyprland_client_entry->valuestring,
                WM_CLIENT_MAX_TITLE_LENGTH - 1);
      }

      hyprland_client_entry = hyprland_client_entry->next;
    }
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
    num_clients++;
  }

  cJSON_Delete(clients_json);
  free(clients_str);

  /* We have to do another loop here to generate the shortcut keys. This is
   * because the shortcut key generation depends on the total number of items
   * that will be generated. */
  uint32_t i = 0;
  wl_list_for_each(wm_client, wm_clients, link) {
    generate_key_shortcut(i, num_clients, wm_client->shortcut_keys);
    i++;
  }

  /* Don't make roundtrip here. Let caller do it. */
}

void hyprland_clients_refresh(struct peekaboo *peekaboo,
                              struct wl_list *wm_clients) {
  struct wm_client *wm_client;
  struct hyprland_client *hyprland_client;
  wl_list_for_each(wm_client, wm_clients, link) {
    hyprland_client = wm_client->client;
    if (hyprland_client->toplevel_export_frame) {
      hyprland_toplevel_export_frame_v1_destroy(
          hyprland_client->toplevel_export_frame);
    }

    hyprland_client->toplevel_export_frame =
        hyprland_toplevel_export_manager_v1_capture_toplevel(
            peekaboo->hyprland_toplevel_export_manager, 0,
            hyprland_client->address);
    hyprland_toplevel_export_frame_v1_add_listener(
        hyprland_client->toplevel_export_frame,
        &hyprland_toplevel_export_frame_listener, peekaboo);
  }
}

void hyprland_clients_destroy(struct wl_list *wm_clients) {
  struct wm_client *wm_client;
  struct wm_client *tmp;
  struct hyprland_client *hyprland_client;
  wl_list_for_each_safe(wm_client, tmp, wm_clients, link) {
    hyprland_client = wm_client->client;
    if (wm_client->wl_buffer) {
      wl_buffer_destroy(wm_client->wl_buffer);
    }
    if (wm_client->surface_cache) {
      surface_cache_destroy(wm_client->surface_cache);
    }
    if (wm_client->orig_surface) {
      cairo_surface_destroy(wm_client->orig_surface);
    }
    if (wm_client->buf) {
      munmap(wm_client->buf, wm_client->height * wm_client->stride);
    }
    if (hyprland_client->toplevel_export_frame) {
      hyprland_toplevel_export_frame_v1_destroy(
          hyprland_client->toplevel_export_frame);
    }

    memset(wm_client->client, 0, sizeof(struct hyprland_client));
    free(wm_client->client);

    memset(wm_client, 0, sizeof(struct wm_client));
    free(wm_client);
  }
}

void hyprland_client_focus(struct wm_client *wm_client) {
  char command[WM_CLIENT_MAX_TITLE_LENGTH + 64] = {0};
  struct hyprland_client *hyprland_client = wm_client->client;
  sprintf(command, "/dispatch focuswindow address:0x%lx",
          hyprland_client->address);

  char *response = send_hyprland_socket(command);
  if (strcmp(response, "ok") != 0) {
    log_error("Could not focus window\n");
    exit(EXIT_FAILURE);
  }
  free(response);
}
