#include <stdio.h>
#include "loop.h"

ssize_t on_read(loop_stream_t *s, void *data, size_t len)
{
	printf("read: %ld\n", len);
	return loop_stream_write(s, data, len);
}

int main()
{
	loop_stream_ops_t ops = { .on_read = on_read };
	loop_t *loop = loop_new();
	loop_tcp_listen(loop, "1234", NULL, &ops);
	return loop_run(loop);
}
