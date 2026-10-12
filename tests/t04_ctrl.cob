       IDENTIFICATION DIVISION.
       PROGRAM-ID. CTRL.
       DATA DIVISION.
       WORKING-STORAGE SECTION.
       77 I PIC 99.
       77 J PIC 99.
       77 K PIC 99 VALUE 0.
       77 S PIC X(8).
       77 TOT PIC 9(4) VALUE 0.
       01 GRADE PIC 999.
       PROCEDURE DIVISION.
       MAIN-PARA.
           PERFORM P-ONE.
           PERFORM P-TWO THRU P-THREE.
           PERFORM SEC-A.
           PERFORM VARYING I FROM 1 BY 1 UNTIL I > 3
               DISPLAY "I=" I
           END-PERFORM.
           PERFORM VARYING I FROM 1 BY 1 UNTIL I > 2
               AFTER J FROM 1 BY 1 UNTIL J > 2
               DISPLAY "I=" I " J=" J
           END-PERFORM.
           MOVE 0 TO K.
           PERFORM UNTIL K >= 3
               ADD 1 TO K
               IF K = 2 EXIT PERFORM CYCLE END-IF
               DISPLAY "K=" K
           END-PERFORM.
           MOVE 0 TO K.
           PERFORM WITH TEST AFTER UNTIL K > 5
               ADD 2 TO K
           END-PERFORM.
           DISPLAY "K after=" K.
           PERFORM 2 TIMES
               PERFORM P-ONE
           END-PERFORM.
           MOVE 85 TO GRADE.
           PERFORM CHECK-GRADE.
           MOVE 55 TO GRADE.
           PERFORM CHECK-GRADE.
           MOVE 100 TO GRADE.
           PERFORM CHECK-GRADE.
           MOVE 3 TO I.
           GO TO LAB1 LAB2 LAB3 DEPENDING ON I.
           DISPLAY "not here".
       LAB1.
           DISPLAY "LAB1".
       LAB2.
           DISPLAY "LAB2".
       LAB3.
           DISPLAY "LAB3".
           PERFORM LOOP-EXIT.
           DISPLAY "tot=" TOT.
           STOP RUN.
       P-ONE.
           DISPLAY "ONE".
       P-TWO.
           DISPLAY "TWO".
       P-THREE.
           DISPLAY "THREE".
       CHECK-GRADE.
           EVALUATE TRUE
               WHEN GRADE >= 90 DISPLAY "A"
               WHEN GRADE >= 80 DISPLAY "B"
               WHEN GRADE >= 60 DISPLAY "C"
               WHEN OTHER DISPLAY "F"
           END-EVALUATE.
           EVALUATE GRADE
               WHEN 100 DISPLAY "PERFECT"
               WHEN 50 THRU 59 DISPLAY "FIFTIES"
               WHEN 85 WHEN 86 DISPLAY "EIGHTY-FIVE/SIX"
               WHEN OTHER CONTINUE
           END-EVALUATE.
       LOOP-EXIT.
           PERFORM VARYING I FROM 1 BY 1 UNTIL I > 10
               IF I = 5 EXIT PERFORM END-IF
               ADD I TO TOT
           END-PERFORM.
       SEC-A SECTION.
       SA-1.
           DISPLAY "SA-1".
       SA-2.
           DISPLAY "SA-2".
