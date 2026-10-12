       IDENTIFICATION DIVISION.
       PROGRAM-ID. MAIN2.
       DATA DIVISION.
       WORKING-STORAGE SECTION.
       77 X PIC 999 VALUE 5.
       77 R PIC S9(4) COMP-5.
       77 BUF PIC X(12) VALUE "hello".
       PROCEDURE DIVISION.
           CALL "SUBA" USING X.
           DISPLAY X.
           CALL "SUBB" USING X.
           DISPLAY X.
           CALL "DYNSUB" USING X.
           DISPLAY X.
           CALL "cadd" USING BY VALUE 20 BY VALUE 22 RETURNING R.
           DISPLAY R.
           CALL "clen" USING BUF RETURNING R.
           DISPLAY R.
           CALL "nothere" ON EXCEPTION DISPLAY "no such".
           STOP RUN.
