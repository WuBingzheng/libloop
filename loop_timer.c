#include <sys/time.h>
#include <assert.h>

#include "wuy_container.h"

#include "loop_internal.h"
#include "loop.h"

void loop_timer_init(loop_t *loop)
{
	loop->timer_ctx = wuy_heap_new_type(offsetof(loop_timer_t, heap_node),
			WUY_HEAP_KEY_INT64, offsetof(loop_timer_t, expire), false);
	assert(loop->timer_ctx != NULL);
}

static int64_t __now_ms(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1000 + tv.tv_usec / 1000000;
}

bool loop_timer_add(loop_t *loop, loop_timer_t *timer,
		int64_t timeout_ms, loop_timer_handler_f *handler)
{
	timer->expire = __now_ms() + timeout_ms;
	timer->handler = handler;
	return wuy_heap_push_or_fix(loop->timer_ctx, timer);
}

void loop_timer_delete(loop_t *loop, loop_timer_t *timer)
{
	wuy_heap_delete(loop->timer_ctx, timer);
}

int64_t loop_timer_expire(loop_timer_ctx_t *ctx)
{
	int64_t now_ms = __now_ms();
	while (1) {
		loop_timer_t *timer = wuy_heap_min(ctx);
		if (timer == NULL) {
			return -1;
		}
		if (timer->expire > now_ms) {
			return timer->expire - now_ms;
		}
		wuy_heap_delete(ctx, timer);
		timer->handler(timer);
	}
}
