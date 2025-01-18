#ifndef _SURFACE_CACHE_H_
#define _SURFACE_CACHE_H_
#include <cairo.h>

/* The longest part of a render is scaling the surfaces. For near-fullscreen
 * surfaces at high resolution (3440x1440), I've seen them take upwards of 16ms.
 * Most of the time, we're scaling to the same surface to the exact same
 * dimensions anyway, so we'd like to use a cache. This file exposes a simple
 * implementation of such a cache. */

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

bool surface_cache_needs_rescale(struct surface_cache *cache, int new_width,
                                 int new_height);

cairo_surface_t *surface_cache_get_scaled(struct surface_cache *cache,
                                          int new_width, int new_height);

#endif /* _SURFACE_CACHE_H_ */
