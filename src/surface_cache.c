#include "surface_cache.h"
#include <stdlib.h>

struct surface_cache *surface_cache_create(cairo_surface_t *source_surface) {
  struct surface_cache *cache = malloc(sizeof(struct surface_cache));
  cache->source_surface = source_surface;
  cache->source_width = cairo_image_surface_get_width(source_surface);
  cache->source_height = cairo_image_surface_get_height(source_surface);
  cache->scaled_surface = NULL;
  cache->scaled_width = 0;
  cache->scaled_height = 0;
  return cache;
}

void surface_cache_destroy(struct surface_cache *cache) {
  if (cache->scaled_surface) {
    cairo_surface_destroy(cache->scaled_surface);
  }
  free(cache);
}

bool surface_cache_needs_rescale(struct surface_cache *cache, int new_width,
                                 int new_height) {
  return cache->scaled_surface == NULL || cache->scaled_width != new_width ||
         cache->scaled_height != new_height;
}

cairo_surface_t *surface_cache_get_scaled(struct surface_cache *cache,
                                          int new_width, int new_height) {
  if (surface_cache_needs_rescale(cache, new_width, new_height)) {
    // Destroy the old scaled surface if it exists
    if (cache->scaled_surface) {
      cairo_surface_destroy(cache->scaled_surface);
    }

    // Create a new scaled surface
    cache->scaled_surface =
        cairo_image_surface_create(CAIRO_FORMAT_ARGB32, new_width, new_height);
    cairo_t *scaled_ctx = cairo_create(cache->scaled_surface);

    // Calculate scaling factors
    double scale_x = (double)new_width / cache->source_width;
    double scale_y = (double)new_height / cache->source_height;

    // Apply scaling and draw the source surface onto the scaled surface
    cairo_scale(scaled_ctx, scale_x, scale_y);
    cairo_set_source_surface(scaled_ctx, cache->source_surface, 0, 0);
    cairo_paint(scaled_ctx);

    cairo_destroy(scaled_ctx);

    // Update cache metadata
    cache->scaled_width = new_width;
    cache->scaled_height = new_height;
  }

  return cache->scaled_surface;
}
