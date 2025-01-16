#ifndef _VEC_H_
#define _VEC_H_
#include <stddef.h>

struct vec {
  size_t count;
  size_t _capacity;
  void** items;
};

struct vec *vec_init();
void vec_destroy(struct vec*);
void vec_append(struct vec*, void*);

#endif /* _VEC_H_ */
