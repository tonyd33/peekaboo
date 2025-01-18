#ifndef _CONFIG_H_
#define _CONFIG_H_

#include "styles.h"

enum client_filter_behavior {
  CLIENT_FILTER_BEHAVIOR_NONE,
  CLIENT_FILTER_BEHAVIOR_DIM,
  CLIENT_FILTER_BEHAVIOR_HIDE,
};

struct config {
  enum client_filter_behavior client_filter_behavior;
  char                        font[512];
  int32_t                     font_size;
  struct                      {
    struct element_style      style;
  }                           peekaboo;
  struct                      {
    struct element_style      style;
  }                           preview;
  struct                      {
    struct element_style      style;
  }                           preview_title;
  struct                      {
    struct element_style      style;
  }                           shortcut;
};

#endif /* _CONFIG_H_ */
