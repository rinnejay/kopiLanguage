CC ?= cc
CFLAGS ?= -O2 -std=c11
SRC = src/lex.c src/parse.c src/units.c src/gen_vm.c src/gen_a64.c src/gen_x64.c src/main.c
HDR = src/ast.h src/lex.h
FRAG = src/parse_stmt.inc src/parse_class.inc src/parse_expr.inc \
       src/parse_scope.inc src/parse_type.inc src/parse_resolve.inc \
       src/gen_vm_fuse.inc src/gen_vm_expr.inc \
       src/gen_a64_expr.inc src/gen_a64_rt.inc \
       src/gen_x64_expr.inc src/gen_x64_rt.inc
VMFRAG = vm/vm_core.inc vm/vm_ops_base.inc vm/vm_ops_net.inc \
         vm/vm_ops_num.inc vm/vm_ops_fast.inc

all: kopicc kopivm

kopicc: $(SRC) $(HDR) $(FRAG)
	$(CC) $(CFLAGS) -o $@ $(SRC)

kopivm: vm/kopivm.c $(VMFRAG) $(HDR)
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
