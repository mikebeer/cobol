       IDENTIFICATION DIVISION.
       PROGRAM-ID. MAINR.
       DATA DIVISION.
       WORKING-STORAGE SECTION.
       77 N PIC 99 VALUE 6.
       77 RES PIC 9(8) VALUE 0.
       PROCEDURE DIVISION.
           CALL "FACT" USING N RES.
           DISPLAY "6! = " RES.
           STOP RUN.
       IDENTIFICATION DIVISION.
       PROGRAM-ID. FACT RECURSIVE.
       DATA DIVISION.
       LOCAL-STORAGE SECTION.
       77 M PIC 99.
       77 SUB-RES PIC 9(8).
       LINKAGE SECTION.
       77 K PIC 99.
       77 OUT-R PIC 9(8).
       PROCEDURE DIVISION USING K OUT-R.
           IF K <= 1
               MOVE 1 TO OUT-R
           ELSE
               COMPUTE M = K - 1
               CALL "FACT" USING M SUB-RES
               COMPUTE OUT-R = K * SUB-RES
           END-IF.
           GOBACK.
       END PROGRAM FACT.
