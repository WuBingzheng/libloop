#include <stdlib.h>

#include "loop_internal.h"
#include "loop.h"

struct loop_defer_s {
	wuy_list_node_t	list_node;
	loop_defer_f	*func;
	void		*data;
	float		rank;
};

void loop_defer_init(loop_t *loop)
{
	wuy_list_init(&loop->defer_head);
}

bool loop_defer_add4(loop_t *loop, loop_defer_f *func, void *data, float rank)
{
	loop_defer_t *defer = malloc(sizeof(loop_defer_t));
	if (defer == NULL) {
		return false;
	}

	defer->func = func;
	defer->data = data;
	defer->rank = rank;

	loop_defer_t *i;
	wuy_list_iter_reverse_type(&loop->defer_head, i, list_node) {
		if (i->rank <= rank) {
			wuy_list_add_after(&i->list_node, &defer->list_node);
			return true;
		}

	}

	wuy_list_insert(&loop->defer_head, &defer->list_node);
	return true;
}

void loop_defer_run(loop_t *loop)
{
	loop_defer_t *defer;
	wuy_list_iter_type(&loop->defer_head, defer, list_node) {
		defer->func(defer->data);
	}
}
