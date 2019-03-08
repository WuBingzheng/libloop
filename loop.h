/**
 * @file     loop.h
 * @author   Wu Bingzheng <wubingzheng@gmail.com>
 * @date     2018-7-19
 *
 * @section LICENSE
 * GPLv2
 *
 * @section DESCRIPTION
 *
 * An event driven lib.
 */

#ifndef LOOP_H
#define LOOP_H

#include <stddef.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>


/* == loop == */

/**
 * @brief The loop context.
 */
typedef struct loop_s loop_t;

/**
 * @brief Create a new loop.
 */
loop_t *loop_new(void);

/**
 * @brief Start the loop.
 */
void loop_run(loop_t *loop);

/**
 * @brief Stop the loop.
 */
void loop_kill(loop_t *loop);


/* == loop.idle == */

/**
 * @brief Idle handler type.
 */
typedef void loop_idle_f(void *data);

/**
 * @brief Add an idle handler, which will be called at each loop iteration.
 */
bool loop_idle_add(loop_t *loop, loop_idle_f *func, void *data);


/* == loop.stream == */

/**
 * @brief Stream type, including pipe and tcp connection.
 */
typedef struct loop_stream_s loop_stream_t;

/**
 * @brief Stream type operations and settings.
 *
 * field:on_read() should return the processed data length. If the returned
 * length is less than the argument len (e.g. not complete message), the
 * not-processed data will be saved and passed in the next time.
 * If returns negetive (e.g. invalid message), the connection will be closed.
 *
 * field:on_read() is the only mandatory member you must set.
 */
typedef struct {
	ssize_t	(*on_read)(loop_stream_t *, void *data, size_t len); ///< read available handler
	void	(*on_close)(loop_stream_t *, const char *reason, int errnum); ///< close handler
	void	(*on_writable)(loop_stream_t *); ///< write available handler, used with loop_stream_write()

	int	bufsz_read; ///< read buffer size. Use 16K if not set.

	int	tmo_read;   ///< read timeout in millisecond. Use 10*1000 if not set.
	int	tmo_write;  ///< write timeout in millisecond. Use 10*1000 if not set.
} loop_stream_ops_t;

/**
 * @brief Add a stream fd, which can be pipe or tcp connection.
 */
loop_stream_t *loop_stream_add(loop_t *loop, int fd, loop_stream_ops_t *ops);

/**
 * @brief Write data to stream.
 *
 * Return writen data length if successful;
 * Return -1 if fail, and the stream will be closed before this returning.
 *
 * If writing blocks: 1) if ops.on_writable is not set, the blocking is traded
 * as a fail, so this returns -1 and closes the stream; 2) if ops.on_writable
 * is set, this returns the writen data length (maybe 0), and the application
 * should save the un-writen data, and re-send it in ops.on_writable().
 */
ssize_t loop_stream_write(loop_stream_t *, const void *data, size_t len);

/**
 * @brief Write data to stream by sendfile().
 *
 * Return the same with loop_stream_write().
 */
ssize_t loop_stream_sendfile(loop_stream_t *, int in_fd, off_t *offset, size_t len);

/**
 * @brief Close a stream.
 */
void loop_stream_close(loop_stream_t *);

/**
 * @brief Return stream's fd.
 */
int loop_stream_fd(loop_stream_t *s);

/**
 * @brief Set application data to stream.
 */
void loop_stream_set_app_data(loop_stream_t *s, void *app_data);
/**
 * @brief Get application data to stream.
 */
void *loop_stream_get_app_data(loop_stream_t *s);


/* == loop.tcp == */

#include <sys/socket.h>

/**
 * @brief Tcp listen context.
 */
typedef struct loop_tcp_listen_s loop_tcp_listen_t;

/**
 * @brief Tcp listen operations and settings.
 */
typedef struct {
	bool	(*on_accept)(loop_tcp_listen_t *, loop_stream_t *,
			struct sockaddr *addr); ///< accept handler. Return false to refuse.

	int	tmo_defer;  ///< defer accept timeout in millisecond. Use accepted_ops.tmo_read if not set.
	int	backlog;    ///< listen backlog. Use 1000 if not set.
	bool	reuse_port; ///< if use SO_REUSEPORT. Default if false.
} loop_tcp_listen_ops_t;

/**
 * @brief Add a listen on address.
 *
 * @param addr see libwuya/wuy_sockaddr.h:wuy_sockaddr_pton() for format.
 * @param ops could be set as NULL if using call default values.
 * @param accepted_ops is applied for accepted connections.
 */
loop_tcp_listen_t *loop_tcp_listen(loop_t *loop, const char *addr,
		loop_tcp_listen_ops_t *ops, loop_stream_ops_t *accepted_ops);

/**
 * @brief Set application data to listen context.
 */
void loop_tcp_listen_set_app_data(loop_tcp_listen_t *tl, void *app_data);
/**
 * @brief Get application data to listen context.
 */
void *loop_tcp_listen_get_app_data(loop_tcp_listen_t *tl);

/**
 * @brief Create a stream by connect to address on TCP.
 *
 * @param addr and default_port see libwuya/wuy_sockaddr.h:wuy_sockaddr_pton() for format.
 */
loop_stream_t *loop_tcp_connect(loop_t *loop, const char *addr,
		unsigned short default_port, loop_stream_ops_t *ops);


/* == loop.inotify == */

#include <sys/inotify.h>

/**
 * @brief Inotify context.
 */
typedef struct loop_inotify_s loop_inotify_t;

/**
 * @brief Inotify operations and settings.
 */
typedef struct {
	void (*on_notify)(loop_inotify_t *, const struct inotify_event *); ///< notify handler
	void (*on_inside)(loop_inotify_t *, uint32_t mask, uint32_t cookie); ///< recursion in directory
	void (*on_delete)(loop_inotify_t *); ///< delete handler
	uint32_t	mask; ///< inotify mask
} loop_inotify_ops_t;

/**
 * @brief Add an inotify on pathname.
 */
loop_inotify_t *loop_inotify_add(loop_t *loop, const char *pathname,
		loop_inotify_ops_t *ops);

/**
 * @brief Delete the inotify.
 */
void loop_inotify_delete(loop_inotify_t *in);


/* == loop.timer == */

/**
 * @brief Timer handler type.
 *
 * The argument at is the timestamp in millisecond when fired.
 * The argument data is the application data set when creating the timer.
 *
 * If you want the timer fired again later, return the gap time in millisecond.
 * If you want to delete the timer, return -1.
 * Otherwise, return 0 for nothing.
 */
typedef int64_t loop_timer_f(int64_t at, void *data);

/**
 * @brief Timer.
 */
typedef struct loop_timer_s loop_timer_t;

/**
 * @brief Create a timer.
 */
loop_timer_t *loop_timer_new(loop_t *loop, loop_timer_f *handler, void *data);

/**
 * @brief Enable the timer fired at param:at in millisecond.
 */
bool loop_timer_set_at(loop_timer_t *timer, int64_t at);

/**
 * @brief Enable the timer fired after param:after in millisecond.
 */
bool loop_timer_set_after(loop_timer_t *timer, int64_t after);

/**
 * @brief Delete the timer.
 */
void loop_timer_delete(loop_timer_t *timer);

#endif
