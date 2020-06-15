CC?=gcc
CFLAGS+=-ansi
LDFLAGS?=
LIBS=-llua.5.1 -lm -ldl -lncurses

craftos: obj/fs_handle.o obj/fs.o obj/keys.o obj/lib.o obj/main.o obj/os.o obj/queue.o obj/term.o
	$(CC) $(LDFLAGS) -o craftos obj/fs_handle.o obj/fs.o obj/keys.o obj/lib.o obj/main.o obj/os.o obj/queue.o obj/term.o $(LIBS)

obj/fs_handle.o: fs_handle.c fs_handle.h
	$(CC) $(CFLAGS) -o obj/fs_handle.o -c fs_handle.c

obj/fs.o: fs.c fs.h fs_handle.h lib.h
	$(CC) $(CFLAGS) -o obj/fs.o -c fs.c

obj/keys.o: keys.c keys.h
	$(CC) $(CFLAGS) -o obj/keys.o -c keys.c

obj/lib.o: lib.c lib.h
	$(CC) $(CFLAGS) -o obj/lib.o -c lib.c

obj/main.o: main.c lib.h fs.h os.h bit.h redstone.h
	$(CC) $(CFLAGS) -o obj/main.o -c main.c

obj/os.o: os.c os.h lib.h
	$(CC) $(CFLAGS) -o obj/os.o -c os.c

obj/queue.o: queue.c queue.h
	$(CC) $(CFLAGS) -o obj/queue.o -c queue.c

obj/term.o: term.c term.h lib.h
	$(CC) $(CFLAGS) -o obj/term.o -c term.c
