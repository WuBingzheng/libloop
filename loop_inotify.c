#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "loop_internal.h"
#include "loop.h"


struct loop_inotify_s {
	int			wd;
	bool			closed;
	char			*name;
	loop_inotify_t		*parent;

	loop_t			*loop;
	loop_inotify_ops_t	*ops;

	wuy_dict_node_t		dict_node;
	wuy_list_node_t		list_node;
	wuy_list_t		inside_head;

	void			*app_data;
};

static loop_inotify_t *loop_inotify_inside_get(loop_inotify_t *parent, char *name)
{
	loop_t *loop = parent->loop;

	loop_inotify_t key = { .parent = parent, .name = name };
	loop_inotify_t *inside = wuy_dict_get(loop->inside_inotify, &key);
	if (inside != NULL) {
		return inside;
	}

	/* create one */
	inside = wuy_pool_alloc(loop->inotify_pool);
	if (inside == NULL) {
		return NULL;
	}
	inside->parent = parent;
	inside->name = strdup(name);
	inside->ops = parent->ops;
	inside->loop = parent->loop;
	inside->wd = -1;
	inside->app_data = NULL;

	wuy_list_insert(&parent->inside_head, &inside->list_node);
	wuy_dict_add(loop->inside_inotify, inside);
	return inside;
}

static void loop_inotify_event(loop_t *loop, struct inotify_event *event)
{
	loop_inotify_t key = { .wd = event->wd };
	loop_inotify_t *in = wuy_dict_get(loop->wd_inotify, &key);
	if (in == NULL || in->closed) {
		return;
	}

	if (event->len > 0 && in->ops->on_inside) {
		loop_inotify_t *inside = loop_inotify_inside_get(in, event->name);
		if (inside == NULL || inside->closed) {
			return;
		}

		in->ops->on_inside(inside, event->mask, event->cookie);
	} else {
		in->ops->on_notify(in, event);
	}
}

struct _loop_inev_data {
	int	type; /* keep this top! */
	loop_t	*loop;
};

void loop_inotify_event_handler(void *data)
{
	loop_t *loop = ((struct _loop_inev_data *)data)->loop;

	int buf[1024]; /* use int to align */
	ssize_t read_len = read(loop->inotify_fd, buf, sizeof(buf));
	if (read_len < 0) {
		if (errno == EAGAIN) {
			return;
		}
		perror("inotify read");
		return;
	}

	char *ptr = (char *)buf;
	char *end = ptr + read_len;
	while (ptr < end) {
		struct inotify_event *event = (struct inotify_event *)ptr;

		loop_inotify_event(loop, event);

		ptr += sizeof(struct inotify_event) + event->len;
	}

	/* tail recursive for read loop */
	loop_inotify_event_handler(data);
}

loop_inotify_t *loop_inotify_add(loop_t *loop, const char *pathname,
		loop_inotify_ops_t *ops)
{
	loop_inotify_t *in = wuy_pool_alloc(loop->inotify_pool);
	if (in == NULL) {
		return NULL;
	}

	in->name = strdup(pathname);
	if (in->name == NULL) {
		wuy_pool_free(in);
		return NULL;
	}

	in->wd = inotify_add_watch(loop->inotify_fd, pathname, ops->mask);
	if (in->wd < 0) {
		free(in->name);
		wuy_pool_free(in);
		return NULL;
	}

	in->ops = ops;
	in->loop = loop;
	in->parent = NULL;
	in->app_data = NULL;

	wuy_list_init(&in->inside_head);
	wuy_dict_add(loop->wd_inotify, in);
	return in;
}

void loop_inotify_delete(loop_inotify_t *in)
{
	if (in->closed) {
		return;
	}
	in->closed = true;

	if (in->ops->on_delete != NULL) {
		in->ops->on_delete(in);
	}

	loop_t *loop = in->loop;
	if (in->parent == NULL) {
		inotify_rm_watch(loop->inotify_fd, in->wd);
		wuy_dict_delete(loop->wd_inotify, in);

		wuy_list_node_t *n;
		wuy_list_iter(&in->inside_head, n) {
			loop_inotify_delete(wuy_containerof(n, loop_inotify_t, list_node));
		}
	} else {
		wuy_list_delete(&in->list_node);
	}

	wuy_list_insert(&loop->inotify_defer_head, &in->list_node);
}

static void loop_inotify_clear_defer(void *data)
{
	wuy_list_t *head = data;
	wuy_list_node_t *node;
	wuy_list_iter_first(head, node) {
		wuy_list_delete(node);
		wuy_pool_free(wuy_containerof(node, loop_inotify_t, list_node));
	}
}

static uint32_t loop_inotify_dict_inside_hash(const void *item)
{
	const loop_inotify_t *in = item;
	return wuy_dict_hash_pointer(in->parent) ^ wuy_dict_hash_string(in->name);
}
static bool loop_inotify_dict_inside_equal(const void *a, const void *b)
{
	const loop_inotify_t *ina = a;
	const loop_inotify_t *inb = b;
	return ina->parent == inb->parent && strcmp(ina->name, inb->name) == 0;
}

void loop_inotify_init(loop_t *loop)
{
	loop->inotify_pool = wuy_pool_new_type(loop_inotify_t);

	loop->inotify_fd = inotify_init1(IN_NONBLOCK);
	assert(loop->inotify_fd >= 0);

	struct _loop_inev_data *data = malloc(sizeof(struct _loop_inev_data));
	data->type = LOOP_TYPE_INOTIFY;
	data->loop = loop;
	wuy_event_status_t event_status;
	bzero(&event_status, sizeof(wuy_event_status_t));
	wuy_event_add_read(loop->event_ctx, loop->inotify_fd, data, &event_status);

	loop->wd_inotify = wuy_dict_new_type(WUY_DICT_KEY_UINT32,
			offsetof(loop_inotify_t, wd),
			offsetof(loop_inotify_t, dict_node));

	loop->inside_inotify = wuy_dict_new_func(loop_inotify_dict_inside_hash,
			loop_inotify_dict_inside_equal,
			offsetof(loop_inotify_t, dict_node));

	wuy_list_init(&loop->inotify_defer_head);

	loop_idle_add(loop, loop_inotify_clear_defer, &loop->inotify_defer_head);
}
