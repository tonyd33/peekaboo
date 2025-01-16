#include "preview.h"
#include "cairo.h"
#include "src/peekaboo.h"
#include "src/surface.h"

void cairo_set_source_u32(void *cairo, uint32_t color) {
  cairo_set_source_rgba((cairo_t *)cairo, (color >> 24 & 0xff) / 255.0,
                        (color >> 16 & 0xff) / 255.0,
                        (color >> 8 & 0xff) / 255.0, (color & 0xff) / 255.0);
}

void render_preview_not_ready(cairo_t *cr, cairo_surface_t *base_surface,
                              struct preview *export_frame, double x,
                              double y, double width, double height,
                              double border_width, uint32_t background_color) {
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

void render_preview_ready(cairo_t *cr, cairo_surface_t *base_surface,
                          struct preview *preview, double x, double y,
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
      preview->buf, CAIRO_FORMAT_ARGB32, preview->width,
      preview->height, preview->stride);

  // Draw the border
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

  // Restore the context
  cairo_restore(cr);

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

  // Restore the original state of the cairo context
  cairo_restore(cr);


  cairo_surface_destroy(preview_surface);
}

void render_preview(cairo_t *cr, cairo_surface_t *base_surface,
                    struct preview *export_frame, double x, double y,
                    double width, double height, double border_width,
                    uint32_t background_color) {
  if (export_frame->ready) {
    render_preview_ready(cr, base_surface, export_frame, x, y, width, height,
                         border_width, background_color);
  } else {
    render_preview_not_ready(cr, base_surface, export_frame, x, y, width,
                             height, border_width, background_color);
  }
}

void render(struct peekaboo *peekaboo, struct surface_buffer *surface_buffer) {
  surface_buffer->state = SURFACE_BUFFER_BUSY;
  cairo_t *cr = surface_buffer->cairo;

  cairo_save(cr);

  int scale_120 = 120;

  int num_previews = wl_list_length(&peekaboo->previews);
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

  struct preview *export_frame;
  int i = 0;
  wl_list_for_each(export_frame, &peekaboo->previews, link) {
    int row = i / previews_per_row;
    int col = i % previews_per_row;

    int x = x_offset + col * (peekaboo->config.preview_window_width +
                              peekaboo->config.preview_window_margin_x);
    int y = y_offset + row * (peekaboo->config.preview_window_height +
                              peekaboo->config.preview_window_margin_y);

    render_preview(cr, surface_buffer->cairo_surface, export_frame, x, y,
                   peekaboo->config.preview_window_width,
                   peekaboo->config.preview_window_height,
                   peekaboo->config.preview_window_border_stroke_size,
                   peekaboo->config.preview_window_background_color);

    i++;
  }

  surface_buffer->state = SURFACE_BUFFER_READY;
  cairo_restore(cr);
}
