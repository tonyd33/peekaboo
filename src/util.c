#include <stddef.h>
#include <stdint.h>
#include <time.h>
#include <cairo.h>

uint32_t gettime_ms() {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);

  uint32_t ms = t.tv_sec * 1000;
  ms += t.tv_nsec / 1000000;
  return ms;
}

void cairo_set_source_u32(void *cairo, uint32_t color) {
  cairo_set_source_rgba((cairo_t *)cairo, (color >> 24 & 0xff) / 255.0,
                        (color >> 16 & 0xff) / 255.0,
                        (color >> 8 & 0xff) / 255.0, (color & 0xff) / 255.0);
}
