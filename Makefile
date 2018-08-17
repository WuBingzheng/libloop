CFLAGS = -I../libwuya -g -O2

all: libloop.a

libloop.a: loop.o loop_stream.o loop_tcp.o loop_timer.o loop_inotify.o loop_idle.o
	ar rcs $@ $^

clean:
	rm -f *.o libloop.a
