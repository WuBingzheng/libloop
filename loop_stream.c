#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/sendfile.h>

#include "wuy_event.h"
#include "wuy_pool.h"
#include "wuy_list.h"

#include "loop_internal.h"
#include "loop.h"

struct loop_stream_s {
	int			type; /* keep this top! */

	int			fd;

	bool			closed;
	bool			write_blocked;

	wuy_event_status_t	event_status;

	loop_timer_t		timer_read;
	loop_timer_t		timer_write;

	void			*read_buffer;
	size_t			read_buf_len;

	wuy_list_head_t		list_node;

	loop_stream_ops_t	*ops;

	loop_t			*loop;
	void			*app_data;
};

static void loop_stream_close_for(loop_stream_t *s, const char *reason, int errnum);

/* timer utils */
static void loop_stream_read_timeout(loop_timer_t *timer)
{
	loop_stream_t *s = wuy_containerof(timer, loop_stream_t, timer_read);
	loop_stream_close_for(s, "read timedout", 0);
}
static void loop_stream_write_timeout(loop_timer_t *timer)
{
	loop_stream_t *s = wuy_containerof(timer, loop_stream_t, timer_write);
	loop_stream_close_for(s, "write timedout", 0);
}
static void loop_stream_set_timer_read(loop_stream_t *s)
{
	loop_timer_add(s->loop, &s->timer_read, s->ops->tmo_read,
			loop_stream_read_timeout);
}
static void loop_stream_set_timer_write(loop_stream_t *s)
{
	loop_timer_add(s->loop, &s->timer_write, s->ops->tmo_write,
			loop_stream_write_timeout);
}
static void loop_stream_del_timer_read(loop_stream_t *s)
{
	loop_timer_delete(s->loop, &s->timer_read);
}
static void loop_stream_del_timer_write(loop_stream_t *s)
{
	loop_timer_delete(s->loop, &s->timer_write);
}

/* event utils */
static void loop_stream_set_event_read(loop_stream_t *s)
{
	wuy_event_add_read(s->loop->event_ctx, s->fd, s, &s->event_status);
}
static void loop_stream_set_event_write(loop_stream_t *s)
{
	wuy_event_add_write(s->loop->event_ctx, s->fd, s, &s->event_status);
}
static void loop_stream_del_event_write(loop_stream_t *s)
{
	wuy_event_del_write(s->loop->event_ctx, s->fd, s, &s->event_status);
}


static void loop_stream_close_for(loop_stream_t *s, const char *reason, int errnum)
{
	if (s->closed) {
		return;
	}
	s->closed = true;

	printf("tcploop connection close: %s %s\n", reason, errnum?strerror(errnum):"");

	if (s->ops->on_close != NULL) {
		s->ops->on_close(s->app_data, reason, errnum);
	}

	close(s->fd);

	free(s->read_buffer);

	loop_stream_del_timer_read(s);
	loop_stream_del_timer_write(s);

	wuy_list_add(&s->list_node, &s->loop->stream_defer_head);
}

static void loop_stream_clear_defer(void *data)
{
	wuy_list_head_t *node, *head = data;
	wuy_list_iter_first(node, head) {
		wuy_list_delete(node);
		wuy_pool_free(wuy_containerof(node, loop_stream_t, list_node));
	}
}

static void loop_stream_readable(loop_stream_t *s)
{
	uint8_t buffer[s->ops->bufsz_read];

	/* read data into buffer */
	if (s->read_buffer != NULL) {
		memcpy(buffer, s->read_buffer, s->read_buf_len);
	}

	ssize_t read_len = read(s->fd, buffer + s->read_buf_len,
			s->ops->bufsz_read - s->read_buf_len);
	if (read_len < 0) {
		if (errno == EAGAIN) {
			loop_stream_set_event_read(s);
			loop_stream_set_timer_read(s);
			return;
		}
		loop_stream_close_for(s, "read error", errno);
		return;
	}
	if (read_len == 0) {
		loop_stream_close_for(s, "peer close", 0);
		return;
	}

	/* process data in buffer */
	read_len += s->read_buf_len;
	ssize_t proc_len = s->ops->on_read(s, buffer, read_len);
	if (proc_len < 0) {
		loop_stream_close_for(s, "app read error", 0);
		return;
	}
	if (proc_len == 0) {
		if (read_len == s->ops->bufsz_read) {
			loop_stream_close_for(s, "read buffer full", 0);
			return;
		}
	} else if (proc_len < read_len) {
		s->read_buf_len = read_len - proc_len;
		s->read_buffer = realloc(s->read_buffer, s->read_buf_len);
		memcpy(s->read_buffer, buffer + proc_len, s->read_buf_len);

	} else if (proc_len == read_len) {
		free(s->read_buffer);
		s->read_buffer = NULL;
		s->read_buf_len = 0;

	} else {
		abort();
	}

	/* tail recursive for read loop */
	loop_stream_readable(s);
}

void loop_stream_event_handler(loop_stream_t *s, bool readable, bool writable)
{
	if (s->closed) {
		return;
	}
	if (readable) {
		loop_stream_readable(s);
	}
	if (writable) {
		s->write_blocked = false;
		s->ops->on_writable(s);
	}
}

static ssize_t loop_stream_write_handle(loop_stream_t *s, size_t data_len, ssize_t write_len)
{
	if (write_len < 0 && errno != EAGAIN) {
		loop_stream_close_for(s, "write error", errno);
		return write_len;
	}
	if (write_len == data_len) {
		loop_stream_del_event_write(s);
		loop_stream_del_timer_write(s);
		return write_len;
	}

	/* set write-block */

	s->write_blocked = true;

	if (s->ops->on_writable == NULL) {
		loop_stream_close_for(s, "write blocks", errno);
		return -1;
	}
	loop_stream_set_event_write(s);
	loop_stream_set_timer_write(s);
	return write_len < 0 ? 0 : write_len;
}

ssize_t loop_stream_write(loop_stream_t *s, const void *data, size_t len)
{
	if (s->closed) {
		return -1;
	}
	if (s->write_blocked) {
		return 0;
	}
	ssize_t write_len = write(s->fd, data, len);
	return loop_stream_write_handle(s, len, write_len);
}

ssize_t loop_stream_sendfile(loop_stream_t *s, int in_fd, off_t *offset, size_t len)
{
	if (s->closed) {
		return -1;
	}
	if (s->write_blocked) {
		return 0;
	}
	ssize_t write_len = sendfile(s->fd, in_fd, offset, len);
	return loop_stream_write_handle(s, len, write_len);
}

loop_stream_t *loop_stream_add(loop_t *loop, int fd, loop_stream_ops_t *ops)
{
	loop_stream_t *s = wuy_pool_alloc(loop->stream_pool);
	if (s == NULL) {
		return NULL;
	}

	bzero(s, sizeof(loop_stream_t));
	s->type = LOOP_TYPE_STREAM;
	s->fd = fd;
	s->loop = loop;
	s->ops = ops;

	/* fix up ops */
	if (ops->bufsz_read == 0) {
		ops->bufsz_read = 1024 * 16;
	}

	/* read it to add event and timer */
	loop_stream_readable(s);

	return s;
}

void loop_stream_close(loop_stream_t *s)
{
	loop_stream_close_for(s, "app close", 0);
}

void loop_stream_init(loop_t *loop)
{
	loop->stream_pool = wuy_pool_new_type(loop_stream_t);

	wuy_list_init(&loop->stream_defer_head);

	loop_idle_add(loop, loop_stream_clear_defer, &loop->stream_defer_head);
}

int loop_stream_fd(loop_stream_t *s)
{
	return s->fd;
}
