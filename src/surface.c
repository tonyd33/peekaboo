#include "surface.h"
#include "log.h"
#include "shm.h"
#include <cairo/cairo.h>
#include <fcntl.h>
#include <pango/pango-font.h>
#include <pango/pango-layout.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define CAIRO_SURFACE_FORMAT CAIRO_FORMAT_ARGB32

static void handle_buffer_release(void *data, struct wl_buffer *wl_buffer) {
  ((struct surface_buffer *)data)->state = SURFACE_BUFFER_READY;
}

static const struct wl_buffer_listener wl_buffer_listener = {
    .release = handle_buffer_release,
};

static struct surface_buffer *surface_buffer_init(struct wl_shm *wl_shm,
                                                  struct surface_buffer *buffer,
                                                  int32_t width,
                                                  int32_t height) {
  const uint32_t stride =
      cairo_format_stride_for_width(CAIRO_SURFACE_FORMAT, width);
  const uint32_t data_size = height * stride;
  void *data;

  int fd = shm_allocate_file(data_size);
  if (fd < 0) {
    log_error("Could not allocate shared buffer for surface buffer.\n");
    return NULL;
  }

  data = mmap(NULL, data_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (data == MAP_FAILED) {
    log_error("Could not mmap shared buffer for surface buffer.\n");

    close(fd);
    return NULL;
  }

  struct wl_shm_pool *wl_shm_pool = wl_shm_create_pool(wl_shm, fd, data_size);
  buffer->wl_buffer = wl_shm_pool_create_buffer(wl_shm_pool, 0, width, height,
                                                stride, WL_SHM_FORMAT_ARGB8888);
  wl_buffer_add_listener(buffer->wl_buffer, &wl_buffer_listener, buffer);
  wl_shm_pool_destroy(wl_shm_pool);

  buffer->data = data;
  buffer->data_size = data_size;
  buffer->width = width;
  buffer->height = height;
  buffer->state = SURFACE_BUFFER_READY;

  buffer->cairo_surface = cairo_image_surface_create_for_data(
      buffer->data, CAIRO_SURFACE_FORMAT, width, height, stride);
  buffer->cairo = cairo_create(buffer->cairo_surface);

  /* buffer->pango_layout = pango_cairo_create_layout(buffer->cairo); */
  /* PangoFontDescription *font_desc = */
  /* pango_font_description_from_string("Comic Code 16"); */
  /* pango_layout_set_font_description(buffer->pango_layout, font_desc); */
  /* pango_font_description_free(font_desc); */

  log_debug("Creating Pango context.\n");
  PangoContext *context = pango_cairo_create_context(buffer->cairo);

  log_debug("Creating Pango font description.\n");
  PangoFontDescription *font_description =
      pango_font_description_from_string("Comic Code");
  pango_font_description_set_size(font_description, 16 * PANGO_SCALE);
  /* if (entry->font_variations[0] != 0) { */
  /* pango_font_description_set_variations( */
  /* font_description, */
  /* entry->font_variations); */
  /* } */
  pango_context_set_font_description(context, font_description);

  buffer->pango_layout = pango_layout_new(context);

  /* if (entry->font_features[0] != 0) { */
  /* log_debug("Setting font features.\n"); */
  /* PangoAttribute *attr = pango_attr_font_features_new(entry->font_features);
   */
  /* PangoAttrList *attr_list = pango_attr_list_new(); */
  /* pango_attr_list_insert(attr_list, attr); */
  /* pango_layout_set_attributes(entry->pango.layout, attr_list); */
  /* } */

  log_debug("Loading Pango font.\n");
  PangoFontMap *map = pango_cairo_font_map_get_default();
  PangoFont *font = pango_font_map_load_font(map, context, font_description);
  PangoFontMetrics *metrics = pango_font_get_metrics(font, NULL);

  pango_font_metrics_unref(metrics);
  g_object_unref(font);
  log_debug("Loaded.\n");

  pango_font_description_free(font_description);

  buffer->pango_context = context;

  return buffer;
}

static void surface_buffer_destroy(struct surface_buffer *buffer) {
  if (buffer->state == SURFACE_BUFFER_UNITIALIZED) {
    return;
  }

  if (buffer->cairo) {
    cairo_destroy(buffer->cairo);
  }

  if (buffer->cairo_surface) {
    cairo_surface_destroy(buffer->cairo_surface);
  }

  if (buffer->wl_buffer) {
    wl_buffer_destroy(buffer->wl_buffer);
  }

  if (buffer->data) {
    munmap(buffer->data, buffer->data_size);
  }

  if (buffer->pango_layout) {
    // This fixes a lot of memory leaks. Probably because pango uses this
    // internally and doesn't free it itself.
    g_object_unref(pango_cairo_font_map_get_default());
    g_object_unref(buffer->pango_layout);
  }

  if (buffer->pango_context) {
    g_object_unref(buffer->pango_context);
  }

  memset(buffer, 0, sizeof(struct surface_buffer));
}

void surface_buffer_pool_init(struct surface_buffer_pool *pool) {
  memset(pool, 0, sizeof(struct surface_buffer_pool));
}

void surface_buffer_pool_destroy(struct surface_buffer_pool *pool) {
  surface_buffer_destroy(&pool->buffers[0]);
  surface_buffer_destroy(&pool->buffers[1]);
}

struct surface_buffer *get_next_buffer(struct wl_shm *wl_shm,
                                       struct surface_buffer_pool *pool,
                                       uint32_t width, uint32_t height) {
  struct surface_buffer *buffer = NULL;
  for (size_t i = 0; i < 2; i++) {
    if (pool->buffers[i].state != SURFACE_BUFFER_BUSY) {
      buffer = &pool->buffers[i];
      break;
    }
  }

  if (buffer == NULL) {
    log_warning("All surface buffers are busy.\n");
    return NULL;
  }

  if (buffer->width != width || buffer->height != height) {
    surface_buffer_destroy(buffer);
  }

  if (buffer->state == SURFACE_BUFFER_UNITIALIZED) {
    if (surface_buffer_init(wl_shm, buffer, width, height) == NULL) {
      log_error("Could not initialize next buffer.\n");
      return NULL;
    }
  }

  return buffer;
}
