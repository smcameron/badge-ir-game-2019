
CFLAGS=-pthread -fsanitize=address -Wall --pedantic -g
CC=gcc

all:	badge-ir-game irxmit

bline.o:	bline.c bline.h
	${CC} ${CFLAGS} -c bline.c

linuxcompat.o:	linuxcompat.c linuxcompat.h Makefile bline.h
	${CC} ${CFLAGS} -c linuxcompat.c

badge-ir-game:	badge-ir-game.c linuxcompat.o bline.o Makefile build_bug_on.h
	${CC} ${CFLAGS} -o badge-ir-game badge-ir-game.c linuxcompat.o bline.o

irxmit:	irxmit.c
	${CC} ${CFLAGS} -o irxmit irxmit.c

clean:
	rm -f *.o badge-ir-game irxmit
