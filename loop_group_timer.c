#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <assert.h>

#include "loop.h"
#include "wuy_list.h"

struct loop_group_timer_head_s {
	int64_t		period;
	wuy_list_t	list_head;
	loop_timer_f	*handler;
	loop_timer_t	*timer;
};

struct loop_group_timer_s {
	int64_t		expire;
	void		*data;
	wuy_list_node_t	list_node;
};

static int64_t loop_group_timer_now(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static void loop_group_timer_handler(loop_group_timer_head_t *group,
		loop_group_timer_t *timer, int64_t at)
{
	timer->expire = 0;
	wuy_list_delete(&timer->list_node);

	int64_t ret = group->handler(at, timer->data);

	/* handle the return value in the same way with loop_timer_expire() */
	if (ret > 0) {
		assert(ret == group->period);
		timer->expire = at + group->period;
		wuy_list_append(&group->list_head, &timer->list_node);
	} else if (ret < 0) {
		free(timer);
	}

}
static int64_t loop_group_timer_head_handler(int64_t at, void *data)
{
	loop_group_timer_head_t *group = data;

	loop_group_timer_t *timer, *safe;
	wuy_list_iter_safe_type(&group->list_head, timer, safe, list_node) {
		if (timer->expire > at) {
			loop_timer_set_at(group->timer, timer->expire);
			break;
		}

		loop_group_timer_handler(group, timer, at);
	}
	return 0;
}

bool loop_group_timer_expire_one_at(loop_group_timer_head_t *group, int64_t at)
{
	loop_group_timer_t *timer;
	wuy_list_first_type(&group->list_head, timer, list_node);
	if (timer == NULL) {
		return false;
	}
	if (timer->expire >= at) {
		return false;
	}

	loop_group_timer_handler(group, timer, at);
	return true;
}
bool loop_group_timer_expire_one_ahead(loop_group_timer_head_t *group, int64_t ahead)
{
	return loop_group_timer_expire_one_at(group, loop_group_timer_now() + ahead);
}

loop_group_timer_head_t *loop_group_timer_head_new(loop_t *loop,
		loop_timer_f *handler, int64_t period)
{
	loop_group_timer_head_t *group = malloc(sizeof(loop_group_timer_head_t));
	wuy_list_init(&group->list_head);
	group->handler = handler;
	group->period = period;
	group->timer = loop_timer_new(loop, loop_group_timer_head_handler, group);
	return group;
}

loop_group_timer_t *loop_group_timer_new(void *data)
{
	loop_group_timer_t *timer = malloc(sizeof(loop_group_timer_t));
	timer->expire = 0;
	timer->data = data;
	return timer;
}

void loop_group_timer_set(loop_group_timer_head_t *group, loop_group_timer_t *timer)
{
	if (timer->expire != 0) {
		wuy_list_delete(&timer->list_node);
	}

	bool was_empty = wuy_list_empty(&group->list_head);

	timer->expire = loop_group_timer_now() + group->period;
	wuy_list_append(&group->list_head, &timer->list_node);

	if (was_empty) {
		loop_timer_set_after(group->timer, group->period);
	}
}
void loop_group_timer_suspend(loop_group_timer_t *timer)
{
	if (timer->expire == 0) {
		return;
	}

	timer->expire = 0;
	wuy_list_delete(&timer->list_node);
}
void loop_group_timer_delete(loop_group_timer_t *timer)
{
	loop_group_timer_suspend(timer);
	free(timer);
}
