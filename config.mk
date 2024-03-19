# pavc version
VERSION = 1.0.0

# Makefile configuration below

# paths
PREFIX = /usr/local
MANPREFIX = $(PREFIX)/share/man

# Pulseaudio
PAINC = /usr/include
PALIB = /usr/lib

# Uncomment and use these flags to create a debug build
# You do require google's AddressSanitizer though
# AddressSanitizer
# ASANLIB = /usr/lib
# ASANFLAGS = -fsanitize=address -fsanitize=undefined
# ASANLDFLAGS = -fsanitize=address -fsanitize=undefined

# includes and libraries
INCS = -I$(PAINC)
LIBS = -L$(PALIB) -lpulse 

# flags
CPPFLAGS = -D_POSIX_SOURCE_200809L
CFLAGS   = -std=c99 -Wpedantic -Wall -Wextra ${INCS} ${CPPFLAGS}
LDFLAGS  = ${LIBS}

# compiler and linker
CC = cc
