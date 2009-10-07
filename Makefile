bindir = /bin
rundir = /var/run/dh

CC = gcc
CFLAGS = -Wall -O2 -D_GNU_SOURCE
DHVERSION = $(shell git describe --tags)


dh::
	${CC} ${CFLAGS} -o dh dh.c -DDHVERSION=\"$(DHVERSION)\"

install : dh
	@bindir=${bindir} rundir=${rundir} sh Sh.install

clean :
	rm -f dh *~

.PHONY : clean install




