CFLAGS = -I../libwuya -g

all: echo-server logsync-recv logsync-send

logsync-recv: logsync-recv.o loop.o loop_stream.o loop_tcp.o loop_timer.o loop_inotify.o loop_idle.o ../libwuya/wuy_dict.o  ../libwuya/wuy_heap.o  ../libwuya/wuy_pool.o ../libwuya/wuy_event.o ../libwuya/wuy_sockaddr.o

logsync-send: logsync-send.o loop.o loop_stream.o loop_tcp.o loop_timer.o loop_inotify.o loop_idle.o ../libwuya/wuy_dict.o  ../libwuya/wuy_heap.o  ../libwuya/wuy_pool.o ../libwuya/wuy_event.o ../libwuya/wuy_sockaddr.o

echo-server: echo-server.o loop.o loop_stream.o loop_tcp.o loop_timer.o loop_inotify.o loop_idle.o ../libwuya/wuy_dict.o  ../libwuya/wuy_heap.o  ../libwuya/wuy_pool.o ../libwuya/wuy_event.o ../libwuya/wuy_sockaddr.o

loop.o: loop.c loop.h loop_internal.h

loop_stream.o: loop_stream.c loop.h loop_internal.h

loop_tcp.o: loop_tcp.c loop.h loop_internal.h

loop_timer.o: loop_timer.c loop.h loop_internal.h

loop_idle.o: loop_idle.c loop.h loop_internal.h

loop_inotify.o: loop_inotify.c loop.h loop_internal.h

echo-server.o: echo-server.c

clean:
	rm -f *.o echo-server
