#include <stddef.h>
#include <stdint.h>
#include <time.h>

uint32_t gettime_ms() {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);

  uint32_t ms = t.tv_sec * 1000;
  ms += t.tv_nsec / 1000000;
  return ms;
}
