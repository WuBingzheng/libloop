#include <assert.h>
#include <stdio.h>
#include <strings.h>
#include <stdlib.h>

#include "wuy_event.h"
#include "wuy_pool.h"
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
	case LOOP_TYPE_INOTIFY:
		loop_inotify_event_handler(data);
		break;
	default:
		abort();
	}
}

loop_t *loop_new(void)
{
	loop_t *loop = malloc(sizeof(loop_t));
	assert(loop != NULL);

	bzero(loop, sizeof(loop_t));

	loop->event_ctx = wuy_event_ctx_new(loop_event_handler);
	assert(loop->event_ctx != NULL);

	loop_timer_init(loop);

	loop_stream_init(loop);

	loop_inotify_init(loop);

	return loop;
}

void loop_run(loop_t *loop)
{
	int64_t timeout = -1;
	while (!loop->quit) {
		/* call loop_event_handler() to handle events */
		wuy_event_run(loop->event_ctx, timeout);

		/* idle functions */
		loop_idle_run(loop);

		/* expire and get the latest timeout */
		timeout = loop_timer_expire(loop->timer_ctx);
	}
}

int loop_kill(loop_t *loop)
{
	loop->quit = 1;
}
