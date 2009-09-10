bindir = /usr/local/bin
rundir = /var/run/dh

CC = gcc
CFLAGS = -Wall -O2

dh : dh.c
	${CC} ${CFLAGS} -o dh dh.c

install : dh
	@bindir=${bindir} rundir=${rundir} sh Sh.install

clean :
	rm -f dh *~

.PHONY : clean install




