#include "surface_cache.h"
#include "cairo.h"
#include "src/vec.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct surface_cache *surface_cache_init(cairo_surface_t *source_surface) {
  struct surface_cache *cache = malloc(sizeof(struct surface_cache));
  cache->source_surface = source_surface;
  cache->source_width = cairo_image_surface_get_width(source_surface);
  cache->source_height = cairo_image_surface_get_height(source_surface);

  cache->entries = vec_init();
  return cache;
}

void surface_cache_destroy(struct surface_cache *cache) {
  struct surface_cache_entry *surface_cache_entry;
  for (uint32_t i = 0; i < cache->entries->count; i++) {
    surface_cache_entry = cache->entries->items[i];
    cairo_surface_destroy(surface_cache_entry->scaled_surface);

    memset(surface_cache_entry, 0, sizeof(struct surface_cache_entry));
    free(cache->entries->items[i]);
  }
  vec_destroy(cache->entries);
  memset(cache, 0, sizeof(struct surface_cache));
  free(cache);
}

struct surface_cache_entry *find_cache_entry(struct surface_cache *cache,
                                             int new_width, int new_height) {
  struct surface_cache_entry *surface_cache_entry;
  for (uint32_t i = 0; i < cache->entries->count; i++) {
    surface_cache_entry = cache->entries->items[i];
    if (surface_cache_entry->scaled_width == new_width &&
        surface_cache_entry->scaled_height == new_height) {
      return surface_cache_entry;
    }
  }
  return NULL;
}

cairo_surface_t *surface_cache_get_scaled(struct surface_cache *cache,
                                          int new_width, int new_height) {

  struct surface_cache_entry *surface_cache_entry =
      find_cache_entry(cache, new_width, new_height);
  if (surface_cache_entry == NULL) {
    // Create a new scaled surface
    surface_cache_entry = calloc(1, sizeof(struct surface_cache_entry));

    surface_cache_entry->scaled_surface =
        cairo_image_surface_create(CAIRO_FORMAT_ARGB32, new_width, new_height);
    cairo_t *scaled_ctx = cairo_create(surface_cache_entry->scaled_surface);

    // Calculate scaling factors
    double scale_x = (double)new_width / cache->source_width;
    double scale_y = (double)new_height / cache->source_height;

    // Apply scaling and draw the source surface onto the scaled surface
    cairo_scale(scaled_ctx, scale_x, scale_y);
    cairo_set_source_surface(scaled_ctx, cache->source_surface, 0, 0);
    cairo_paint(scaled_ctx);

    cairo_destroy(scaled_ctx);

    // Update cache metadata
    surface_cache_entry->scaled_width = new_width;
    surface_cache_entry->scaled_height = new_height;

    vec_append(cache->entries, surface_cache_entry);
  }

  return surface_cache_entry->scaled_surface;
}
