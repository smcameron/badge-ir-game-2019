
CFLAGS=-pthread -fsanitize=address -Wall --pedantic -g
GTKCFLAGS:=$(subst -I,-isystem ,$(shell pkg-config --cflags gtk+-2.0))
GTKLDFLAGS:=$(shell pkg-config --libs gtk+-2.0) $(shell pkg-config --libs gthread-2.0)

CC=gcc

all:	lasertag irxmit

bline.o:	bline.c bline.h
	${CC} ${CFLAGS} -c bline.c

linuxcompat.o:	linuxcompat.c linuxcompat.h Makefile bline.h
	${CC} ${CFLAGS} ${GTKCFLAGS} -c linuxcompat.c

lasertag:	lasertag.c linuxcompat.o bline.o Makefile build_bug_on.h lasertag-protocol.h
	${CC} ${CFLAGS} ${GTKCFLAGS} -o lasertag lasertag.c linuxcompat.o bline.o ${GTKLDFLAGS}

irxmit:	irxmit.c lasertag-protocol.h
	${CC} ${CFLAGS} -o irxmit irxmit.c

clean:
	rm -f *.o badge-ir-game irxmit
