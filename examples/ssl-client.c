#include <stdio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "loop.h"

#include "ssl_underlying.c"

int on_read(loop_stream_t *s, void *data, int len)
{
	char *str = data;
	str[len] = '\0';
	printf("read: %d. %s\n", len, str);
	return len; // loop_stream_write(s, data, len);
}
void on_writable(loop_stream_t *s)
{
	printf("on_writable\n");
	int len = loop_stream_write(s, "world\n", 6);
	printf("write len: %d\n", len);
}

void on_close(loop_stream_t *s, enum loop_stream_close_reason reason)
{
	printf("close for %s\n", loop_stream_close_string(reason));
}

int main()
{
	ERR_load_crypto_strings();
	SSL_load_error_strings();	
	OpenSSL_add_ssl_algorithms();
	SSL_CTX *ssl_ctx = SSL_CTX_new(SSLv23_client_method());
	SSL_CTX_set_ecdh_auto(ssl_ctx, 1);

	loop_stream_ops_t ops = {
		.on_read = on_read,
		.on_writable = on_writable,
		.on_close = on_close,

		.underlying_read = ssl_underlying_read,
		.underlying_write = ssl_underlying_write,
		.underlying_close = ssl_underlying_close,
	};
	loop_t *loop = loop_new();
	loop_stream_t *s = loop_tcp_connect(loop, "127.0.0.1:1234", 0, &ops);

	SSL *ssl = SSL_new(ssl_ctx);
	SSL_set_fd(ssl, loop_stream_fd(s));
	SSL_set_connect_state(ssl);
	loop_stream_set_underlying(s, ssl);

	int len = loop_stream_write(s, "hello\n", 6);
	printf("write first len: %d\n", len);
	loop_run(loop);
	return 0;
}
