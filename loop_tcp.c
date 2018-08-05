#include <errno.h>
#include <stdlib.h>
#include <strings.h>

#include "wuy_tcp.h"

#include "loop_internal.h"
#include "loop.h"


struct loop_tcp_listen_s {
	int			type; /* keep this top! */
	int			fd;
	loop_stream_ops_t	*accepted_ops;
	loop_t			*loop;
	void			*app_data;
};

void loop_tcp_listen_acceptable(loop_tcp_listen_t *tl)
{
	int client_fd = wuy_tcp_accept(tl->fd, NULL);
	if (client_fd < 0) {
		if (errno != EAGAIN) {
			perror("accept fail");
		}
		return;
	}

	loop_stream_add(tl->loop, client_fd, tl->accepted_ops);

	/* tail recursive for accept loop */
	loop_tcp_listen_acceptable(tl);
}

loop_tcp_listen_t *loop_tcp_listen(loop_t *loop, struct sockaddr *addr,
		loop_stream_ops_t *accepted_ops)
{
	loop_tcp_listen_t *tl = malloc(sizeof(loop_tcp_listen_t));
	if (tl == NULL) {
		return NULL;
	}

	tl->fd = wuy_tcp_listen(addr, accepted_ops->tmo_read / 1000);
	if (tl->fd < 0) {
		free(tl);
		return NULL;
	}

	tl->type = LOOP_TYPE_TCP_LISTEN;
	tl->accepted_ops = accepted_ops;
	tl->loop = loop;

	wuy_event_status_t event_status;
	bzero(&event_status, sizeof(wuy_event_status_t));
	wuy_event_add_read(loop->event_ctx, tl->fd, tl, &event_status);

	/* fix up accepted_ops */
	if (accepted_ops->tmo_read == 0) {
		accepted_ops->tmo_read = 10 * 1000;
	}
	if (accepted_ops->tmo_write == 0 && accepted_ops->on_writable != NULL) {
		accepted_ops->tmo_write = 10 * 1000;
	}

	return tl;
}

loop_stream_t *loop_stream_tcp_connect(loop_t *loop, struct sockaddr *addr,
		loop_stream_ops_t *ops)
{
	return NULL;
}
