#include <stdio.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <strings.h>

#include "loop.h"


ssize_t on_read(loop_stream_t *s, void *data, size_t len)
{
	printf("read: %ld\n", len);
	return loop_stream_write(s, data, len);
}

int main()
{
	struct sockaddr_in addr;
	bzero(&addr, sizeof(struct sockaddr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(2345);

	loop_stream_ops_t ops = { .on_read = on_read };
	loop_t *loop = loop_new();
	loop_tcp_listen(loop, (struct sockaddr *)&addr, &ops);
	return loop_run(loop);
}
