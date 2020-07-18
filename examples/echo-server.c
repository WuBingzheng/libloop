#include <stdio.h>
#include "loop.h"

int on_read(loop_stream_t *s, void *data, int len)
{
	printf("read: %d\n", len);
	return loop_stream_write(s, data, len);
}

int main()
{
	loop_stream_ops_t ops = { .on_read = on_read, .timeout_ms = 10*1000 };
	loop_t *loop = loop_new();
	loop_tcp_listen(loop, "1234", NULL, &ops);
	loop_run(loop);
	return 0;
}
