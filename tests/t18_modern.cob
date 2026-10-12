       IDENTIFICATION DIVISION.
       PROGRAM-ID. MODERN.
       ENVIRONMENT DIVISION.
       CONFIGURATION SECTION.
       REPOSITORY.
           FUNCTION ALL INTRINSIC.
       DATA DIVISION.
       WORKING-STORAGE SECTION.
       01 X PIC X(10) VALUE "abc".
       01 Y PIC X(10).
       01 N PIC 9(4).
       01 BIG CONSTANT 100.
       01 C2 CONSTANT AS 7.
       78 C3 VALUE 9.
       01 BC USAGE BINARY-CHAR UNSIGNED.
       01 FS USAGE FLOAT-SHORT.
       01 FL USAGE FLOAT-LONG.
       01 BITS PIC 1(8) VALUE B"10100101".
       01 TBL.
          05 EL PIC 99 OCCURS 5 TIMES.
       01 PTR1 USAGE POINTER.
       01 TX PIC X(30).
       PROCEDURE DIVISION.
           MOVE UPPER-CASE(X) TO Y.
           DISPLAY Y.
           COMPUTE N = MAX(1, 5, 3) + BIG + C2 + C3.
           DISPLAY N.
           MOVE 255 TO BC.
           ADD 1 TO BC.
           DISPLAY BC.
           MOVE 1.5 TO FS.
           COMPUTE FL = FS * 2.
           DISPLAY FL.
           DISPLAY BITS.
           PERFORM VARYING N FROM 1 BY 1 UNTIL N > 5
               MOVE N TO EL(N)
           END-PERFORM.
           MOVE 0 TO N.
           PERFORM 10 TIMES
               ADD 1 TO N
               IF N = 3 EXIT PERFORM CYCLE END-IF
               IF N > 6 EXIT PERFORM END-IF
           END-PERFORM.
           DISPLAY N.
           INSPECT X CONVERTING "abc" TO "xyz".
           DISPLAY X.
           ACCEPT TX FROM ENVIRONMENT "HOME".
           SORT EL ON DESCENDING KEY EL.
           DISPLAY EL(1) EL(2) EL(5).
           COMPUTE N ROUNDED MODE NEAREST-EVEN = 2.5.
           DISPLAY N.
           EVALUATE TRUE ALSO N
               WHEN N > 1 ALSO 2 DISPLAY "even"
               WHEN OTHER DISPLAY "other"
           END-EVALUATE.
           DISPLAY "to stderr" UPON SYSERR.
           STOP RUN.
