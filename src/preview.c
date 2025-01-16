#include "preview.h"
#include "peekaboo.h"
#include "surface.h"
#include "wm_client/wm_client.h"
#include <cairo.h>
#include <glib.h>
#include <pango/pangocairo.h>
#include <wayland-util.h>
#include <xkbcommon/xkbcommon.h>

void cairo_set_source_u32(void *cairo, uint32_t color) {
  cairo_set_source_rgba((cairo_t *)cairo, (color >> 24 & 0xff) / 255.0,
                        (color >> 16 & 0xff) / 255.0,
                        (color >> 8 & 0xff) / 255.0, (color & 0xff) / 255.0);
}

void render_preview_not_ready(cairo_t *cr, PangoLayout *layout,
                              cairo_surface_t *base_surface,
                              struct wm_client *wm_client, double x, double y,
                              double width, double height, double border_width,
                              uint32_t background_color) {
  cairo_save(cr);

  // Draw the background
  cairo_rectangle(cr, x, y, width, height);
  cairo_set_source_u32(cr, background_color);
  cairo_fill(cr);

  // Set border line width
  cairo_set_line_width(cr, border_width);

  // Draw the border rectangle around the bounding box
  cairo_rectangle(cr, x, y, width, height);
  cairo_stroke(cr);

  cairo_restore(cr);
}

void render_preview_ready(cairo_t *cr, PangoLayout *layout,
                          cairo_surface_t *base_surface,
                          struct wm_client *wm_client, double x, double y,
                          double width, double height, double border_width,
                          uint32_t background_color) {
  // Save the current state of the cairo context
  cairo_save(cr);
  cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

  // TODO: Check the format is compatible
  // TODO: Maybe generate the surface earlier
  // But I'm not sure if we continuously receive the window's buffer in shared
  // memory. If we do, then we really should create the cairo surface every
  // render to be as up-to-date as possible.
  cairo_surface_t *preview_surface = cairo_image_surface_create_for_data(
      wm_client->buf, CAIRO_FORMAT_ARGB32, wm_client->width, wm_client->height,
      wm_client->stride);

  // Draw the border
  {
    cairo_save(cr);

    // Draw the background
    cairo_rectangle(cr, x, y, width, height);
    cairo_set_source_u32(cr, background_color);
    cairo_fill(cr);

    // Set border line width
    cairo_set_line_width(cr, border_width);

    // Draw the border rectangle around the bounding box
    cairo_rectangle(cr, x, y, width, height);
    cairo_stroke(cr);

    cairo_restore(cr);
  }

  // Set the base surface as the target for drawing
  cairo_set_source_surface(cr, base_surface, 0, 0);
  cairo_paint(cr);

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
  cairo_paint(cr);

  cairo_restore(cr);

  {
    cairo_save(cr);

    // Render the key shortcuts
    cairo_set_source_rgba(cr, 1.0, 0.0, 1.0, 1.0);
    cairo_move_to(cr, x, y);
    pango_layout_set_text(layout, wm_client->shortcut_keys, -1);
    pango_cairo_update_layout(cr, layout);
    pango_cairo_show_layout(cr, layout);

    cairo_restore(cr);
  }

  cairo_surface_destroy(preview_surface);
}

void render_preview(cairo_t *cr, PangoLayout *layout,
                    cairo_surface_t *base_surface, struct wm_client *wm_client,
                    double x, double y, double width, double height,
                    double border_width, uint32_t background_color) {
  if (wm_client->ready) {
    render_preview_ready(cr, layout, base_surface, wm_client, x, y, width,
                         height, border_width, background_color);
  } else {
    render_preview_not_ready(cr, layout, base_surface, wm_client, x, y, width,
                             height, border_width, background_color);
  }
}

void render(struct peekaboo *peekaboo, struct surface_buffer *surface_buffer) {
  surface_buffer->state = SURFACE_BUFFER_BUSY;
  cairo_t *cr = surface_buffer->cairo;

  cairo_save(cr);

  int scale_120 = 120;

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
                   surface_buffer->cairo_surface, wm_client, x, y,
                   peekaboo->config.preview_window_width,
                   peekaboo->config.preview_window_height,
                   peekaboo->config.preview_window_border_stroke_size,
                   peekaboo->config.preview_window_background_color);

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
        if (strncmp(peekaboo->input, wm_client->shortcut_keys,
                    peekaboo->input_size) == 0) {
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
