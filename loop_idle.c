#include <stdlib.h>

#include "loop_internal.h"
#include "loop.h"

struct __loop_idle_s {
	loop_idle_f	*func;
	void		*data;
};

bool loop_idle_add(loop_t *loop, loop_idle_f *func, void *data)
{
	if (loop->idle_count == loop->idle_capacity) {
		int new_size = loop->idle_capacity ? loop->idle_capacity * 2 : 4;
		__loop_idle_t *new_array = realloc(loop->idle_funcs,
				sizeof(__loop_idle_t) * new_size);
		if (new_array == NULL) {
			return false;
		}

		loop->idle_funcs = new_array;
		loop->idle_capacity = new_size;
	}

	loop->idle_funcs[loop->idle_count].func = func;
	loop->idle_funcs[loop->idle_count].data = data;
	loop->idle_count++;
	return true;
}

void __loop_idle_run(loop_t *loop)
{
	int i;
	for (i = 0; i < loop->idle_count; i++) {
		__loop_idle_t *idle = &loop->idle_funcs[i];
		idle->func(idle->data);
	}
}
