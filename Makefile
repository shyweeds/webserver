# An admittedly primitive Makefile
# To compile, type "make" or make "all"
# To remove files, type "make clean"

CC := gcc
CFLAGS := -Wall -g -O0 -fsanitize=address -fno-omit-frame-pointer -Iinclude
SRC_DIR := src
BIN_DIR := bin
BUILD_DIR := build
BASEDIR := bin/basedir
OBJS := ${BUILD_DIR}/wserver.o \
				${BUILD_DIR}/wclient.o \
				${BUILD_DIR}/request.o \
				${BUILD_DIR}/io_helper.o 

all: ${BIN_DIR}/wserver ${BIN_DIR}/wclient ${BASEDIR}/spin.cgi

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

${BASEDIR}/spin.cgi: ${SRC_DIR}/spin.c | ${BASEDIR}
	$(CC) $(CFLAGS) -o \
		${BASEDIR}/spin.cgi \
		${SRC_DIR}/spin.c

${BUILD_DIR}/%.o: ${SRC_DIR}/%.c | ${BUILD_DIR}
	$(CC) $(CFLAGS) -o $@ -c $<

${BUILD_DIR} ${BIN_DIR} ${BASEDIR}:
	mkdir -p $@

debug: all
	gdb -q -nx -x ./bin/.gdbinit

test: all
	tests/ci.sh

clean:
	-rm -f $(OBJS) \
		${BIN_DIR}/wserver \
		${BIN_DIR}/wclient \
		${BASEDIR}/spin.cgi
