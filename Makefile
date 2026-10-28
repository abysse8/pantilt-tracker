CC      ?= gcc
CFLAGS  ?= -std=c11 -O2 -Wall -Wextra -g
CFLAGS  += -D_DEFAULT_SOURCE -Iinclude
LDLIBS  = -lm

LIB_SRC = src/frame.c src/vision.c src/servo.c src/control.c src/config.c src/log.c
LIB_OBJ = $(LIB_SRC:src/%.c=build/%.o)

all: bin/tracker bin/gen_frames bin/test_control

bin/tracker: build/main.o $(LIB_OBJ) | bin
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

bin/test_control: tests/test_control.c $(LIB_OBJ) | bin
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

bin/gen_frames: tests/gen_frames.c | bin
	$(CC) $(CFLAGS) -o $@ $< $(LDLIBS)

build/%.o: src/%.c $(wildcard include/*.h) | build
	$(CC) $(CFLAGS) -c -o $@ $<

data/cercle.yuv: bin/gen_frames
	./bin/gen_frames 640 480 90 > $@ 2> data/cercle_attendu.csv

test: bin/test_control bin/tracker data/cercle.yuv
	./bin/test_control
	./bin/tracker -n < data/cercle.yuv

bin build:
	mkdir -p $@

clean:
	rm -rf bin build data/cercle.yuv data/cercle_attendu.csv

.PHONY: all test clean
