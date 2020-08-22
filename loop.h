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
 * @brief Reasons of stream close, used by loop_stream_ops_t.on_close().
 */
enum loop_stream_close_reason {
	LOOP_STREAM_APP_CLOSE = 1,
	LOOP_STREAM_TIMEOUT,
	LOOP_STREAM_READ_ERROR,
	LOOP_STREAM_PEER_CLOSE,
	LOOP_STREAM_APP_READ_ERROR,
	LOOP_STREAM_APP_INVALID_RETURN,
	LOOP_STREAM_READ_BUFFER_FULL,
	LOOP_STREAM_WRITE_BLOCK,
	LOOP_STREAM_WRITE_ERROR,
	LOOP_STREAM_SENDFILE_ERROR,
	LOOP_STREAM_CLOSED_YET,
};

/**
 * @brief Explain close reason to string.
 */
const char *loop_stream_close_string(enum loop_stream_close_reason reason);

/**
 * @brief Stream type operations and settings.
 *
 * field:on_read() should return the processed data length. If the returned
 * length is less than the argument len (e.g. not complete message), the
 * not-processed data will be saved and passed in the next time.
 * If returns negetive (e.g. invalid message), the connection will be closed.
 *
 * field:on_read() or field:on_readable() is the only mandatory member you must set.
 */
typedef struct {
	int	(*on_read)(loop_stream_t *, void *data, int len); ///< read available handler
	void	(*on_close)(loop_stream_t *, enum loop_stream_close_reason); ///< close handler
	void	(*on_readable)(loop_stream_t *); ///< read available handler, used with loop_stream_read()
	void	(*on_writable)(loop_stream_t *); ///< write available handler, used with loop_stream_write()

	int	(*underlying_read)(void *underlying, void *buffer, int len);
	int	(*underlying_write)(void *underlying, const void *data, int len);
	void	(*underlying_close)(void *underlying);

	int	bufsz_read; ///< read buffer size. Use 16K if not set.

	int	timeout_ms;   ///< active timeout in millisecond. Set 0 to disable.
} loop_stream_ops_t;

/**
 * @brief Create a stream.
 */
loop_stream_t *loop_stream_new(loop_t *loop, int fd, const loop_stream_ops_t *ops,
		bool write_blocked);

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
int loop_stream_write(loop_stream_t *, const void *data, int len);

/**
 * @brief Write data to stream by sendfile().
 *
 * Return the same with loop_stream_write().
 */
int loop_stream_sendfile(loop_stream_t *, int in_fd, off_t *offset, int len);

/**
 * @brief Read data from stream. This should be called in ops->on_readable().
 *
 * Return <0 if connection error or closed;
 * Return =0 if read blocked;
 * Return >0 as read length.
 */
int loop_stream_read(loop_stream_t *s, void *buffer, int buf_len);

/**
 * @brief Set timer.
 */
void loop_stream_set_timeout(loop_stream_t *s, int timeout_ms);

/**
 * @brief Close a stream.
 */
void loop_stream_close(loop_stream_t *);

/**
 * @brief Return stream's fd.
 */
int loop_stream_fd(loop_stream_t *s);

/**
 * @brief Return if stream's closed.
 */
bool loop_stream_is_closed(loop_stream_t *s);

/**
 * @brief Return if stream's read-blocked.
 */
bool loop_stream_is_read_blocked(loop_stream_t *s);

/**
 * @brief Return if stream's write blocked.
 */
bool loop_stream_is_write_blocked(loop_stream_t *s);

/**
 * @brief Set application data to stream.
 */
void loop_stream_set_app_data(loop_stream_t *s, void *app_data);
/**
 * @brief Get application data to stream.
 */
void *loop_stream_get_app_data(loop_stream_t *s);

/**
 * @brief Set stream's underlying context.
 *
 * Make sure that stream's ops->underlying_*() are set.
 */
void loop_stream_set_underlying(loop_stream_t *s, void *underlying);
/**
 * @brief Get stream's underlying context.
 */
void *loop_stream_get_underlying(loop_stream_t *s);

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

	int	defer;  ///< defer accept timeout in second. Use 10 if not set.
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
		const loop_tcp_listen_ops_t *ops,
		const loop_stream_ops_t *accepted_ops);

/**
 * @brief Set application data to listen context.
 */
void loop_tcp_listen_set_app_data(loop_tcp_listen_t *tl, void *app_data);
/**
 * @brief Get application data to listen context.
 */
void *loop_tcp_listen_get_app_data(loop_tcp_listen_t *tl);

/**
 * @brief Parse address and create a stream by connect on TCP.
 *
 * @param addr and default_port see libwuya/wuy_sockaddr.h:wuy_sockaddr_pton() for format.
 */
loop_stream_t *loop_tcp_connect(loop_t *loop, const char *addr,
		unsigned short default_port, const loop_stream_ops_t *ops);

/**
 * @brief Create a stream by connect to sockaddr on TCP.
 */
loop_stream_t *loop_tcp_connect_sockaddr(loop_t *loop, struct sockaddr *sa,
		const loop_stream_ops_t *ops);

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
 * @brief Suspend the timer.
 */
void loop_timer_suspend(loop_timer_t *timer);

/**
 * @brief Delete the timer.
 */
void loop_timer_delete(loop_timer_t *timer);


/* == loop.group_timer == */

/**
 * @brief Group timer.
 *
 * The timers that have the same expiration time can be a group, and
 * loop.group_timer is useful here.
 *
 * loop.timer is OK, however loop.group_timer is better in some cases.
 *
 * loop.timer is managed by a heap which need a continuously memory.
 * The memory may grow big if may timers. There are also memory copy
 * during the growing. Besides the timer's time complexity is O(logN).
 *
 * While loop.group_timer is managed by a list. So continuously memory
 * is not need, and the complexity is O(1). If your program uses many
 * timers, loop.group_timer worth a shot.
 */
typedef struct loop_group_timer_s loop_group_timer_t;

/**
 * @brief Group timer node.
 *
 * The group-timer-node can be set to a group to enable timer.
 */
typedef struct loop_group_timer_node_s loop_group_timer_node_t;

/**
 * @brief Create a new group timer.
 *
 * @param handler  called for each expired group-timer-node.
 * @param period  each added group-timer-node expires after this millisecond
 */
loop_group_timer_t *loop_group_timer_new(loop_t *loop,
		loop_timer_f *handler, int64_t period);

/**
 * @brief Create a new group timer node.
 */
loop_group_timer_node_t *loop_group_timer_node_new(void *data);

/**
 * @brief Set the node to group.
 *
 * The node can be set to different groups.
 *
 * If the node was alreay set to a group, this group or others,
 * it will be removed from that group first.
 */
void loop_group_timer_node_set(loop_group_timer_t *group, loop_group_timer_node_t *node);

/**
 * @brief Suspend the node.
 */
void loop_group_timer_node_suspend(loop_group_timer_node_t *node);

/**
 * @brief Suspend and free the node.
 */
void loop_group_timer_node_delete(loop_group_timer_node_t *node);

/**
 * @brief Expire one group-timer-node at @at.
 *
 * Return if expired.
 */
bool loop_group_timer_expire_one_at(loop_group_timer_t *group, int64_t at);

/**
 * @brief Expire one group-timer-node ahead @ahead.
 *
 * Return if expired.
 */
bool loop_group_timer_expire_one_ahead(loop_group_timer_t *group, int64_t ahead);

#endif
