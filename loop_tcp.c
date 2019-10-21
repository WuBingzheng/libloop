#include <errno.h>
#include <stdlib.h>
#include <strings.h>

#include "wuy_tcp.h"
#include "wuy_sockaddr.h"

#include "loop_internal.h"
#include "loop.h"


struct loop_tcp_listen_s {
	int			type; /* keep this top! */
	int			fd;
	const loop_tcp_listen_ops_t	*ops;
	const loop_stream_ops_t	*accepted_ops;
	loop_t			*loop;
	void			*app_data;
};

void loop_tcp_listen_acceptable(loop_tcp_listen_t *tl)
{
	while (1) {
		struct sockaddr client_addr;
		int client_fd = wuy_tcp_accept(tl->fd, &client_addr);
		if (client_fd < 0) {
			if (errno != EAGAIN) {
				perror("accept fail");
			}
			return;
		}

		loop_stream_t *s = loop_stream_new(tl->loop,
				client_fd, tl->accepted_ops);

		if (tl->ops->on_accept) {
			if (!tl->ops->on_accept(tl, s, &client_addr)) {
				loop_stream_close(s);
				continue;
			}
		} else {
			loop_stream_set_app_data(s, tl);
		}
	}
}

loop_tcp_listen_t *loop_tcp_listen(loop_t *loop, const char *addr,
		const loop_tcp_listen_ops_t *ops,
		const loop_stream_ops_t *accepted_ops)
{
	static loop_tcp_listen_ops_t default_ops;
	if (ops == NULL) {
		ops = &default_ops;
	}

	/* socket listen */
	struct sockaddr sa;
	if (!wuy_sockaddr_pton(addr, &sa, 0)) {
		return NULL;
	}
	int fd = wuy_tcp_listen(&sa, ops->backlog ? ops->backlog : 1000, ops->reuse_port);
	if (fd < 0) {
		return NULL;
	}

	int defer = ops->tmo_defer ? ops->tmo_defer : accepted_ops->tmo_read;
	wuy_tcp_set_defer_accept(fd, defer);

	/* add event */
	loop_tcp_listen_t *tl = malloc(sizeof(loop_tcp_listen_t));
	if (tl == NULL) {
		close(fd);
		return NULL;
	}

	tl->fd = fd;
	tl->type = LOOP_TYPE_TCP_LISTEN;
	tl->ops = ops;
	tl->accepted_ops = accepted_ops;
	tl->loop = loop;

	wuy_event_status_t event_status;
	bzero(&event_status, sizeof(wuy_event_status_t));
	wuy_event_add_read(loop->event_ctx, tl->fd, tl, &event_status);

	return tl;
}

void loop_tcp_listen_set_app_data(loop_tcp_listen_t *tl, void *app_data)
{
	tl->app_data = app_data;
}
void *loop_tcp_listen_get_app_data(loop_tcp_listen_t *tl)
{
	return tl->app_data;
}

loop_stream_t *loop_tcp_connect(loop_t *loop, const char *addr,
		unsigned short default_port, const loop_stream_ops_t *ops)
{
	struct sockaddr sa;
	if (!wuy_sockaddr_pton(addr, &sa, default_port)) {
		return NULL;
	}
	int fd = wuy_tcp_connect(&sa);
	if (fd < 0) {
		return NULL;
	}

	loop_stream_t *s = loop_stream_new(loop, fd, ops);
	if (s == NULL) {
		return NULL;
	}

	return s;
}
