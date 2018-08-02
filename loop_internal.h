#ifndef LOOP_INTERNAL_H
#define LOOP_INTERNAL_H

#include <stdint.h>

#include "wuy_heap.h"
#include "wuy_pool.h"
#include "wuy_list.h"
#include "wuy_event.h"

#include "loop.h"


#define LOOP_TYPE_TCP_LISTEN	101
#define LOOP_TYPE_STREAM	102
#define LOOP_TYPE_INOTIFY	103

/* idle */
typedef struct __loop_idle_s __loop_idle_t;
void __loop_idle_run(loop_t *loop);

/* stream */
void __loop_stream_init(loop_t *loop);
void __loop_stream_event_handler(loop_stream_t *s, bool readable, bool writable);

/* tcp_listen */
void __loop_tcp_listen_acceptable(loop_tcp_listen_t *tl);

/* timer */
typedef wuy_heap_t __loop_timer_ctx_t;
void __loop_timer_init(loop_t *loop);
int64_t __loop_timer_expire(__loop_timer_ctx_t *ctx);

/* loop_t */
struct loop_s {
	unsigned		quit:1;

	wuy_event_ctx_t		*event_ctx;

	wuy_pool_t		*stream_pool;
	wuy_list_head_t		stream_defer_head;

	__loop_timer_ctx_t	*timer_ctx;

	int			idle_count;
	int			idle_capacity;
	__loop_idle_t		*idle_funcs;
};

#endif
