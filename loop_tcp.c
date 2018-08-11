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
	loop_tcp_listen_ops_t	*ops;
	loop_stream_ops_t	*accepted_ops;
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

		loop_stream_t *s = loop_stream_add(tl->loop,
				client_fd, tl->accepted_ops);

		if (tl->ops->on_accept) {
			if (!tl->ops->on_accept(tl, s, &client_addr)) {
				loop_stream_close(s);
			}
		}
	}
}

loop_tcp_listen_t *loop_tcp_listen(loop_t *loop, const char *addr,
		loop_tcp_listen_ops_t *ops, loop_stream_ops_t *accepted_ops)
{
	static loop_tcp_listen_ops_t default_zero_ops;

	struct sockaddr sa;
	if (!wuy_sockaddr_pton(addr, &sa, 0)) {
		return NULL;
	}

	loop_tcp_listen_t *tl = malloc(sizeof(loop_tcp_listen_t));
	if (tl == NULL) {
		return NULL;
	}

	if (ops == NULL) {
		ops = &default_zero_ops;
	}

	int defer = ops->tmo_defer ? ops->tmo_defer : accepted_ops->tmo_read;

	tl->fd = wuy_tcp_listen(&sa, defer / 1000);
	if (tl->fd < 0) {
		free(tl);
		return NULL;
	}

	tl->type = LOOP_TYPE_TCP_LISTEN;
	tl->ops = ops;
	tl->accepted_ops = accepted_ops;
	tl->loop = loop;

	wuy_event_status_t event_status;
	bzero(&event_status, sizeof(wuy_event_status_t));
	wuy_event_add_read(loop->event_ctx, tl->fd, tl, &event_status);

	/* fix up accepted_ops */
	if (accepted_ops->tmo_read == 0) {
		accepted_ops->tmo_read = 10 * 1000;
	}
	if (accepted_ops->tmo_write == 0) {
		accepted_ops->tmo_write = 10 * 1000;
	}

	return tl;
}

loop_stream_t *loop_tcp_connect(loop_t *loop, const char *addr,
		unsigned short default_port, loop_stream_ops_t *ops)
{
	struct sockaddr sa;
	if (!wuy_sockaddr_pton(addr, &sa, default_port)) {
		return NULL;
	}
	int fd = wuy_tcp_connect(&sa);
	if (fd < 0) {
		return NULL;
	}

	/* fix up ops */
	if (ops->tmo_read == 0) {
		ops->tmo_read = 10 * 1000;
	}
	if (ops->tmo_write == 0) {
		ops->tmo_write = 10 * 1000;
	}
	return loop_stream_add(loop, fd, ops);
}
