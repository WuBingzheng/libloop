#include <sys/time.h>
#include <assert.h>

#include "wuy_container.h"

#include "loop_internal.h"
#include "loop.h"

struct loop_timer_s {
	int64_t			expire; /* in millisecond */
	wuy_heap_node_t		heap_node;
	loop_timer_f		*handler;
	loop_timer_ctx_t	*ctx;
	void			*data;
};

void loop_timer_init(loop_t *loop)
{
	loop->timer_pool = wuy_pool_new_type(loop_timer_t);

	loop->timer_ctx = wuy_heap_new_type(WUY_HEAP_KEY_INT64,
			offsetof(loop_timer_t, expire), false,
			offsetof(loop_timer_t, heap_node));
}

static int64_t loop_timer_now(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

loop_timer_t *loop_timer_new(loop_t *loop, loop_timer_f *handler, void *data)
{
	loop_timer_t *timer = wuy_pool_alloc(loop->timer_pool);
	if (timer == NULL) {
		return NULL;
	}
	timer->handler = handler;
	timer->ctx = loop->timer_ctx;
	timer->data = data;
	return timer;
}

bool loop_timer_set_at(loop_timer_t *timer, int64_t at)
{
	timer->expire = at;
	return wuy_heap_push_or_fix(timer->ctx, timer);
}

bool loop_timer_set_after(loop_timer_t *timer, int64_t after)
{
	return loop_timer_set_at(timer, after + loop_timer_now());
}

void loop_timer_delete(loop_timer_t *timer)
{
	if (timer == NULL) {
		return;
	}
	wuy_heap_delete(timer->ctx, timer);
	wuy_pool_free(timer);
}

int64_t loop_timer_expire(loop_timer_ctx_t *ctx)
{
	int64_t now = loop_timer_now();
	while (1) {
		loop_timer_t *timer = wuy_heap_min(ctx);
		if (timer == NULL) {
			return -1;
		}
		if (timer->expire > now) {
			return timer->expire - now;
		}
		wuy_heap_delete(ctx, timer);
		int64_t next = timer->handler(timer->expire, timer->data);
		if (next > 0) {
			timer->expire += next;
			wuy_heap_push(ctx, timer);
		} else if (next < 0) {
			loop_timer_delete(timer);
		}
	}
}
