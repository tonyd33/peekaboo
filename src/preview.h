#ifndef _PREVIEW_H_
#define _PREVIEW_H_

#include "peekaboo.h"
#include <cairo.h>

void render(struct peekaboo *peekaboo, struct surface_buffer *surface_buffer);

bool handle_key(struct peekaboo *peekaboo, uint32_t key, char character);

#endif /* _PREVIEW_H_ */
