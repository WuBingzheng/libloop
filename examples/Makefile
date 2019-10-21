CFLAGS = -g -O2 -I../
LDFLAGS = -L../ -L../../libwuya/
LDLIBS = -lloop -lwuya -lssl -lcrypto

all: echo-server echo-server-ssl ssl-client

echo-server: echo-server.o
	gcc -o $@ $^ $(LDFLAGS) $(LDLIBS)

echo-server-ssl: echo-server-ssl.o
	gcc -o $@ $^ $(LDFLAGS) $(LDLIBS)

ssl-client: ssl-client.o
	gcc -o $@ $^ $(LDFLAGS) $(LDLIBS)

clean:
	rm -f echo-server echo-server-ssl ssl-client *.o

