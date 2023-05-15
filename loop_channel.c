#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <stdlib.h>

#include "wuy_event.h"
#include "wuy_pool.h"

#include "loop_internal.h"
#include "loop.h"

struct loop_channel_s {
	int		type; /* keep this top! */

	loop_channel_on_receive_f *on_receive;

	int		notify_fd;

	bool		need_notify_in_idle;

	size_t		capacity;

	atomic_size_t	head;
	atomic_size_t	tail;

	void		*entries[0];
};


void loop_channel_poll_handler(loop_channel_t *ch)
{
	/* do not need to read() the eventfd */
	while (1) {
		size_t head = atomic_load_explicit(&ch->head, memory_order_relaxed);
		if (head == atomic_load_explicit(&ch->tail, memory_order_acquire)) {
			break;
		}
		ch->on_receive(ch->entries[head]);
		atomic_store_explicit(&ch->head, (head + 1) % ch->capacity, memory_order_release);
	}
}

bool loop_channel_send(loop_channel_t *ch, void *data)
{
	size_t tail = atomic_load_explicit(&ch->tail, memory_order_relaxed);
	size_t next_tail = (tail + 1) % ch->capacity;
	if (next_tail == atomic_load_explicit(&ch->head, memory_order_acquire)) {
		return false; // queue is full
	}

	ch->entries[tail] = data;
	atomic_store_explicit(&ch->tail, next_tail, memory_order_release);

	/* We try to avoid `write()` syscall here, so we check the queue
	 * before notifying. However, if the consumer pops the data bewteen
	 * the checking (tail==head) and the writing (ch->notify_fd), then
	 * the checking will never become true. We handle this case by
	 * notifing in loop's idle. */
	if (tail == atomic_load_explicit(&ch->head, memory_order_acquire)) {
		uint64_t val = 1;
		if (write(ch->notify_fd, &val, sizeof(val))) {}; // notify the consumer

		ch->need_notify_in_idle = false;
	} else {
		ch->need_notify_in_idle = true;
	}
	return true;
}

static void loop_channel_receive_in_idle(void *data)
{
	loop_channel_poll_handler(data);
}

loop_channel_t *loop_channel_new_receiver(loop_t *loop, size_t capacity,
		loop_channel_on_receive_f *on_receive)
{
	loop_channel_t *ch = malloc(sizeof(loop_channel_t) + sizeof(void *) * capacity);
	ch->type = LOOP_TYPE_CHANNEL;
	ch->capacity = capacity;
	ch->on_receive = on_receive;
	ch->need_notify_in_idle = false;
	atomic_init(&ch->head, 0);
	atomic_init(&ch->tail, 0);

	ch->notify_fd = eventfd(0, EFD_NONBLOCK);
	if (ch->notify_fd < 0) {
		return NULL;
	}
	wuy_event_status_t status = { 0, 0 };
	wuy_event_add_read(loop->event_ctx, ch->notify_fd, ch, &status);

	loop_idle_add(loop, loop_channel_receive_in_idle, ch);

	return ch;
}

size_t loop_channel_len(loop_channel_t *ch)
{
	size_t tail = atomic_load_explicit(&ch->tail, memory_order_relaxed);
	size_t head = atomic_load_explicit(&ch->head, memory_order_relaxed);
	size_t len = tail - head;
	return (len < 0) ? len + ch->capacity : len;
}

static void loop_channel_notify_in_idle(void *data)
{
	loop_channel_t *ch = data;
	if (ch->need_notify_in_idle) {
		uint64_t val = 1;
		if (write(ch->notify_fd, &val, sizeof(val))) {};
		ch->need_notify_in_idle = false;
	}
}

void loop_channel_add_sender(loop_t *loop, loop_channel_t *ch)
{
	loop_idle_add(loop, loop_channel_notify_in_idle, ch);
}
