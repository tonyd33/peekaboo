#include "vec.h"
#include <stdlib.h>

struct vec *vec_init() {
  struct vec *vec = malloc(sizeof(struct vec));
  vec->count = 0;
  vec->_capacity = 128;
  vec->items = calloc(sizeof(void *), 128);

  return vec;
}

void vec_destroy(struct vec *vec) {
  free(vec->items);
  vec->count = 0;
  vec->_capacity = 0;
  vec->items = NULL;
  free(vec);
}

void vec_append(struct vec *vec, void *item) {
  if (vec->count == vec->_capacity) {
    vec->_capacity *= 2;
    vec->items = realloc(vec->items, vec->_capacity);
  }
  vec->items[vec->count++] = item;
}
