# cobol

A COBOL to C89 translator with its own runtime (exact decimal
arithmetic, no 64-bit integers needed). Builds with GCC on VM/370plus
CMS (source lines <= 71 columns) and on Linux/Windows.

    make            # builds ./cobol
    make test       # runs tests/run.sh
    ./cobol -x -o hello hello.cob

Status: phase 1 (COBOL-85 core: data, arithmetic, control flow,
tables, strings, CALL/LINKAGE, COPY/REPLACE, free format).
Files, SORT/MERGE, declaratives and OO are planned.
