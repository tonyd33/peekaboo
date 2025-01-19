#ifndef _SURFACE_BUFFER_H_
#define _SURFACE_BUFFER_H_

#include "config.h"
#include <cairo/cairo.h>
#include <pango/pangocairo.h>
#include <wayland-client.h>

enum surface_buffer_state {
  // This must be set to 0 as we set the whole structure to 0 when it's not
  // initialized.
  SURFACE_BUFFER_UNITIALIZED = 0,

  SURFACE_BUFFER_READY =       1,
  SURFACE_BUFFER_BUSY =        2,
};

struct surface_buffer {
  enum surface_buffer_state state;
  struct wl_buffer          *wl_buffer;
  cairo_surface_t           *cairo_surface;
  cairo_t                   *cairo;
  void                      *data;
  PangoLayout               *pango_layout;
  PangoContext              *pango_context;
  size_t                    data_size;
  uint32_t                  width;
  uint32_t                  height;
};

struct surface_buffer_pool {
  struct surface_buffer buffers[2];
};

void surface_buffer_pool_init(struct surface_buffer_pool *pool);
void surface_buffer_pool_destroy(struct surface_buffer_pool *pool);

struct surface_buffer *get_next_buffer(struct config *config,
                                       struct wl_shm *wl_shm,
                                       struct surface_buffer_pool *pool,
                                       uint32_t width, uint32_t height);

/* The longest part of a render is scaling the surfaces. For near-fullscreen
 * surfaces at high resolution (3440x1440), I've seen them take upwards of 16ms.
 * Most of the time, we're scaling to the same surface to the exact same
 * dimensions anyway, so we'd like to use a cache. This functions exposes a
 * simple implementation of such a cache. */

struct surface_cache_entry {
  cairo_surface_t *scaled_surface;
  int scaled_width;
  int scaled_height;
};

struct surface_cache {
  cairo_surface_t *source_surface;
  int source_width;
  int source_height;

  struct vec *entries;
};

struct surface_cache *surface_cache_init(cairo_surface_t *source_surface);

void surface_cache_destroy(struct surface_cache *cache);

cairo_surface_t *surface_cache_get_scaled(struct surface_cache *cache,
                                          int new_width, int new_height);

#endif /* _SURFACE_BUFFER_H_ */
