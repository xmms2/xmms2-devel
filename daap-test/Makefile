LIBS = `pkg-config --libs glib-2.0`
CC = gcc
CFLAGS = -Wall -g `pkg-config --cflags glib-2.0` -DDEBUG 
LDFLAGS = ${LIBS}

OBJS = client.o daap_conn.o daap_util.o cc_handlers.o daap_md5.o
BINS = mydaap

all: ${OBJS} ${BINS}

%.o: %.c %.h
	${CC} -c ${CFLAGS} -o $@ $<

mydaap: ${OBJS}
	${CC} ${CFLAGS} ${LDFLAGS} -o $@ $^

clean:
	rm -f ${BINS} ${OBJS}

dep:
	makedepend -Y -- *.c 2> /dev/null

