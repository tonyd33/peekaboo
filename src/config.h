#ifndef _CONFIG_H_
#define _CONFIG_H_

#include "styles.h"

#define CONFIG_FIELD_MAX_LEN 512

enum client_filter_behavior {
  CLIENT_FILTER_BEHAVIOR_NONE,
  CLIENT_FILTER_BEHAVIOR_DIM,
  CLIENT_FILTER_BEHAVIOR_HIDE,
};

struct config {
  enum client_filter_behavior client_filter_behavior;
  char                        font[CONFIG_FIELD_MAX_LEN];
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

/* Loads into config. If *config_path is NULL, tries to find a default config
 * path and loads it into *config_path. */
bool config_load(struct config *config, char **config_path);

#endif /* _CONFIG_H_ */
