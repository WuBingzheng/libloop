#ifndef LOOP_H
#define LOOP_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/socket.h>


/* loop */
typedef struct loop_s loop_t;

loop_t *loop_new(void);
int loop_run(loop_t *loop);
int loop_kill(loop_t *loop);

/* loop.idle */
typedef void loop_idle_f(void *data);

bool loop_idle_add(loop_t *loop, loop_idle_f *func, void *data);

/* loop.stream */
typedef struct loop_stream_s loop_stream_t;
typedef struct {
	ssize_t	(*on_read)(loop_stream_t *, void *data, size_t len);
	void	(*on_close)(loop_stream_t *, const char *reason, int errnum);
	void	(*on_writable)(loop_stream_t *);

	int	bufsz_read;

	int	tmo_read;
	int	tmo_write;
} loop_stream_ops_t;

loop_stream_t *loop_stream_add(loop_t *loop, int fd, loop_stream_ops_t *ops);
ssize_t loop_stream_write(loop_stream_t *, const void *data, size_t len);
ssize_t loop_stream_sendfile(loop_stream_t *, int in_fd, off_t *offset, size_t len);
void loop_stream_close(loop_stream_t *);
void loop_stream_idle(loop_stream_t *, int timeout);
int loop_stream_fd(loop_stream_t *s);

/* loop.tcp_listen */
typedef struct loop_tcp_listen_s loop_tcp_listen_t;
loop_tcp_listen_t *loop_tcp_listen(loop_t *loop, struct sockaddr *addr,
		loop_stream_ops_t *ops);

/* loop.inotify */

/* loop.timer */
typedef struct loop_timer_s loop_timer_t;

typedef void loop_timer_handler_f(loop_timer_t *);

#include "wuy_heap.h"
struct loop_timer_s {
	int64_t			expire;
	wuy_heap_node_t		heap_node;
	loop_timer_handler_f	*handler;
};

bool loop_timer_add(loop_t *loop, loop_timer_t *timer,
		int64_t timeout_ms, loop_timer_handler_f *handler);
void loop_timer_delete(loop_t *loop, loop_timer_t *timer);

#endif
