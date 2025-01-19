#ifndef _LAYOUT_H_
#define _LAYOUT_H_
#include "vec.h"
#include <stdint.h>

/* This file is dedicated to algorithms to determine where each preview should
 * go and the dimensions they should be assigned, given a number of previews
 * we need to show. */

struct rect {
  double x, y;
  double width, height;
};

struct layout {
  struct vec *preview_geometries;
  uint32_t num_rows, num_cols;
};

/*
 * - Each preview will have the same dimensions
 * - Each preview will have the same aspect ratio as the surface dimensions
 * - The previews are divided into a grid, where the number of rows and number
 *   of columns of the _grid_ with the aspect ratio closest to the surface's
 *   aspect ratio is chosen
 * - If there is empty leftover space on the grid, then the grid is centered
 *   on the surface
 */
struct layout *calculate_layout_fixed_individual_aspect_ratio(
    uint32_t num_previews, double surface_width, double surface_height);

void layout_destroy(struct layout *layout);

#endif /* _LAYOUT_H_ */
