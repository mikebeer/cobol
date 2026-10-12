# cobol

A COBOL to C89 translator with its own runtime. The runtime does exact
decimal arithmetic without 64-bit integers, so the generated programs
build with GCC on VM/370plus CMS (source lines are at most 71 columns),
and on Linux and Windows. A TinyCC can be bundled with the Windows
packages, so no other compiler is needed there.

    make                        # builds ./cobol and src/cobrt.h
    make test                   # runs tests/run.sh (20 tests)
    ./cobol -c hello.cob -o hello && ./hello

`cobol prog.cob -o prog.c` only translates; `-c` also builds the
program with gcc, cc, clang or tcc; `-M` builds a module that `CALL`
loads at run time (directories in `COB_LIBRARY_PATH`).

## Language

* COBOL-85 core: all data descriptions (DISPLAY, COMP, COMP-1/2/3,
  COMP-5, INDEX, POINTER), REDEFINES, RENAMES, OCCURS (DEPENDING ON),
  level 88, MOVE/arithmetic with ROUNDED (including MODE) and
  SIZE ERROR, STRING/UNSTRING/INSPECT, EVALUATE, SEARCH, PERFORM
  (all forms), CALL/LINKAGE (BY REFERENCE/CONTENT/VALUE), nested and
  RECURSIVE programs, COPY/REPLACE, fixed and free format.
* Files: sequential, line sequential, relative and indexed (alternate
  keys), READ/WRITE/REWRITE/DELETE/START, FILE STATUS, USE
  declaratives, SORT and MERGE (files and tables).
* About 150 intrinsic functions, user-defined FUNCTION-ID.
* Object orientation: CLASS-ID, FACTORY, OBJECT, METHOD-ID, INVOKE,
  INHERITS, INTERFACE, NEW, SELF/SUPER.
* Separate compilation, dynamic CALL of modules, calls of C functions.

Not implemented: REPORT SECTION, SCREEN SECTION, ALTER, JSON/XML
statements. See chapter 15 of the manual for the full list.

## Documentation

`docs/COBOL-User-Guide.docx` (and `.pdf`) is the manual (81 pages);
`make -C docs` is not needed, use `sh docs/make_manual.sh` to rebuild
(needs node with `docx`, LibreOffice and poppler). Examples are in
`examples/`.

## Layout

* `src/` translator parts (`c*.c`) and runtime parts (`rt*.h`);
  `tools/amalg.sh` joins the translator into `cobol_all.c` and
  `tools/mkrt.sh` joins the runtime into `src/cobrt.h`, which every
  generated program includes. Both joined files are build products.
* `tests/` regression tests (`t*.cob` with `.out`); `sh tests/run.sh`.
* `make lines` checks that sources stay within 71 columns.

## Packages

`make dist` makes the Windows zips (32 and 64 bit, with TinyCC after
`sh tools/build-tcc.sh`), a static Linux tarball, rpm, deb and the
source tarball in `dist/`. `sh tools/wintest.sh x86_64` runs the tests
under Wine with MinGW.

## CMS

The translator is the single file `cobol_all.c` (lines of at most 71
columns) and the generated C also keeps to 71 columns, long literals
being split. Build with GCC for VM/370plus: compile `cobol_all.c`,
put `cobrt.h` where the generated programs can include it. This has
not been run on a real CMS system here; only the column limit and
strict C89 are checked.
