# An admittedly primitive Makefile
# To compile, type "make" or make "all"
# To remove files, type "make clean"

CC := gcc
CFLAGS := -Wall -g -O0 -fsanitize=address -fno-omit-frame-pointer -Iinclude
SRC_DIR := src
BIN_DIR := bin
BUILD_DIR := build
OBJS := ${BUILD_DIR}/wserver.o \
				${BUILD_DIR}/wclient.o \
				${BUILD_DIR}/request.o \
				${BUILD_DIR}/io_helper.o 

all: ${BIN_DIR}/wserver ${BIN_DIR}/wclient ${BIN_DIR}/spin.cgi

${BIN_DIR}/wserver: ${BUILD_DIR}/wserver.o ${BUILD_DIR}/request.o ${BUILD_DIR}/io_helper.o
	$(CC) $(CFLAGS) -o ${BIN_DIR}/wserver \
		${BUILD_DIR}/wserver.o \
		${BUILD_DIR}/request.o \
		${BUILD_DIR}/io_helper.o 

${BIN_DIR}/wclient: ${BUILD_DIR}/wclient.o ${BUILD_DIR}/io_helper.o
	$(CC) $(CFLAGS) -o \
		${BIN_DIR}/wclient \
		${BUILD_DIR}/wclient.o \
		${BUILD_DIR}/io_helper.o

${BIN_DIR}/spin.cgi: ${SRC_DIR}/spin.c
	$(CC) $(CFLAGS) -o \
		${BIN_DIR}/spin.cgi \
		${SRC_DIR}/spin.c

${BUILD_DIR}/%.o: ${SRC_DIR}/%.c
	$(CC) $(CFLAGS) -o $@ -c $<

debug: all
	gdb -q -nx -x .gdbinit

test: all

clean:
	-rm -f $(OBJS) \
		${BIN_DIR}/wserver \
		${BIN_DIR}/wclient \
		${BIN_DIR}/spin.cgi
