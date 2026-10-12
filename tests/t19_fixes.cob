       IDENTIFICATION DIVISION.
       PROGRAM-ID. FIXES.
       ENVIRONMENT DIVISION.
       INPUT-OUTPUT SECTION.
       FILE-CONTROL.
           SELECT OUTF ASSIGN TO OUTDD.
       DATA DIVISION.
       FILE SECTION.
       FD OUTF.
       01 OUT-REC PIC X(10).
       WORKING-STORAGE SECTION.
       78 MAX-ITEMS VALUE 4.
       01 REC.
          05 MONTH-PART PIC 99 VALUE 10.
          05 DAY-PART PIC 99 VALUE 31.
       66 MD RENAMES MONTH-PART THRU DAY-PART.
       01 SZ CONSTANT AS LENGTH OF REC.
       01 TBL.
          05 EL PIC 9 OCCURS MAX-ITEMS TIMES.
       01 N PIC 9.
       01 TOT PIC 99 VALUE 0.
       01 BIG PIC 9(60).
       01 R PIC 9(60).
       01 F PIC 9(3).
       01 X PIC X(9) VALUE "  pad    ".
       PROCEDURE DIVISION.
       MAIN-PARA.
           PERFORM SHOW 2 TIMES.
           MOVE 1 TO N. *> inline comment in fixed format
           DISPLAY MD " " SZ " " MAX-ITEMS.
           MOVE 2.4 TO TOT.
           COMPUTE N ROUNDED MODE TOWARD-GREATER = 2.4.
           DISPLAY N.
           COMPUTE N ROUNDED MODE TOWARD-LESSER = 2.6.
           DISPLAY N.
           MOVE ALL "9" TO BIG.
           DIVIDE BIG BY 3 GIVING R.
           DISPLAY R.
           MOVE 5 TO F.
           PERFORM FACT-SEC.
           DISPLAY FUNCTION TRIM(X LEADING) "|".
           DISPLAY FUNCTION TRIM(X TRAILING) "|".
           OPEN OUTPUT OUTF.
           CLOSE OUTF.
           CALL "NOPE".
           STOP RUN.
       SHOW.
           ADD 1 TO TOT.
           DISPLAY "show " TOT.
       FACT-SEC SECTION.
           DISPLAY "fact " F.
           EXIT SECTION.
