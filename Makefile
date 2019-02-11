
CFLAGS=-pthread -fsanitize=address -Wall --pedantic -g
GTKCFLAGS:=$(subst -I,-isystem ,$(shell pkg-config --cflags gtk+-2.0))
GTKLDFLAGS:=$(shell pkg-config --libs gtk+-2.0) $(shell pkg-config --libs gthread-2.0)

CC=gcc

all:	badge-ir-game irxmit

bline.o:	bline.c bline.h
	${CC} ${CFLAGS} -c bline.c

linuxcompat.o:	linuxcompat.c linuxcompat.h Makefile bline.h
	${CC} ${CFLAGS} ${GTKCFLAGS} -c linuxcompat.c

badge-ir-game:	badge-ir-game.c linuxcompat.o bline.o Makefile build_bug_on.h badge-ir-game-protocol.h
	${CC} ${CFLAGS} ${GTKCFLAGS} -o badge-ir-game badge-ir-game.c linuxcompat.o bline.o ${GTKLDFLAGS}

irxmit:	irxmit.c badge-ir-game-protocol.h
	${CC} ${CFLAGS} -o irxmit irxmit.c

clean:
	rm -f *.o badge-ir-game irxmit
