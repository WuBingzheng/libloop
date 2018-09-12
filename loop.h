#ifndef LOOP_H
#define LOOP_H

#include <stddef.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>


/* loop */
typedef struct loop_s loop_t;

loop_t *loop_new(void);
void loop_run(loop_t *loop);
void loop_kill(loop_t *loop);

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

/* loop.tcp */
#include <sys/socket.h>
typedef struct loop_tcp_listen_s loop_tcp_listen_t;
typedef struct {
	bool	(*on_accept)(loop_tcp_listen_t *, loop_stream_t *,
			struct sockaddr *addr);
	int	tmo_defer;
	int	backlog;
	bool	reuse_port;
} loop_tcp_listen_ops_t;

loop_tcp_listen_t *loop_tcp_listen(loop_t *loop, const char *addr,
		loop_tcp_listen_ops_t *ops, loop_stream_ops_t *accepted_ops);

loop_stream_t *loop_tcp_connect(loop_t *loop, const char *addr,
		unsigned short default_port, loop_stream_ops_t *ops);

/* loop.inotify */
#include <sys/inotify.h>
typedef struct loop_inotify_s loop_inotify_t;
typedef struct {
	void (*on_notify)(loop_inotify_t *, const struct inotify_event *);
	void (*on_inside)(loop_inotify_t *, uint32_t mask, uint32_t cookie);
	void (*on_delete)(loop_inotify_t *);
	uint32_t	mask;
} loop_inotify_ops_t;

loop_inotify_t *loop_inotify_add(loop_t *loop, const char *pathname,
		loop_inotify_ops_t *ops);
void loop_inotify_delete(loop_inotify_t *in);

/* loop.timer */
typedef int64_t loop_timer_f(int64_t at, void *data);
typedef struct loop_timer_s loop_timer_t;

loop_timer_t *loop_timer_new(loop_t *loop, loop_timer_f *handler, void *data);
bool loop_timer_set_at(loop_t *loop, loop_timer_t *timer, int64_t at);
bool loop_timer_set_after(loop_t *loop, loop_timer_t *timer, int64_t after);
void loop_timer_delete(loop_t *loop, loop_timer_t *timer);

#endif
