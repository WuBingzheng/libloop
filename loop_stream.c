#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/sendfile.h>
#include <openssl/ssl.h>

#include "wuy_event.h"
#include "wuy_pool.h"
#include "wuy_list.h"

#include "loop_internal.h"
#include "loop.h"

struct loop_stream_s {
	int			type; /* keep this top! */

	int			fd;
	SSL			*ssl;

	bool			closed;
	bool			write_blocked;

	wuy_event_status_t	event_status;

	loop_timer_t		*timer_read;
	loop_timer_t		*timer_write;

	void			*read_buffer;
	int			read_buf_len;

	wuy_list_node_t		list_node;

	const loop_stream_ops_t	*ops;

	loop_t			*loop;
	void			*app_data;
};

static void loop_stream_close_for(loop_stream_t *s, const char *reason, int errnum);

/* timer utils */
static int64_t loop_stream_read_timeout(int64_t at, void *data)
{
	loop_stream_t *s = data;
	loop_stream_close_for(s, "read timedout", 0);
	return 0;
}
static int64_t loop_stream_write_timeout(int64_t at, void *data)
{
	loop_stream_t *s = data;
	loop_stream_close_for(s, "write timedout", 0);
	return 0;
}
static void loop_stream_set_timer_read(loop_stream_t *s)
{
	if (s->ops->tmo_read < 0) {
		return;
	}
	if (s->timer_read == NULL) {
		s->timer_read = loop_timer_new(s->loop, loop_stream_read_timeout, s);
	}
	loop_timer_set_after(s->timer_read, s->ops->tmo_read ? s->ops->tmo_read : 10*1000);
}
static void loop_stream_set_timer_write(loop_stream_t *s)
{
	if (s->ops->tmo_write < 0) {
		return;
	}
	if (s->timer_write == NULL) {
		s->timer_write = loop_timer_new(s->loop, loop_stream_write_timeout, s);
	}
	loop_timer_set_after(s->timer_write, s->ops->tmo_write ? s->ops->tmo_write : 10*1000);
}
static void loop_stream_del_timer_read(loop_stream_t *s)
{
	loop_timer_delete(s->timer_read);
	s->timer_read = NULL;
}
static void loop_stream_del_timer_write(loop_stream_t *s)
{
	loop_timer_delete(s->timer_write);
	s->timer_write = NULL;
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
static void loop_stream_del_event(loop_stream_t *s)
{
	wuy_event_del(s->loop->event_ctx, s->fd, &s->event_status);
}


static void loop_stream_close_for(loop_stream_t *s, const char *reason, int errnum)
{
	if (s->closed) {
		return;
	}
	s->closed = true;

	if (s->ops->on_close != NULL) {
		s->ops->on_close(s, reason, errnum);
	}

	if (s->ssl != NULL) {
		SSL_free(s->ssl);
	}
	loop_stream_del_event(s);
	close(s->fd);

	free(s->read_buffer);

	loop_stream_del_timer_read(s);
	loop_stream_del_timer_write(s);

	wuy_list_insert(&s->loop->stream_defer_head, &s->list_node);
}

static void loop_stream_clear_defer(void *data)
{
	wuy_list_t *head = data;
	wuy_list_node_t *node;
	wuy_list_iter_first(head, node) {
		wuy_list_delete(node);
		wuy_pool_free(wuy_containerof(node, loop_stream_t, list_node));
	}
}

static void loop_stream_readable(loop_stream_t *s)
{
	uint8_t buffer[s->ops->bufsz_read ? s->ops->bufsz_read : 1024*16];
	int read_len;
	int prev_len = 0;

	/* previous read data */
	if (s->read_buffer != NULL) {
		memcpy(buffer, s->read_buffer, s->read_buf_len);
		prev_len = s->read_buf_len;
	}

	do {
		if (s->ssl != NULL) {
			read_len = SSL_read(s->ssl, buffer + prev_len, sizeof(buffer) - prev_len);
			if (read_len <= 0) {
				int sslerr = SSL_get_error(s->ssl, read_len);
				if (sslerr == SSL_ERROR_WANT_READ || sslerr == SSL_ERROR_WANT_WRITE) {
					break;
				}
				if (sslerr == SSL_ERROR_ZERO_RETURN) {
					loop_stream_close_for(s, "peer close", 0);
					return;
				}
				loop_stream_close_for(s, "SSL read error", sslerr);
				return;
			}
		} else {
			read_len = read(s->fd, buffer + prev_len, sizeof(buffer) - prev_len);
			if (read_len < 0) {
				if (errno == EAGAIN) {
					break;
				}
				loop_stream_close_for(s, "read error", errno);
				return;
			}
			if (read_len == 0) {
				loop_stream_close_for(s, "peer close", 0);
				return;
			}
		}

		/* process data in buffer */
		read_len += prev_len;
		int proc_len = s->ops->on_read(s, buffer, read_len);
		if (proc_len < 0) {
			loop_stream_close_for(s, "app read error", 0);
			return;
		}
		if (proc_len > read_len) {
			loop_stream_close_for(s, "app read invalid return", 0);
			return;
		}

		prev_len = read_len - proc_len;
		if (prev_len == sizeof(buffer)) {
			loop_stream_close_for(s, "read buffer full", 0);
			return;
		}

		if (prev_len != 0 && proc_len != 0) {
			memmove(buffer, buffer + proc_len, prev_len);
		}

	} while(read_len < sizeof(buffer));

	if (prev_len != 0) {
		s->read_buf_len = prev_len;
		s->read_buffer = realloc(s->read_buffer, prev_len);
		memcpy(s->read_buffer, buffer, prev_len);

	} else if (s->read_buffer != NULL) {
		free(s->read_buffer);
		s->read_buffer = NULL;
		s->read_buf_len = 0;
	}

	loop_stream_set_event_read(s);
	loop_stream_set_timer_read(s);
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

static int loop_stream_write_handle(loop_stream_t *s, int data_len, int write_len)
{
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

int loop_stream_write(loop_stream_t *s, const void *data, int len)
{
	if (s->closed) {
		return -1;
	}
	if (s->write_blocked) {
		return 0;
	}

	int write_len;
	if (s->ssl != NULL) {
		write_len = SSL_write(s->ssl, data, len);
		if (write_len <= 0) {
			int sslerr = SSL_get_error(s->ssl, write_len);
			if (sslerr != SSL_ERROR_WANT_READ && sslerr != SSL_ERROR_WANT_WRITE) {
				loop_stream_close_for(s, "SSL write error", sslerr);
				return -1;
			}
		}
	} else {
		write_len = write(s->fd, data, len);
		if (write_len < 0 && errno != EAGAIN) {
			loop_stream_close_for(s, "write error", errno);
			return write_len;
		}
	}
	return loop_stream_write_handle(s, len, write_len);
}

int loop_stream_sendfile(loop_stream_t *s, int in_fd, off_t *offset, int len)
{
	if (s->closed) {
		return -1;
	}
	if (s->write_blocked) {
		return 0;
	}
	assert(s->ssl == NULL);

	int write_len = sendfile(s->fd, in_fd, offset, len);
	if (write_len < 0 && errno != EAGAIN) {
		loop_stream_close_for(s, "sendfile error", errno);
		return write_len;
	}
	return loop_stream_write_handle(s, len, write_len);
}

loop_stream_t *loop_stream_new(loop_t *loop, int fd, const loop_stream_ops_t *ops)
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

	/* add read event */
	loop_stream_set_event_read(s);
	loop_stream_set_timer_read(s);

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

void loop_stream_set_app_data(loop_stream_t *s, void *app_data)
{
	s->app_data = app_data;
}
void *loop_stream_get_app_data(loop_stream_t *s)
{
	return s->app_data;
}

SSL *loop_stream_get_ssl(loop_stream_t *s)
{
	return s->ssl;
}
void loop_stream_set_ssl(loop_stream_t *s, SSL *ssl)
{
	s->ssl = ssl;
}
