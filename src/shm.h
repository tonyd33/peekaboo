#ifndef _SHM_H
#define _SHM_H

#include <stddef.h>

int shm_reallocate_file(int fd, size_t size);
int shm_allocate_file(size_t size);

#endif /* _SHM_H */
