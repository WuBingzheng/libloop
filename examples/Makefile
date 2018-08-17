CFLAGS = -g -O2 -I../
LDFLAGS = -L../ -L../../libwuya/
LDLIBS = -lloop -lwuya

all: echo-server

echo-server: echo-server.o
	gcc -o $@ $^ $(LDFLAGS) $(LDLIBS)

clean:
	rm -f echo-server *.o

