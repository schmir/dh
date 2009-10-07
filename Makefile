bindir = /bin
rundir = /var/run/dh

CC = gcc
CFLAGS = -Wall -O2 -D_GNU_SOURCE
DHVERSION = $(shell git describe --tags)


dh::
	${CC} ${CFLAGS} -o dh dh.c -DDHVERSION=\"$(DHVERSION)\"

install:: dh
	install -m 1777 -d ${rundir}
	install -m 755 -d ${bindir}
	install -m 755 -p -s dh ${bindir}

clean:
	rm -f dh *~

.PHONY : clean install
