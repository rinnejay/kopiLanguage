CC ?= cc
CFLAGS ?= -O2 -std=c11
SRC = src/lex.c src/parse.c src/units.c src/gen_vm.c src/gen_a64.c src/gen_x64.c src/main.c

all: kopicc kopivm

kopicc: $(SRC)
	$(CC) $(CFLAGS) -o $@ $(SRC)

kopivm: vm/kopivm.c
	$(CC) $(CFLAGS) -I src -o $@ vm/kopivm.c

clean:
	rm -f kopicc kopivm out.s out.kbc

test: all
	sh test.sh

.PHONY: all clean test
