#ifndef LOOP_INTERNAL_H
#define LOOP_INTERNAL_H

#include <stdint.h>
#include <stdlib.h>

#include "wuy_heap.h"
#include "wuy_list.h"
#include "wuy_dict.h"
#include "wuy_event.h"

#include "loop.h"


#define LOOP_TYPE_TCP_LISTEN	101
#define LOOP_TYPE_STREAM	102

/* defer */
typedef struct loop_defer_s loop_defer_t;
void loop_defer_run(loop_t *loop);

/* stream */
void loop_stream_init(loop_t *loop);
void loop_stream_event_handler(loop_stream_t *s, bool readable, bool writable);

/* tcp_listen */
void loop_tcp_listen_acceptable(loop_tcp_listen_t *tl);

/* timer */
typedef wuy_heap_t loop_timer_ctx_t;
void loop_timer_init(loop_t *loop);
int64_t loop_timer_expire(loop_timer_ctx_t *ctx);
int64_t loop_timer_next(loop_timer_ctx_t *ctx);

/* loop_t */
struct loop_s {
	unsigned		quit:1;

	wuy_event_ctx_t		*event_ctx;

	wuy_list_t		stream_defer_head;

	loop_timer_ctx_t	*timer_ctx;

	wuy_list_t		defer_head;
};

#endif
