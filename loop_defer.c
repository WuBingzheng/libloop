#include <stdlib.h>

#include "loop_internal.h"
#include "loop.h"

struct loop_defer_s {
	loop_defer_f	*func;
	void		*data;
};

bool loop_defer_add(loop_t *loop, loop_defer_f *func, void *data)
{
	if (loop->defer_count == loop->defer_capacity) {
		int new_size = loop->defer_capacity ? loop->defer_capacity * 2 : 4;
		loop_defer_t *new_array = realloc(loop->defer_funcs,
				sizeof(loop_defer_t) * new_size);
		if (new_array == NULL) {
			return false;
		}

		loop->defer_funcs = new_array;
		loop->defer_capacity = new_size;
	}

	loop->defer_funcs[loop->defer_count].func = func;
	loop->defer_funcs[loop->defer_count].data = data;
	loop->defer_count++;
	return true;
}

void loop_defer_run(loop_t *loop)
{
	int i;
	for (i = 0; i < loop->defer_count; i++) {
		loop_defer_t *defer = &loop->defer_funcs[i];
		defer->func(defer->data);
	}
}
