CFLAGS = -I../libwuya -g -Wall -O2

all: libloop.a

libloop.a: loop.o loop_stream.o loop_tcp.o loop_timer.o loop_inotify.o loop_idle.o loop_channel.o
	ar rcs $@ $^

clean:
	rm -f *.o libloop.a
