#ifndef _STYLES_H_
#define _STYLES_H_

#include <stdint.h>

typedef uint32_t color_t;

/* For things like padding, margin, etc. */
struct directional {
  int32_t top, bottom, left, right;
};

struct border {
  uint32_t width;
  color_t  color;
  uint32_t radius;
};

enum align_e {
  ALIGN_START,
  ALIGN_CENTER,
  ALIGN_END,
};

struct align {
  enum align_e vertical;
  enum align_e horizontal;
};

struct element_style {
  color_t            foreground_color;
  color_t            highlight_color;
  color_t            background_color;
  struct border      border;
  struct align       align;
  struct directional padding;
  struct directional margin;
};

#endif /* _STYLES_H_ */
