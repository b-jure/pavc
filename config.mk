# pavc version
VERSION = 1.1.0


# paths
PREFIX = /usr/local
MANPREFIX = ${PREFIX}/share/man


# Pulseaudio
PAINC = /usr/include
PALIB = /usr/lib


# Address sanitizer
#ASANFLAGS = -fsanitize=address -fsanitize=undefined


# debug vars
#DBGDEFS = -DPAVC_ASSERT
#DBGFLAGS = -g


# enables optimizations
OPTS = -O2


# includes and libraries
INCS = -I${PAINC}
LIBS = -L${PALIB} -lpulse ${ASANFLAGS}


# compiler and linker flags
CPPFLAGS = -D_POSIX_SOURCE_200809L
CFLAGS   = -std=c99 -Wpedantic -Wall -Wextra ${OPTS} ${DBGDEFS} ${DBGFLAGS} \
	   ${INCS} ${CPPFLAGS} ${ASANFLAGS}
LDFLAGS  = ${LIBS}


# compiler and linker
CC = gcc
