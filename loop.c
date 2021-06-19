#include <assert.h>
#include <stdio.h>
#include <strings.h>
#include <stdlib.h>

#include "wuy_event.h"
#include "wuy_list.h"

#include "loop_internal.h"
#include "loop.h"


static void loop_event_handler(void *data, bool readable, bool writable)
{
	int type = *(int *)data;
	switch (type) {
	case LOOP_TYPE_TCP_LISTEN:
		loop_tcp_listen_acceptable(data);
		break;
	case LOOP_TYPE_STREAM:
		loop_stream_event_handler(data, readable, writable);
		break;
	default:
		abort();
	}
}

loop_t *loop_new_noev(void)
{
	loop_t *loop = calloc(1, sizeof(loop_t));
	assert(loop != NULL);
	loop_defer_init(loop);
	loop_timer_init(loop);
	loop_stream_init(loop);
	return loop;
}
void loop_new_event(loop_t *loop)
{
	loop->event_ctx = wuy_event_ctx_new(loop_event_handler);
}
loop_t *loop_new(void)
{
	loop_t *loop = loop_new_noev();
	loop_new_event(loop);
	return loop;
}

void loop_run(loop_t *loop)
{
	while (!loop->quit) {
		/* call loop_event_handler() to handle events */
		wuy_event_run(loop->event_ctx, loop_timer_next(loop->timer_ctx));

		/* call loop_timer_expire() before loop_defer_run(), because
		 * timers may make something that need the defers to cleanup. */
		loop_timer_expire(loop->timer_ctx);

		loop_defer_run(loop);
	}
}

void loop_kill(loop_t *loop)
{
	loop->quit = 1;
}
