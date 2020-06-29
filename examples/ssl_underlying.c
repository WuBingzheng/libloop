
int ssl_underlying_read(void *ssl, void *buffer, int buf_len)
{
	errno = 0;
	int read_len = SSL_read(ssl, buffer, buf_len);
	if (read_len <= 0) {
		int sslerr = SSL_get_error(ssl, read_len);
		if (sslerr == SSL_ERROR_WANT_READ || sslerr == SSL_ERROR_WANT_WRITE) {
			errno = EAGAIN;
			return -1;
		}
		if (sslerr == SSL_ERROR_ZERO_RETURN) {
			return 0;
		}
		return -1;
	}
	return read_len;
}

int ssl_underlying_write(void *ssl, const void *data, int len)
{
	errno = 0;
	int write_len = SSL_write(ssl, data, len);
	if (write_len <= 0) {
		int sslerr = SSL_get_error(ssl, write_len);
		if (sslerr != SSL_ERROR_WANT_READ && sslerr != SSL_ERROR_WANT_WRITE) {
			errno = EAGAIN;
		}
		return -1;
	}
	return write_len;
}

void ssl_underlying_close(void *ssl)
{
	SSL_free(ssl);
}
