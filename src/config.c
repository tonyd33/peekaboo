#include "config.h"
#include "log.h"
#include <cyaml/cyaml.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef char *color_extended_t;

struct directional_extended {
  char *all;
  char *vertical;
  char *horizontal;
  char *top;
  char *bottom;
  char *left;
  char *right;
};

struct border_extended {
  char *width;
  color_extended_t color;
  char *radius;
};

struct element_style_extended {
  color_extended_t foreground_color;
  color_extended_t highlight_color;
  color_extended_t background_color;
  struct border_extended border;
  struct directional_extended padding;
  struct directional_extended margin;
};

struct config_peekaboo_extended {
  struct element_style_extended style;
};
struct config_preview_extended {
  struct element_style_extended style;
};
struct config_preview_title_extended {
  struct element_style_extended style;
};
struct config_shortcut_extended {
  struct element_style_extended style;
};

struct config_extended {
  char *font;
  uint32_t font_size;
  enum client_filter_behavior client_filter_behavior;
  struct config_peekaboo_extended peekaboo;
  struct config_preview_extended preview;
  struct config_preview_title_extended preview_title;
  struct config_shortcut_extended shortcut;
};

static const cyaml_strval_t client_filter_behavior_strings[] = {
    {"none", CLIENT_FILTER_BEHAVIOR_NONE},
    {"dim", CLIENT_FILTER_BEHAVIOR_DIM},
    {"hide", CLIENT_FILTER_BEHAVIOR_HIDE},
};

static const cyaml_schema_field_t direction_schema[] = {
    CYAML_FIELD_STRING_PTR("all", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL,
                           struct directional_extended, all, 0,
                           CONFIG_FIELD_MAX_LEN),
    CYAML_FIELD_STRING_PTR("vertical", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL,
                           struct directional_extended, vertical, 0,
                           CONFIG_FIELD_MAX_LEN),
    CYAML_FIELD_STRING_PTR(
        "horizontal", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL,
        struct directional_extended, horizontal, 0, CONFIG_FIELD_MAX_LEN),
    CYAML_FIELD_STRING_PTR("top", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL,
                           struct directional_extended, top, 0,
                           CONFIG_FIELD_MAX_LEN),
    CYAML_FIELD_STRING_PTR("bottom", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL,
                           struct directional_extended, bottom, 0,
                           CONFIG_FIELD_MAX_LEN),
    CYAML_FIELD_STRING_PTR("left", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL,
                           struct directional_extended, left, 0,
                           CONFIG_FIELD_MAX_LEN),
    CYAML_FIELD_STRING_PTR("right", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL,
                           struct directional_extended, right, 0,
                           CONFIG_FIELD_MAX_LEN),
    CYAML_FIELD_END,
};

static const cyaml_schema_field_t border_schema[] = {
    CYAML_FIELD_STRING_PTR("width", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL,
                           struct border_extended, width, 0,
                           CONFIG_FIELD_MAX_LEN),
    CYAML_FIELD_STRING_PTR("color", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL,
                           struct border_extended, color, 0,
                           CONFIG_FIELD_MAX_LEN),
    CYAML_FIELD_STRING_PTR("radius", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL,
                           struct border_extended, radius, 0,
                           CONFIG_FIELD_MAX_LEN),
    CYAML_FIELD_END,
};

static const cyaml_schema_field_t element_style_schema[] = {
    CYAML_FIELD_STRING_PTR("foreground_color",
                           CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL,
                           struct element_style_extended, foreground_color, 0,
                           CONFIG_FIELD_MAX_LEN),
    CYAML_FIELD_STRING_PTR("highlight_color",
                           CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL,
                           struct element_style_extended, highlight_color, 0,
                           CONFIG_FIELD_MAX_LEN),
    CYAML_FIELD_STRING_PTR("background_color",
                           CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL,
                           struct element_style_extended, background_color, 0,
                           CONFIG_FIELD_MAX_LEN),
    CYAML_FIELD_MAPPING("border", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL,
                        struct element_style_extended, border, border_schema),
    CYAML_FIELD_MAPPING("padding", CYAML_FLAG_OPTIONAL,
                        struct element_style_extended, padding,
                        direction_schema),
    CYAML_FIELD_MAPPING("margin", CYAML_FLAG_OPTIONAL,
                        struct element_style_extended, margin,
                        direction_schema),
    CYAML_FIELD_END,
};

static const cyaml_schema_field_t peekaboo_schema[] = {
    CYAML_FIELD_MAPPING("style", CYAML_FLAG_OPTIONAL,
                        struct config_peekaboo_extended, style,
                        element_style_schema),
    CYAML_FIELD_END,
};
static const cyaml_schema_field_t preview_schema[] = {
    CYAML_FIELD_MAPPING("style", CYAML_FLAG_OPTIONAL,
                        struct config_preview_extended, style,
                        element_style_schema),
    CYAML_FIELD_END,
};
static const cyaml_schema_field_t preview_title_schema[] = {
    CYAML_FIELD_MAPPING("style", CYAML_FLAG_OPTIONAL,
                        struct config_preview_title_extended, style,
                        element_style_schema),
    CYAML_FIELD_END,
};
static const cyaml_schema_field_t shortcut_schema[] = {
    CYAML_FIELD_MAPPING("style", CYAML_FLAG_OPTIONAL,
                        struct config_shortcut_extended, style,
                        element_style_schema),
    CYAML_FIELD_END,
};

static const cyaml_schema_field_t yaml_config_fields_schema[] = {
    CYAML_FIELD_ENUM("client_filter_behavior", CYAML_FLAG_OPTIONAL,
                     struct config_extended, client_filter_behavior,
                     client_filter_behavior_strings,
                     CYAML_ARRAY_LEN(client_filter_behavior_strings)),
    CYAML_FIELD_STRING_PTR("font", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL,
                           struct config_extended, font, 0,
                           CONFIG_FIELD_MAX_LEN),
    CYAML_FIELD_UINT("font_size", CYAML_FLAG_OPTIONAL, struct config_extended,
                     font_size),
    CYAML_FIELD_MAPPING("peekaboo", CYAML_FLAG_OPTIONAL, struct config_extended,
                        peekaboo, peekaboo_schema),
    CYAML_FIELD_MAPPING("preview", CYAML_FLAG_OPTIONAL, struct config_extended,
                        preview, preview_schema),
    CYAML_FIELD_MAPPING("preview_title", CYAML_FLAG_OPTIONAL,
                        struct config_extended, preview_title,
                        preview_title_schema),
    CYAML_FIELD_MAPPING("shortcut", CYAML_FLAG_OPTIONAL, struct config_extended,
                        shortcut, shortcut_schema),
    CYAML_FIELD_END,
};

static const cyaml_schema_value_t yaml_config_schema = {CYAML_VALUE_MAPPING(
    CYAML_FLAG_POINTER, struct config_extended, yaml_config_fields_schema)};

bool color_extended_load(color_t *color,
                         const color_extended_t color_extended) {
  if (color_extended == NULL || *color_extended == '\0') {
    return true;
  }
  *color = 0;

  /* If the string starts with 0x or #, parse as hex */
  bool starts_with_0x = strncmp("0x", color_extended, 2) == 0;
  bool starts_with_pound = color_extended[0] == '#';
  if (starts_with_0x || starts_with_pound) {
    char *hex_start = color_extended + (starts_with_0x ? 2 : 1);
    *color = strtoul(hex_start, NULL, 16);
    /* Specified RGB, but we need to pad alpha channel to be 1.0 */
    if (strlen(hex_start) <= 6) {
      *color <<= 8;
      *color |= 0x000000FF;
    }
    return true;
  }
  /* If the string starts with rgb, tokenize and parse components */
  else if (strncmp("rgb", color_extended, 3) == 0) {
    bool has_alpha_channel = color_extended[3] == 'a';
    int32_t tokens_to_parse = has_alpha_channel ? 4 : 3;
    uint32_t start = has_alpha_channel ? 5 : 4;

    /* Don't modify the original buffer */
    char tmp_buf[CONFIG_FIELD_MAX_LEN];
    strncpy(tmp_buf, color_extended, CONFIG_FIELD_MAX_LEN);
    tmp_buf[CONFIG_FIELD_MAX_LEN - 1] = '\0';

    uint32_t color_component;
    int32_t component_num = 4; // 4 components in RGBA
    char *rest = &tmp_buf[start];
    char *token;
    while (component_num-- >= (4 - tokens_to_parse) &&
           (token = strtok_r(rest, ",", &rest)) != NULL) {
      color_component = strtoul(token, NULL, 10) & 0xFF;
      *color |= color_component << 8 * component_num;
    }
    if (!has_alpha_channel) {
      *color |= 0x000000FF;
    }
    return true;
  }

  return false;
}

bool directional_extended_load(
    struct directional *directional,
    const struct directional_extended *directional_extended) {
  if (directional_extended == NULL) {
    return true;
  }

  if (directional_extended->all != NULL) {
    uint32_t num = strtoul(directional_extended->all, NULL, 0);
    directional->top = num;
    directional->bottom = num;
    directional->left = num;
    directional->right = num;
  }
  if (directional_extended->vertical != NULL) {
    uint32_t num = strtoul(directional_extended->vertical, NULL, 0);
    directional->top = num;
    directional->bottom = num;
  }
  if (directional_extended->horizontal != NULL) {
    uint32_t num = strtoul(directional_extended->horizontal, NULL, 0);
    directional->left = num;
    directional->right = num;
  }
  if (directional_extended->top != NULL) {
    directional->top = strtoul(directional_extended->top, NULL, 0);
  }
  if (directional_extended->bottom != NULL) {
    directional->bottom = strtoul(directional_extended->bottom, NULL, 0);
  }
  if (directional_extended->left != NULL) {
    directional->left = strtoul(directional_extended->left, NULL, 0);
  }
  if (directional_extended->right != NULL) {
    directional->right = strtoul(directional_extended->right, NULL, 0);
  }
  return true;
}

bool border_extended_load(struct border *border,
                          const struct border_extended *border_extended) {
  if (border == NULL) {
    return true;
  }
  if (border_extended->radius != NULL) {
    border->radius = strtoul(border_extended->radius, NULL, 0);
  }
  if (border_extended->width != NULL) {
    border->width = strtoul(border_extended->width, NULL, 0);
  }

  bool failed = !color_extended_load(&border->color, border_extended->color);

  return !failed;
}

bool element_style_extended_load(
    struct element_style *element_style,
    const struct element_style_extended *element_style_extended) {

  bool failed =
      !color_extended_load(&element_style->foreground_color,
                           element_style_extended->foreground_color) ||
      !color_extended_load(&element_style->background_color,
                           element_style_extended->background_color) ||
      !color_extended_load(&element_style->highlight_color,
                           element_style_extended->highlight_color) ||
      !border_extended_load(&element_style->border,
                            &element_style_extended->border) ||
      !directional_extended_load(&element_style->padding,
                                 &element_style_extended->padding) ||
      !directional_extended_load(&element_style->margin,
                                 &element_style_extended->margin);

  return !failed;
}

bool config_extended_load(struct config *config,
                          const struct config_extended *config_extended) {
  strncpy(config->font, config_extended->font, CONFIG_FIELD_MAX_LEN);
  config->font_size = config_extended->font_size;
  config->client_filter_behavior = config_extended->client_filter_behavior;
  bool failed =
      !element_style_extended_load(&config->peekaboo.style,
                                   &config_extended->peekaboo.style) ||
      !element_style_extended_load(&config->preview.style,
                                   &config_extended->preview.style) ||
      !element_style_extended_load(&config->preview_title.style,
                                   &config_extended->preview_title.style) ||
      !element_style_extended_load(&config->shortcut.style,
                                   &config_extended->shortcut.style);

  return !failed;
}

static const cyaml_config_t cyaml_config = {
    .log_fn = cyaml_log,
    .mem_fn = cyaml_mem,
    .log_level = CYAML_LOG_WARNING,
};

void get_config_path(char **config_path) {
  char *base_dir = getenv("XDG_CONFIG_HOME");
  char *ext = "";
  size_t len = strlen("/peekaboo/config") + 1;
  if (!base_dir) {
    base_dir = getenv("HOME");
    ext = "/.config";
    if (!base_dir) {
      log_error("Couldn't find XDG_CONFIG_HOME or HOME envvars\n");
      return;
    }
  }
  len += strlen(base_dir) + strlen(ext) + 5;
  *config_path = realloc(*config_path, len);
  snprintf(*config_path, len, "%s%s%s", base_dir, ext, "/peekaboo/config.yml");
}

bool config_load(struct config *config, char **config_path) {
  if (*config_path == NULL) {
    get_config_path(config_path);
    if (*config_path == NULL) {
      return false;
    }
  }

  struct config_extended *config_extended;

  cyaml_err_t err =
      cyaml_load_file(*config_path, &cyaml_config, &yaml_config_schema,
                      (cyaml_data_t **)&config_extended, NULL);
  if (err != CYAML_OK) {
    log_error("Failed to parse config: %s\n", cyaml_strerror(err));
    cyaml_free(&cyaml_config, &yaml_config_schema, &config_extended, 0);
    return false;
  }
  bool status = config_extended_load(config, config_extended);

  cyaml_free(&cyaml_config, &yaml_config_schema, config_extended, 0);

  return status;
}
