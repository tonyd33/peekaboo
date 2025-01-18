#ifndef _STYLES_H_
#define _STYLES_H_

#include <stdint.h>

typedef uint32_t color;

/* For things like padding, margin, etc. */
struct directional {
  int32_t top, bottom, left, right;
};

struct element_style {
  uint32_t foreground_color;
  uint32_t highlight_color;
  uint32_t background_color;
  struct directional padding;
  struct directional margin;
  uint32_t background_corner_radius;
};

#endif /* _STYLES_H_ */
