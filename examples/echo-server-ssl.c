#include <stdio.h>
#include <errno.h>
#include <openssl/ssl.h>
#include "loop.h"

#include "ssl_underlying.c"

SSL_CTX *ssl_ctx;

bool on_accept(loop_tcp_listen_t *loop_listen,
		loop_stream_t *s, struct sockaddr *addr)
{
	SSL *ssl = SSL_new(ssl_ctx);
	SSL_set_fd(ssl, loop_stream_fd(s));
	SSL_set_accept_state(ssl);
	loop_stream_set_underlying(s, ssl);
	return true;
}

int on_read(loop_stream_t *s, void *data, int len)
{
	printf("read: %d\n", len);
	return loop_stream_write(s, data, len);
}

int main()
{
	SSL_load_error_strings();	
	OpenSSL_add_ssl_algorithms();

	ssl_ctx = SSL_CTX_new(SSLv23_server_method());
	SSL_CTX_set_ecdh_auto(ssl_ctx, 1);
	SSL_CTX_use_certificate_file(ssl_ctx, "cert.pem", SSL_FILETYPE_PEM);
	SSL_CTX_use_PrivateKey_file(ssl_ctx, "key.pem", SSL_FILETYPE_PEM);

	loop_tcp_listen_ops_t ops = { .on_accept = on_accept };
	loop_stream_ops_t stream_ops = {
		.on_read = on_read,
		.underlying_read = ssl_underlying_read,
		.underlying_write = ssl_underlying_write,
		.underlying_close = ssl_underlying_close,
	};
	loop_t *loop = loop_new();
	loop_tcp_listen(loop, "1234", &ops, &stream_ops);
	loop_run(loop);
	return 0;
}
