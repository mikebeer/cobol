       IDENTIFICATION DIVISION.
       PROGRAM-ID. CONDS.
       DATA DIVISION.
       WORKING-STORAGE SECTION.
       77 A PIC 99 VALUE 10.
       77 B PIC 99 VALUE 20.
       77 C PIC 99 VALUE 30.
       77 N PIC S9(3) VALUE -5.
       77 X PIC X(5) VALUE "ABC".
       77 Y PIC X(5) VALUE "12345".
       77 Z PIC X(5) VALUE "ab c".
       77 CODE-V PIC 9 VALUE 3.
          88 IS-ONE VALUE 1.
          88 IS-LOW VALUE 1 THRU 3.
          88 IS-OTHER VALUE 4 5 6.
       77 FLAG PIC X VALUE "N".
          88 FLAG-ON VALUE "Y".
          88 FLAG-OFF VALUE "N".
       PROCEDURE DIVISION.
           IF A < B DISPLAY "A<B".
           IF A NOT = B DISPLAY "A NE B".
           IF A = 10 AND B = 20 DISPLAY "AND OK".
           IF A = 1 OR B = 20 DISPLAY "OR OK".
           IF NOT A = 10 DISPLAY "wrong" ELSE DISPLAY "NOT OK".
           IF A < B AND B < C DISPLAY "CHAIN".
           IF A > 5 AND < 15 DISPLAY "ABBREV AND".
           IF A = 1 OR 10 OR 20 DISPLAY "ABBREV OR".
           IF A NOT > B DISPLAY "NOT >".
           IF (A + B) = C DISPLAY "SUM EQ".
           IF A + B > 25 DISPLAY "SUM GT".
           IF N < 0 DISPLAY "NEG".
           IF N IS NEGATIVE DISPLAY "NEGATIVE".
           IF A IS POSITIVE DISPLAY "POSITIVE".
           IF X IS ALPHABETIC DISPLAY "ALPHA".
           IF Y IS NUMERIC DISPLAY "NUMERIC".
           IF X IS NOT NUMERIC DISPLAY "X NOT NUM".
           IF Z IS ALPHABETIC-LOWER DISPLAY "LOWER".
           IF X = "ABC" DISPLAY "X EQ PADDED".
           IF X > "AB" DISPLAY "X GT AB".
           IF X EQUAL TO SPACES DISPLAY "bad".
           IF IS-LOW DISPLAY "LOW".
           IF IS-ONE DISPLAY "bad".
           IF NOT IS-OTHER DISPLAY "NOT OTHER".
           IF FLAG-OFF DISPLAY "OFF".
           SET FLAG-ON TO TRUE.
           IF FLAG-ON DISPLAY "ON" END-IF.
           DISPLAY FLAG.
           MOVE 5 TO CODE-V.
           IF IS-OTHER DISPLAY "OTHER".
           IF A = B DISPLAY "no" ELSE IF A < B DISPLAY "nested" END-IF
           END-IF.
           IF A > 100 NEXT SENTENCE ELSE DISPLAY "ELSE BRANCH".
           DISPLAY "END".
           IF A = ZERO DISPLAY "zero".
           IF X = Z DISPLAY "no".
           STOP RUN.
