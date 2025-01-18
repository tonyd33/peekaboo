#include "layout.h"
#include "log.h"
#include "src/vec.h"
#include <glib.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct layout *layout_init() {
  struct layout *layout = calloc(1, sizeof(struct layout));
  layout->preview_geometries = vec_init();
  return layout;
}

/* Layout rectangles. The layout should be as such:
 * - Each rectangle has the same dimensions
 * - Each rectangle dimensions approximate the surface buffer's aspect ratio
 *   as closely as possible
 * - Rectangles are laid out left to right, and wrap around if necessary.
 *
 * - Optionally, we may center the rectangles.
 *
 * These conditions should restrain us to a single layout.
 */
struct layout *calculate_layout_fixed_individual_aspect_ratio(
    uint32_t num_previews, double surface_width, double surface_height) {
  double surface_aspect_ratio = surface_width / surface_height;

  uint32_t best_num_cols = 1;
  double best_rect_width = 0;
  double best_rect_height = 0;
  double best_bounding_aspect_ratio_diff = 10e12;

  for (uint32_t num_cols = 1; num_cols < num_previews + 1; num_cols++) {
    double rect_width = surface_width / (double)num_cols;
    double rect_height = surface_height / (double)num_cols;
    uint32_t num_rows = ceil(surface_height / rect_height);
    double bounding_aspect_ratio =
        (rect_width * num_cols) / (rect_height * num_rows);
    double bounding_aspect_ratio_diff =
        ABS(surface_aspect_ratio - bounding_aspect_ratio);

    if (bounding_aspect_ratio_diff < best_bounding_aspect_ratio_diff &&
        num_cols * num_rows >= num_previews) {
      best_num_cols = num_cols;
      best_rect_width = rect_width;
      best_rect_height = rect_height;
      best_bounding_aspect_ratio_diff = bounding_aspect_ratio_diff;
    }
  }
  // The _actual_ number of rows we're going to display may be different from
  // what was used for layout calculations earlier.
  uint32_t best_num_rows = ceil((double)num_previews / (double)best_num_cols);

  struct layout *layout = layout_init();
  layout->num_cols = best_num_cols;
  layout->num_rows = best_num_rows;
  struct preview_geometry *preview_geometry;
  double bounding_height = best_num_rows * best_rect_height;
  // Used for centering.
  double y_offset = (surface_height - bounding_height) / 2.0;

  for (uint32_t i = 0; i < num_previews; i++) {
    preview_geometry = calloc(1, sizeof(struct preview_geometry));
    preview_geometry->x = (i % best_num_cols) * best_rect_width;
    preview_geometry->y =
        (floor((double)i / best_num_cols) * best_rect_height) + y_offset;
    preview_geometry->width = best_rect_width;
    preview_geometry->height = best_rect_height;
    vec_append(layout->preview_geometries, preview_geometry);
  }

  return layout;
}

void layout_destroy(struct layout *layout) {
  for (size_t i = 0; i < layout->preview_geometries->count; i++) {
    memset(layout->preview_geometries->items[i], 0,
           sizeof(struct preview_geometry));
    free(layout->preview_geometries->items[i]);
  }
  vec_destroy(layout->preview_geometries);

  memset(layout, 0, sizeof(struct layout));
  free(layout);
}
