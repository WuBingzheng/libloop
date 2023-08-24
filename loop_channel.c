#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>

#include "wuy_event.h"
#include "wuy_pool.h"

#include "loop_internal.h"
#include "loop.h"

struct loop_channel_s {
	int		type; /* keep this top! */

	loop_channel_on_receive_f *on_receive;

	int		notify_fd;

	/* the lock protects not only the head and tail indexes,
	 * but also writing notify_fd to reduce syscall */
	pthread_mutex_t lock;
	size_t		capacity;
	size_t		head;
	size_t		tail;
	void		*entries[0];
};


loop_channel_t *loop_channel_new(loop_t *loop, size_t capacity,
		loop_channel_on_receive_f *on_receive)
{
	loop_channel_t *ch = malloc(sizeof(loop_channel_t) + sizeof(void *) * capacity);
	ch->type = LOOP_TYPE_CHANNEL;
	ch->on_receive = on_receive;
	pthread_mutex_init(&ch->lock, 0);
	ch->capacity = capacity;
	ch->head = 0;
	ch->tail = 0;

	ch->notify_fd = eventfd(0, EFD_NONBLOCK);
	if (ch->notify_fd < 0) {
		return NULL;
	}
	wuy_event_status_t status = { 0, 0 };
	wuy_event_add_read(loop->event_ctx, ch->notify_fd, ch, &status);

	return ch;
}

void loop_channel_poll_handler(loop_channel_t *ch)
{
	/* do not need to read() the eventfd */
	while (1) {
		pthread_mutex_lock(&ch->lock);

		if (ch->head == ch->tail) {
			pthread_mutex_unlock(&ch->lock);
			break;
		}
		void *data = ch->entries[ch->head];
		ch->entries[ch->head] = NULL;
		ch->head = (ch->head + 1) % ch->capacity;

		pthread_mutex_unlock(&ch->lock);

		ch->on_receive(data);
	}
}

bool loop_channel_send(loop_channel_t *ch, void *data)
{
	pthread_mutex_lock(&ch->lock);

	size_t tail = ch->tail;
	size_t next_tail = (tail + 1) % ch->capacity;
	if (next_tail == ch->head) {
		pthread_mutex_unlock(&ch->lock);
		return false; // queue is full
	}

	ch->entries[tail] = data;
	ch->tail = next_tail;

	/* notify the consumer only if the channel was empty */
	if (tail == ch->head) {
		uint64_t val = 1;
		if (write(ch->notify_fd, &val, sizeof(val))) {};
	}

	pthread_mutex_unlock(&ch->lock);
	return true;
}

size_t loop_channel_len(loop_channel_t *ch)
{
	size_t len = ch->tail - ch->head;
	return (len < 0) ? len + ch->capacity : len;
}
