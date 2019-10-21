#ifndef LOOP_INTERNAL_H
#define LOOP_INTERNAL_H

#include <stdint.h>

#include "wuy_heap.h"
#include "wuy_pool.h"
#include "wuy_list.h"
#include "wuy_dict.h"
#include "wuy_event.h"

#include "loop.h"


#define LOOP_TYPE_TCP_LISTEN	101
#define LOOP_TYPE_STREAM	102
#define LOOP_TYPE_INOTIFY	103

/* idle */
typedef struct loop_idle_s loop_idle_t;
void loop_idle_run(loop_t *loop);

/* stream */
void loop_stream_init(loop_t *loop);
void loop_stream_event_handler(loop_stream_t *s, bool readable, bool writable);

/* tcp_listen */
void loop_tcp_listen_acceptable(loop_tcp_listen_t *tl);

/* timer */
typedef wuy_heap_t loop_timer_ctx_t;
void loop_timer_init(loop_t *loop);
int64_t loop_timer_expire(loop_timer_ctx_t *ctx);

/* inotify */
void loop_inotify_init(loop_t *loop);
void loop_inotify_event_handler(void *data);

/* loop_t */
struct loop_s {
	unsigned		quit:1;

	wuy_event_ctx_t		*event_ctx;

	wuy_pool_t		*stream_pool;
	wuy_list_t		stream_defer_head;

	wuy_pool_t		*timer_pool;
	loop_timer_ctx_t	*timer_ctx;

	int			inotify_fd;
	wuy_pool_t		*inotify_pool;
	wuy_dict_t		*wd_inotify;
	wuy_dict_t		*inside_inotify;
	wuy_list_t		inotify_defer_head;

	int			idle_count;
	int			idle_capacity;
	loop_idle_t		*idle_funcs;
};

#endif
