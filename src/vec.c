#include "vec.h"
#include "log.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct vec *vec_init(size_t elt_size) {
  struct vec *vec = malloc(sizeof(struct vec));
  vec->count = 0;
  vec->elt_size = elt_size;
  vec->_capacity = 128;
  vec->items = calloc(128, elt_size);

  return vec;
}

void *vec_get(struct vec *vec, uint32_t index) {
  if (0 <= index && index < vec->count) {
    return (void *)(((uintptr_t)vec->items) + (vec->elt_size * index));
  }
  return NULL;
}

void vec_destroy(struct vec *vec) {
  memset(vec->items, 0, vec->elt_size * vec->_capacity);
  free(vec->items);
  memset(vec, 0, sizeof(struct vec));
  free(vec);
}

void vec_append(struct vec *vec, void *item) {
  if (vec->count == vec->_capacity) {
    vec->_capacity *= 2;
    vec->items = reallocarray(vec->items, vec->_capacity, vec->elt_size);
  }
  uintptr_t p_new = ((uintptr_t)vec->items) + (vec->elt_size * vec->count++);
  memcpy((void *)p_new, item, vec->elt_size);
}
