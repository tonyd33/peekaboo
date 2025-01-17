#include "preview.h"
#include "log.h"
#include "pango/pango-types.h"
#include "peekaboo.h"
#include "surface.h"
#include "wm_client/wm_client.h"
#include <cairo.h>
#include <glib.h>
#include <pango/pangocairo.h>
#include <wayland-util.h>
#include <xkbcommon/xkbcommon-keysyms.h>
#include <xkbcommon/xkbcommon.h>

void cairo_set_source_u32(void *cairo, uint32_t color) {
  cairo_set_source_rgba((cairo_t *)cairo, (color >> 24 & 0xff) / 255.0,
                        (color >> 16 & 0xff) / 255.0,
                        (color >> 8 & 0xff) / 255.0, (color & 0xff) / 255.0);
}

void draw_rounded_rectangle(cairo_t *cr, cairo_surface_t *base_surface,
                            double x, double y, double width, double height,
                            double radius) {
  cairo_save(cr);

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

  cairo_fill(cr);

  cairo_restore(cr);
}

void render_cairo_surface(cairo_t *cr, cairo_surface_t *base_surface,
                          cairo_surface_t *preview_surface, double x, double y,
                          double width, double height) {
  cairo_save(cr);

  // Set the base surface as the target for drawing
  cairo_set_source_surface(cr, base_surface, 0, 0);

  // Get the original dimensions of the source surface
  double source_width = cairo_image_surface_get_width(preview_surface);
  double source_height = cairo_image_surface_get_height(preview_surface);

  // Calculate the scale factor while preserving the aspect ratio
  double scale_x = width / source_width;
  double scale_y = height / source_height;
  double scale =
      fmin(scale_x,
           scale_y); // Use the smaller scale factor to preserve aspect ratio

  // Calculate the offset to center the surface in the bounding box
  double offset_x = (width - scale * source_width) / 2.0;
  double offset_y = (height - scale * source_height) / 2.0;

  // Translate to the target position and apply the offset for centering
  cairo_translate(cr, x + offset_x, y + offset_y);

  // Apply the scaling
  cairo_scale(cr, scale, scale);

  // Paint the scaled and centered surface onto the target
  cairo_set_source_surface(cr, preview_surface, 0, 0);
  cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
  cairo_paint(cr);

  cairo_restore(cr);
}

void render_preview(cairo_t *cr, PangoLayout *layout,
                    cairo_surface_t *base_surface, struct config *config,
                    struct wm_client *wm_client, double x, double y,
                    double width, double height) {
  // Save the current state of the cairo context
  cairo_save(cr);

  // Draw the background
  {
    cairo_save(cr);
    cairo_set_source_u32(cr, config->preview_window_background_color);
    draw_rounded_rectangle(cr, base_surface, x, y, width, height,
                           config->preview_window_border_radius);
    cairo_restore(cr);
  }

  // We should apply padding to everything past here
  x += config->preview_window_padding_x;
  width -= config->preview_window_padding_x * 2;
  y += config->preview_window_padding_y;
  height -= config->preview_window_padding_y * 2;

  if (wm_client->ready) {
    // TODO: Check the format is compatible
    // TODO: Maybe generate the surface earlier
    // TODO: Account for padding (we're adding extra padding to the aspect that
    // was scaled down already still)
    cairo_surface_t *preview_surface = cairo_image_surface_create_for_data(
        wm_client->buf, CAIRO_FORMAT_ARGB32, wm_client->width,
        wm_client->height, wm_client->stride);
    render_cairo_surface(cr, base_surface, preview_surface, x, y, width,
                         height);
    cairo_surface_destroy(preview_surface);
  }

  // Render the key shortcuts
  {
    cairo_save(cr);

    pango_layout_set_text(layout, wm_client->shortcut_keys, -1);

    int text_width, text_height;
    pango_layout_get_size(layout, &text_width, &text_height);
    text_width /= PANGO_SCALE;
    text_height /= PANGO_SCALE;

    // Draw background for key shortcuts
    cairo_set_source_u32(cr, config->preview_window_title_background_color);
    draw_rounded_rectangle(cr, base_surface, x, y,
                           text_width + config->shortcut_padding_x * 2, text_height,
                           5);

    cairo_move_to(cr, x + config->shortcut_padding_x, y);
    cairo_set_source_u32(cr, config->shortcut_foreground_color);
    pango_cairo_update_layout(cr, layout);
    pango_cairo_show_layout(cr, layout);

    cairo_restore(cr);
  }

  // Render the title
  {
    cairo_save(cr);

    // Measure the total text width for centering
    pango_layout_set_text(layout, wm_client->title, -1);
    // pango_layout_get_pixel_extents(layout, &ink_rect, &logical_rect);

    pango_layout_set_width(layout, width * PANGO_SCALE);
    int text_width, text_height;
    pango_layout_get_size(layout, &text_width, &text_height);
    text_width /= PANGO_SCALE;
    text_height /= PANGO_SCALE;

    double text_x = x + (width - text_width) / 2.0; // Center horizontally
    double text_y = y + (height - text_height);     // Bottom vertically

    // Draw background for title
    cairo_set_source_u32(cr, config->preview_window_title_background_color);
    draw_rounded_rectangle(cr, base_surface, text_x, text_y, text_width,
                           text_height, 5);

    // Draw highlighted part
    cairo_move_to(cr, text_x, text_y);
    pango_cairo_update_layout(cr, layout);

    cairo_set_source_u32(cr, config->preview_window_title_foreground_color);
    pango_cairo_show_layout(cr, layout);

    cairo_restore(cr);
  }
}

void render(struct peekaboo *peekaboo, struct surface_buffer *surface_buffer) {
  surface_buffer->state = SURFACE_BUFFER_BUSY;
  cairo_t *cr = surface_buffer->cairo;

  // Clear the screen
  {
    cairo_save(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.0);
    cairo_rectangle(cr, 0, 0, surface_buffer->width, surface_buffer->height);
    cairo_fill(cr);
    cairo_restore(cr);
  }

  cairo_save(cr);

  int num_previews = wl_list_length(&peekaboo->wm_clients);
  int previews_per_row =
      (peekaboo->surface_width + peekaboo->config.preview_window_margin_x) /
      (peekaboo->config.preview_window_width +
       peekaboo->config.preview_window_margin_x);
  int row_count = (num_previews + previews_per_row - 1) / previews_per_row;

  int total_previews_width =
      previews_per_row * (peekaboo->config.preview_window_width +
                          peekaboo->config.preview_window_margin_x) -
      peekaboo->config.preview_window_margin_x;
  int total_previews_height =
      row_count * (peekaboo->config.preview_window_height +
                   peekaboo->config.preview_window_margin_y) -
      peekaboo->config.preview_window_margin_y;

  int x_offset = (peekaboo->surface_width - total_previews_width) / 2;
  int y_offset = (peekaboo->surface_height - total_previews_height) / 2;

  struct wm_client *wm_client;
  int i = 0;
  wl_list_for_each(wm_client, &peekaboo->wm_clients, link) {
    int row = i / previews_per_row;
    int col = i % previews_per_row;

    int x = x_offset + col * (peekaboo->config.preview_window_width +
                              peekaboo->config.preview_window_margin_x);
    int y = y_offset + row * (peekaboo->config.preview_window_height +
                              peekaboo->config.preview_window_margin_y);

    render_preview(cr, surface_buffer->pango_layout,
                   surface_buffer->cairo_surface, &peekaboo->config, wm_client,
                   x, y, peekaboo->config.preview_window_width,
                   peekaboo->config.preview_window_height);

    i++;
  }

  surface_buffer->state = SURFACE_BUFFER_READY;

  cairo_restore(cr);
}

bool handle_key(struct peekaboo *peekaboo, xkb_keysym_t keysym, char ch) {

  switch (keysym) {
  case XKB_KEY_Escape:
    peekaboo->running = false;
    return false;
  default:
    if (g_unichar_isprint(ch)) {
      peekaboo->input[peekaboo->input_size++] = ch;
      // Check if we're done
      struct wm_client *wm_client;
      wl_list_for_each(wm_client, &peekaboo->wm_clients, link) {
        if (strcmp(peekaboo->input, wm_client->shortcut_keys) == 0) {
          peekaboo->selected_client = wm_client;
          peekaboo->running = false;
          return true;
        }
      }

      return true;
    }
    return false;
  }
}
