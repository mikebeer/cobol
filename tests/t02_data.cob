       IDENTIFICATION DIVISION.
       PROGRAM-ID. DATA1.
       DATA DIVISION.
       WORKING-STORAGE SECTION.
       01 REC.
          05 FLD-A     PIC X(5) VALUE "ABC".
          05 FLD-N     PIC 9(5) VALUE 123.
          05 FLD-S     PIC S9(5) VALUE -42.
          05 FLD-D     PIC 9(3)V99 VALUE 12.5.
          05 FLD-P     PIC S9(5)V99 COMP-3 VALUE -123.45.
          05 FLD-B     PIC S9(4) COMP VALUE 1234.
          05 FLD-B5    PIC S9(9) COMP-5 VALUE 99999.
       01 ED1          PIC ZZ,ZZ9.99.
       01 ED2          PIC $$$,$$9.99-.
       01 ED3          PIC -(5)9.
       01 ED4          PIC 9(3)B9(3).
       01 ED5          PIC 99/99/99.
       01 ED6          PIC ***9.99.
       01 ED7          PIC +999.
       01 ED8          PIC 999CR.
       01 X5           PIC X(5).
       01 N3           PIC 999.
       01 Z1           PIC S9(3)V9 VALUE 0.
       PROCEDURE DIVISION.
           DISPLAY "A=" FLD-A "|N=" FLD-N "|S=" FLD-S.
           DISPLAY "D=" FLD-D "|P=" FLD-P "|B=" FLD-B "|B5=" FLD-B5.
           MOVE 1234.5 TO ED1. DISPLAY ED1.
           MOVE 1234.5 TO ED2. DISPLAY ED2.
           MOVE -1234.5 TO ED2. DISPLAY ED2.
           MOVE -42 TO ED3. DISPLAY ED3.
           MOVE 123456 TO ED4. DISPLAY ED4.
           MOVE 123194 TO ED5. DISPLAY ED5.
           MOVE 5.5 TO ED6. DISPLAY ED6.
           MOVE 7 TO ED7. DISPLAY ED7.
           MOVE -7 TO ED8. DISPLAY ED8.
           MOVE "HELLO WORLD" TO X5. DISPLAY X5.
           MOVE 12345 TO N3. DISPLAY N3.
           MOVE FLD-S TO N3. DISPLAY N3.
           MOVE "AB" TO X5. DISPLAY "[" X5 "]".
           MOVE 3.14159 TO Z1. DISPLAY Z1.
           MOVE ZERO TO FLD-N. DISPLAY FLD-N.
           MOVE SPACES TO X5. DISPLAY "[" X5 "]".
           MOVE ALL "*" TO X5. DISPLAY X5.
           MOVE HIGH-VALUES TO X5.
           MOVE LOW-VALUES TO X5.
           MOVE 'QQ' TO X5. DISPLAY X5.
           STOP RUN.
