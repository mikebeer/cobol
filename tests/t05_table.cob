       IDENTIFICATION DIVISION.
       PROGRAM-ID. TABLES.
       DATA DIVISION.
       WORKING-STORAGE SECTION.
       01 TBL.
          05 ENT OCCURS 5 TIMES INDEXED BY IX.
             10 ENT-KEY   PIC 99.
             10 ENT-NAME  PIC X(6).
       01 GRID.
          05 ROW OCCURS 3 TIMES.
             10 CELL PIC 9 OCCURS 4 TIMES.
       01 NUMS.
          05 N PIC 99 OCCURS 4 TIMES VALUE 7.
       77 I PIC 9.
       77 J PIC 9.
       77 FOUND PIC X VALUE "N".
       77 SUMV PIC 999 VALUE 0.
       01 CNT PIC 9 VALUE 3.
       01 DYN.
          05 DE OCCURS 1 TO 5 TIMES DEPENDING ON CNT PIC 9.
       PROCEDURE DIVISION.
           PERFORM VARYING I FROM 1 BY 1 UNTIL I > 5
               COMPUTE ENT-KEY(I) = I * 10
               MOVE "ITEM-" TO ENT-NAME(I)
               MOVE I TO ENT-NAME(I)(6:1)
           END-PERFORM.
           PERFORM VARYING I FROM 1 BY 1 UNTIL I > 3
               PERFORM VARYING J FROM 1 BY 1 UNTIL J > 4
                   COMPUTE CELL(I, J) = I * J
               END-PERFORM
           END-PERFORM.
           DISPLAY ENT-KEY(3) " " ENT-NAME(3).
           DISPLAY GRID.
           DISPLAY CELL(2, 3) CELL(3, 4).
           SET IX TO 1.
           SEARCH ENT
               AT END DISPLAY "not found"
               WHEN ENT-KEY(IX) = 30 DISPLAY "found 30 " ENT-NAME(IX)
           END-SEARCH.
           SET IX TO 1.
           SEARCH ENT
               AT END DISPLAY "not found 99"
               WHEN ENT-KEY(IX) = 99 DISPLAY "found 99"
           END-SEARCH.
           SEARCH ALL ENT
               WHEN ENT-KEY(IX) = 40 DISPLAY "all 40 at " IX
           END-SEARCH.
           PERFORM VARYING IX FROM 1 BY 1 UNTIL IX > 4
               ADD N(IX) TO SUMV
           END-PERFORM.
           DISPLAY "SUM=" SUMV.
           MOVE 5 TO CNT.
           MOVE 1 TO I.
           MOVE 9 TO DE(5).
           DISPLAY "DE5=" DE(5).
           SET IX UP BY 2.
           DISPLAY "IX=" IX.
           INITIALIZE TBL.
           DISPLAY ENT-KEY(2) "/" ENT-NAME(2) "/".
           MOVE 3 TO I.
           MOVE "ABCDEF" TO ENT-NAME(I).
           DISPLAY ENT-NAME(I)(2:3).
           STOP RUN.
