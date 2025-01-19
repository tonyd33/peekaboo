#include "preview.h"
#include "layout.h"
#include "log.h"
#include "pango/pango-layout.h"
#include "pango/pango-types.h"
#include "peekaboo.h"
#include "styles.h"
#include "surface.h"
#include "util.h"
#include "vec.h"
#include "wm_client/wm_client.h"
#include <cairo.h>
#include <glib.h>
#include <pango/pangocairo.h>
#include <unistd.h>
#include <wayland-util.h>
#include <xkbcommon/xkbcommon-keysyms.h>
#include <xkbcommon/xkbcommon.h>

void cairo_set_source_u32(void *cairo, uint32_t color) {
  cairo_set_source_rgba((cairo_t *)cairo, (color >> 24 & 0xff) / 255.0,
                        (color >> 16 & 0xff) / 255.0,
                        (color >> 8 & 0xff) / 255.0, (color & 0xff) / 255.0);
}

void draw_rounded_rectangle(cairo_t *cr, cairo_surface_t *base_surface,
                            struct element_style *element_style, double x,
                            double y, double width, double height) {
  cairo_save(cr);

  double radius = element_style->border.radius;

  double degrees = M_PI / 180.0;

  cairo_new_sub_path(cr);
  cairo_arc(cr, x + width - radius, y + radius, radius, -90 * degrees,
            0 * degrees);
  cairo_arc(cr, x + width - radius, y + height - radius, radius, 0 * degrees,
            90 * degrees);
  cairo_arc(cr, x + radius, y + height - radius, radius, 90 * degrees,
            180 * degrees);
  cairo_arc(cr, x + radius, y + radius, radius, 180 * degrees, 270 * degrees);
  cairo_close_path(cr);

  cairo_set_source_u32(cr, element_style->background_color);
  cairo_fill_preserve(cr);

  cairo_set_line_width(cr, element_style->border.width);
  cairo_set_source_u32(cr, element_style->border.color);
  cairo_stroke(cr);

  cairo_restore(cr);
}

void render_wm_client_preview_surface(cairo_t *cr, struct wm_client *wm_client,
                                      double x, double y, double width,
                                      double height) {
  cairo_save(cr);

  // Preserve aspect ratio when scaling
  double scale_x = (double)width / wm_client->width;
  double scale_y = (double)height / wm_client->height;
  double scale = fmin(scale_x, scale_y);

  cairo_surface_t *scaled_surface = surface_cache_get_scaled(
      wm_client->surface_cache, wm_client->width * scale,
      wm_client->height * scale);

  // Calculate the offset to center the surface in the bounding box
  double offset_x = (width - scale * wm_client->width) / 2.0;
  double offset_y = (height - scale * wm_client->height) / 2.0;
  cairo_set_source_surface(cr, scaled_surface, x + offset_x, y + offset_y);
  cairo_paint(cr);

  cairo_restore(cr);
}

void render_text_themed(cairo_t *cr, PangoLayout *layout,
                        cairo_surface_t *base_surface, char *text,
                        struct element_style *theme, PangoRectangle *ink_rect,
                        PangoRectangle *logical_rect) {
  cairo_save(cr);

  double x, y;
  cairo_get_current_point(cr, &x, &y);

  pango_layout_set_text(layout, text, -1);
  pango_layout_get_pixel_extents(layout, ink_rect, logical_rect);

  // Draw background
  draw_rounded_rectangle(
      cr, base_surface, theme, x, y,
      logical_rect->width + theme->padding.left + theme->padding.right,
      logical_rect->height + theme->padding.top + theme->padding.bottom);

  // Draw text
  cairo_move_to(cr, x + theme->padding.left, y + theme->padding.top);
  cairo_set_source_u32(cr, theme->foreground_color);
  pango_cairo_update_layout(cr, layout);
  pango_cairo_show_layout(cr, layout);

  cairo_restore(cr);
}

void render_highlighted_text_themed(cairo_t *cr, PangoLayout *layout,
                                    cairo_surface_t *base_surface, char *text,
                                    uint32_t hl_len,
                                    struct element_style *theme,
                                    PangoRectangle *ink_rect,
                                    PangoRectangle *logical_rect) {
  cairo_save(cr);

  double x, y;
  cairo_get_current_point(cr, &x, &y);

  // Measure total text
  pango_layout_set_text(layout, text, -1);
  pango_layout_get_pixel_extents(layout, ink_rect, logical_rect);

  // Draw background
  draw_rounded_rectangle(
      cr, base_surface, theme, x, y,
      logical_rect->width + theme->padding.left + theme->padding.right,
      logical_rect->height + theme->padding.top + theme->padding.bottom);

  // Measure highlighted text
  PangoRectangle ink_rect_tmp;
  PangoRectangle logical_rect_tmp;
  pango_layout_set_text(layout, text, hl_len);
  pango_layout_get_pixel_extents(layout, &ink_rect_tmp, &logical_rect_tmp);

  // Draw highlighted text
  cairo_move_to(cr, x + theme->padding.left, y + theme->padding.top);
  cairo_set_source_u32(cr, theme->highlight_color);
  pango_cairo_update_layout(cr, layout);
  pango_cairo_show_layout(cr, layout);

  // Draw non-highlighted text
  pango_layout_set_text(layout, &text[hl_len], -1);
  cairo_rel_move_to(cr, logical_rect_tmp.width, 0);
  cairo_set_source_u32(cr, theme->foreground_color);
  pango_cairo_update_layout(cr, layout);
  pango_cairo_show_layout(cr, layout);

  cairo_restore(cr);
}

int count_matching_prefix(const char *str1, const char *str2) {
  int count = 0;
  while (*str1 != '\0' && *str2 != '\0' && *str1 == *str2) {
    count++;
    str1++;
    str2++;
  }

  return count;
}

void render_preview(cairo_t *cr, PangoLayout *layout,
                    cairo_surface_t *base_surface, struct config *config,
                    struct wm_client *wm_client, double x, double y,
                    double width, double height) {
#ifdef DEBUG_RENDERS
  uint32_t start_time_ms = gettime_ms();
#endif /* DEBUG_RENDERS */
  cairo_save(cr);

  // Draw the background
  {
    cairo_save(cr);
    draw_rounded_rectangle(cr, base_surface, &config->preview.style, x, y,
                           width, height);
    cairo_restore(cr);
  }

  // We should apply padding to (almost) everything past here
  double padded_x = x + config->preview.style.padding.left;
  double padded_width = width - (config->preview.style.padding.left +
                                 config->preview.style.padding.right);
  double padded_y = y + config->preview.style.padding.top;
  double padded_height = height - (config->preview.style.padding.top +
                                   config->preview.style.padding.bottom);

  if (wm_client->ready) {
    render_wm_client_preview_surface(cr, wm_client, padded_x, padded_y,
                                     padded_width, padded_height);
  }

  // Render the key shortcuts
  {
    PangoRectangle ink_rect;
    PangoRectangle logical_rect;
    cairo_move_to(cr, padded_x + config->shortcut.style.margin.left,
                  padded_y + config->shortcut.style.margin.top);
    render_highlighted_text_themed(
        cr, layout, base_surface, wm_client->shortcut_keys,
        wm_client->shortcut_keys_highlight_len, &config->shortcut.style,
        &ink_rect, &logical_rect);
  }

  // Render the title
  {
    cairo_save(cr);

    // Measure the total text width for centering
    pango_layout_set_text(layout, wm_client->title, -1);
    PangoRectangle ink_rect;
    PangoRectangle logical_rect;
    pango_layout_set_width(layout, padded_width * PANGO_SCALE);
    pango_layout_get_pixel_extents(layout, &ink_rect, &logical_rect);

    // Center horizontally
    double text_x = padded_x + ((padded_width - logical_rect.width) / 2.0);
    // Bottom vertically
    double text_y = padded_y + (padded_height - logical_rect.height) -
                    config->preview_title.style.margin.bottom;

    // Draw background for title
    cairo_set_source_u32(cr, config->preview_title.style.background_color);
    draw_rounded_rectangle(
        cr, base_surface, &config->preview_title.style, text_x, text_y,
        logical_rect.width + config->preview_title.style.padding.left +
            config->preview_title.style.padding.right,
        logical_rect.height + config->preview_title.style.padding.top +
            config->preview_title.style.padding.bottom);

    // Draw text
    cairo_move_to(cr, text_x + config->preview_title.style.padding.left,
                  text_y + config->preview_title.style.padding.top);
    pango_cairo_update_layout(cr, layout);

    cairo_set_source_u32(cr, config->preview_title.style.foreground_color);
    pango_cairo_show_layout(cr, layout);

    cairo_restore(cr);
  }

  if (wm_client->dim) {
    cairo_save(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    /* 50% transparent black */
    struct element_style style = {
        .background_color = 0x7f,
        .border = config->preview.style.border,
    };
    draw_rounded_rectangle(cr, base_surface, &style, x, y, width, height);
    cairo_restore(cr);
  }

  cairo_restore(cr);
#ifdef DEBUG_RENDERS
  uint32_t end_time_ms = gettime_ms();
  log_debug("Rendering one preview for %s took %u ms\n", wm_client->title,
            end_time_ms - start_time_ms);
#endif /* DEBUG_RENDERS */
}

void render(struct peekaboo *peekaboo, struct surface_buffer *surface_buffer) {
  surface_buffer->state = SURFACE_BUFFER_BUSY;
#ifdef DEBUG_RENDERS
  uint32_t start_time_ms = gettime_ms();
#endif /* DEBUG_RENDERS */

  cairo_t *cr = surface_buffer->cairo;
  struct config config = peekaboo->config;

  cairo_save(cr);

  size_t num_previews_to_show = 0;
  {
    struct wm_client *wm_client;
    wl_list_for_each(wm_client, &peekaboo->wm_clients, link) {
      if (!wm_client->hide) {
        num_previews_to_show++;
      }
    }
  }
  struct layout *layout = calculate_layout_fixed_individual_aspect_ratio(
      num_previews_to_show,
      surface_buffer->width - (config.peekaboo.style.padding.left +
                               config.peekaboo.style.padding.right),
      surface_buffer->height - (config.peekaboo.style.padding.top +
                                config.peekaboo.style.padding.bottom));

  double margin_x_size =
      config.preview.style.margin.left + config.preview.style.margin.right;

  double margin_y_size =
      config.preview.style.margin.top + config.preview.style.margin.bottom;

  double offset_x = margin_x_size + config.peekaboo.style.padding.left;
  double offset_y = margin_y_size + config.peekaboo.style.padding.top;

  // Clear the screen
  {
    cairo_save(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.0);
    cairo_rectangle(cr, config.peekaboo.style.padding.left,
                    config.peekaboo.style.padding.top,
                    surface_buffer->width - config.peekaboo.style.padding.right,
                    surface_buffer->height -
                        config.peekaboo.style.padding.bottom);
    cairo_fill(cr);
    cairo_restore(cr);
  }

  // Render the clients
  {
    struct wm_client *wm_client;
    uint32_t i = 0;
    wl_list_for_each(wm_client, &peekaboo->wm_clients, link) {
      if (!wm_client->hide) {
        struct preview_geometry *preview_geometry =
            vec_get(layout->preview_geometries, i);

        double x = preview_geometry->x + offset_x;
        double y = preview_geometry->y + offset_y;

        double width = preview_geometry->width - margin_x_size;
        double height = preview_geometry->height - margin_y_size;

        render_preview(cr, surface_buffer->pango_layout,
                       surface_buffer->cairo_surface, &peekaboo->config,
                       wm_client, x, y, width, height);

        i++;
      }
    }
  }

  layout_destroy(layout);
  cairo_restore(cr);

#ifdef DEBUG_RENDERS
  uint32_t end_time_ms = gettime_ms();
  log_debug("Full render took %u ms\n", end_time_ms - start_time_ms);
#endif /* DEBUG_RENDERS */
  surface_buffer->state = SURFACE_BUFFER_READY;
}

bool recalculate_clients(struct peekaboo *peekaboo) {
  struct wm_client *wm_client;
  bool changed = false;

  wl_list_for_each(wm_client, &peekaboo->wm_clients, link) {
    if (strcmp(peekaboo->input, wm_client->shortcut_keys) == 0) {
      peekaboo->selected_client = wm_client;
      peekaboo->running = false;
    }
    uint32_t matched_prefix_count =
        count_matching_prefix(wm_client->shortcut_keys, peekaboo->input);
    // If we match only 2 chars but the input is 3 chars now, then we should
    // not show any highlight at all.
    uint32_t new_shortcut_highlight_keys_len =
        matched_prefix_count == peekaboo->input_size ? matched_prefix_count : 0;

    bool new_hide;
    bool new_dim;
    switch (peekaboo->config.client_filter_behavior) {
    case CLIENT_FILTER_BEHAVIOR_HIDE:
      new_hide = peekaboo->input_size != 0 && matched_prefix_count == 0;
      new_dim = false;
      break;
    case CLIENT_FILTER_BEHAVIOR_DIM:
      new_hide = false;
      new_dim = peekaboo->input_size != 0 && matched_prefix_count == 0;
      break;
    case CLIENT_FILTER_BEHAVIOR_NONE:
    default:
      new_hide = false;
      new_dim = false;
      break;
    }

    changed = changed ||
              new_shortcut_highlight_keys_len !=
                  wm_client->shortcut_keys_highlight_len ||
              new_hide != wm_client->hide || new_dim != wm_client->dim;

    wm_client->shortcut_keys_highlight_len = matched_prefix_count;
    wm_client->hide = new_hide;
    wm_client->dim = new_dim;
  }

  return changed;
}

bool handle_key(struct peekaboo *peekaboo, xkb_keysym_t keysym, char ch) {

  switch (keysym) {
  case XKB_KEY_Escape:
  case XKB_KEY_q:
    peekaboo->running = false;
    return false;
  case XKB_KEY_BackSpace:
    if (peekaboo->input_size == 0)
      return false;

    peekaboo->input[--peekaboo->input_size] = '\0';
    return recalculate_clients(peekaboo);
  default:
    if (g_unichar_isprint(ch)) {
      peekaboo->input[peekaboo->input_size++] = ch;
      return recalculate_clients(peekaboo);
    }
    return false;
  }
}
