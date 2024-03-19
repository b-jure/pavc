# pavc - pulse audio volume control
# See any of the source files (pavc.c, util.c, util.h) for
# copyright and licence details.

include config.mk

SRC = pavc.c util.c
OBJ = ${SRC:.c=.o}

all: options pavc

options:
	@echo pavc build options:
	@echo "CFLAGS  = ${CFLAGS}"
	@echo "LDFLAGS = ${LDFLAGS}"
	@echo "CC      = ${CC}"

.c.o:
	${CC} -c ${CFLAGS} $<

${OBJ}: config.mk

pavc: ${OBJ}
	${CC} -o $@ ${OBJ} ${LDFLAGS}

clean:
	rm -f pavc ${OBJ} pavc-${VERSION}.tar.gz

dist: clean
	mkdir -p pavc-${VERSION}
	cp -r COPYING Makefile README config.mk \
		pavc.1 util.h ${SRC} pavc-${VERSION}
	tar -cf pavc-${VERSION}.tar pavc-${VERSION}
	gzip pavc-${VERSION}.tar
	rm -rf pavc-${VERSION}

install: all
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f pavc ${DESTDIR}${PREFIX}/bin
	chmod 755 ${DESTDIR}${PREFIX}/bin/pavc
	mkdir -p ${DESTDIR}${MANPREFIX}/man1
	sed "s/VERSION/${VERSION}/g" < pavc.1 | gzip > ${DESTDIR}${MANPREFIX}/man1/pavc.1.gz
	chmod 644 ${DESTDIR}${MANPREFIX}/man1/pavc.1.gz

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/pavc\
		${DESTDIR}${MANPREFIX}/man1/pavc.1.gz

.PHONY: all options clean dist install unistall
