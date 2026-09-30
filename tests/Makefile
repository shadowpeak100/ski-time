PURE = $(wildcard ../src/c/geom.c ../src/c/glyphs.c ../src/c/layout.c ../src/c/skier.c ../src/c/avalanche.c ../src/c/saver.c ../src/c/daypart.c)
SRCS = host_tests.c trig_host.c $(PURE)
CFLAGS = -std=c99 -Wall -Werror -O0 -g -DHOST_TEST -I../src/c -I.

test: host_tests
	./host_tests

host_tests: $(SRCS) $(wildcard ../src/c/*.h) trig_shim.h
	$(CC) $(CFLAGS) -o $@ $(SRCS) -lm

clean:
	rm -f host_tests

.PHONY: test clean
