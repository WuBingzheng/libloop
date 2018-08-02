CFLAGS = -I../libwuya -g

all: echo-server

echo-server: echo-server.o loop.o loop_stream.o loop_tcp.o loop_timer.o loop_idle.o ../libwuya/wuy_dict.o  ../libwuya/wuy_heap.o  ../libwuya/wuy_pool.o ../libwuya/wuy_event.o

loop.o: loop.c loop.h loop_internal.h

loop_stream.o: loop_stream.c loop.h loop_internal.h

loop_tcp.o: loop_tcp.c loop.h loop_internal.h

loop_timer.o: loop_timer.c loop.h loop_internal.h

loop_idle.o: loop_idle.c loop.h loop_internal.h

echo-server.o: echo-server.c

clean:
	rm -f *.o echo-server
