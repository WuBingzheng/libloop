#include <stdio.h>
#include "loop.h"

ssize_t on_read(loop_stream_t *s, void *data, size_t len)
{
	printf("read: %ld\n", len);
	return loop_stream_write(s, data, len);
}

int main()
{
	SSL_load_error_strings();	
	OpenSSL_add_ssl_algorithms();

	SSL_CTX *ctx = SSL_CTX_new(SSLv23_server_method());
	SSL_CTX_set_ecdh_auto(ctx, 1);
	SSL_CTX_use_certificate_file(ctx, "cert.pem", SSL_FILETYPE_PEM);
	SSL_CTX_use_PrivateKey_file(ctx, "key.pem", SSL_FILETYPE_PEM);

	loop_stream_ops_t ops = { .on_read = on_read, .ssl_ctx = ctx };
	loop_t *loop = loop_new();
	loop_tcp_listen(loop, "1234", NULL, &ops);
	loop_run(loop);
	return 0;
}
