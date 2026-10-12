# COBOL to C89 translator, portable C89.
CC      ?= cc
CFLAGS  ?= -O2 -std=c89 -pedantic -Wall -Wextra
LIBS    = -lm
PARTS   = src/rt00_const.h src/c00_defs.h src/c10_util.c src/c20_lex.c src/c25_copy.c \
          src/c28_par.c src/c30_pic.c src/c32_data.c src/c40_expr.c \
          src/c50_stmt.c src/c52_more.c src/c55_io.c src/c58_proc.c \
          src/c70_main.c
RTPARTS = src/rt0_head.h src/rt00_const.h src/rt1_dec.h src/rt2_fld.h src/rt3_core.h \
          src/rt4_misc.h src/rt5_str.h src/rt6_fn.h src/rt6b_fn.h src/rt7_file.h

all: cobol src/cobrt.h

cobol: cobol_all.c
	$(CC) $(CFLAGS) -o $@ cobol_all.c $(LIBS)

cobol_all.c: $(PARTS) tools/amalg.sh
	sh tools/amalg.sh > cobol_all.c

src/cobrt.h: $(RTPARTS) src/c40_expr.c tools/mkrt.sh
	sh tools/mkrt.sh > src/cobrt.h

test: all
	COBOL=./cobol sh tests/run.sh

lines:
	sh tools/linelen.sh src/*.c src/*.h tools/*.sh tests/*.cob \
	   cobol_all.c

clean:
	rm -f cobol cobol_all.c src/cobrt.h
