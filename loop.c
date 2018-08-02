#include <assert.h>
#include <stdio.h>
#include <strings.h>
#include <stdlib.h>

#include "wuy_event.h"
#include "wuy_timer.h"
#include "wuy_pool.h"
#include "wuy_list.h"

#include "loop_internal.h"
#include "loop.h"


static void __loop_event_handler(void *data, bool readable, bool writable)
{
	int type = *(int *)data;
	switch (type) {
	case LOOP_TYPE_TCP_LISTEN:
		__loop_tcp_listen_acceptable(data);
		break;
	case LOOP_TYPE_STREAM:
		__loop_stream_event_handler(data, readable, writable);
		break;
//	case LOOP_TYPE_INOTIFY:
//		__loop_inotify_event_handler(data);
//		break;
	default:
		abort();
	}
}

loop_t *loop_new(void)
{
	loop_t *loop = malloc(sizeof(loop_t));
	assert(loop != NULL);

	bzero(loop, sizeof(loop_t));

	loop->event_ctx = wuy_event_ctx_new(__loop_event_handler);
	assert(loop->event_ctx != NULL);

	__loop_timer_init(loop);

	__loop_stream_init(loop);

	// __loop_inotify_init(loop);

	return loop;
}

int loop_run(loop_t *loop)
{
	int64_t timeout = -1;
	while (!loop->quit) {
		/* call __loop_event_handler() to handle events */
		wuy_event_run(loop->event_ctx, timeout);

		/* expire and get the latest timeout */
		timeout = __loop_timer_expire(loop->timer_ctx);

		/* idle functions */
		__loop_idle_run(loop);
	}
	return 0;
}

int loop_kill(loop_t *loop)
{
	loop->quit = 1;
}
