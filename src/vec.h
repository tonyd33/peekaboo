#ifndef _VEC_H_
#define _VEC_H_
#include <stddef.h>
#include <stdint.h>

struct vec {
  size_t count;
  size_t elt_size;
  size_t _capacity;
  void **items;
};

struct vec *vec_init(size_t elt_size);
void *vec_get(struct vec *, uint32_t index);
void vec_destroy(struct vec *vec);
void vec_append(struct vec *, void *item);

#endif /* _VEC_H_ */
