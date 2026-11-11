CC ?= cc
CFLAGS ?= -O2 -std=c11
SRC = src/lex.c src/parse.c src/units.c src/gen_vm.c src/gen_a64.c src/gen_x64.c src/main.c
HDR = src/ast.h src/lex.h

all: kopicc kopivm

kopicc: $(SRC) $(HDR)
	$(CC) $(CFLAGS) -o $@ $(SRC)

kopivm: vm/kopivm.c $(HDR)
	$(CC) $(CFLAGS) -I src -o $@ vm/kopivm.c

clean:
	rm -f kopicc kopivm out.s out.kbc

test: all
	sh test.sh

docs:
	python3 tools/gen_libref.py
	python3 tools/md2html.py
	python3 tools/check_docs.py

.PHONY: all clean test docs
