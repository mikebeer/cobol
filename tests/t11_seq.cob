       IDENTIFICATION DIVISION.
       PROGRAM-ID. SEQT.
       ENVIRONMENT DIVISION.
       INPUT-OUTPUT SECTION.
       FILE-CONTROL.
           SELECT SF ASSIGN TO "t11_seq.dat"
               ORGANIZATION IS SEQUENTIAL
               FILE STATUS IS FS.
           SELECT LF ASSIGN TO "t11_line.txt"
               ORGANIZATION IS LINE SEQUENTIAL.
           SELECT OPTIONAL NF ASSIGN TO "t11_nofile.dat"
               FILE STATUS IS FS.
       DATA DIVISION.
       FILE SECTION.
       FD SF.
       01 SREC.
          05 S-ID PIC 9(4).
          05 S-NAME PIC X(10).
       FD LF.
       01 LREC PIC X(40).
       FD NF.
       01 NREC PIC X(10).
       WORKING-STORAGE SECTION.
       77 FS PIC XX.
       77 EOF-FLAG PIC X VALUE "N".
          88 AT-EOF VALUE "Y".
       77 CNT PIC 99 VALUE 0.
       PROCEDURE DIVISION.
           OPEN OUTPUT SF.
           DISPLAY "open out " FS.
           MOVE 1 TO S-ID. MOVE "ALPHA" TO S-NAME.
           WRITE SREC.
           MOVE 2 TO S-ID. MOVE "BETA" TO S-NAME.
           WRITE SREC.
           MOVE 3 TO S-ID.
           WRITE SREC FROM "0003GAMMA".
           CLOSE SF.
           OPEN INPUT SF.
           PERFORM UNTIL AT-EOF
               READ SF
                   AT END SET AT-EOF TO TRUE
                   NOT AT END
                       ADD 1 TO CNT
                       DISPLAY S-ID "-" S-NAME
               END-READ
           END-PERFORM.
           DISPLAY "count " CNT " status " FS.
           CLOSE SF.
           OPEN I-O SF.
           READ SF.
           MOVE "ALPHA2" TO S-NAME.
           REWRITE SREC.
           CLOSE SF.
           OPEN INPUT SF.
           READ SF INTO LREC.
           DISPLAY LREC(1:14).
           CLOSE SF.
           OPEN OUTPUT LF.
           MOVE "first line   " TO LREC. WRITE LREC.
           MOVE "second" TO LREC. WRITE LREC.
           MOVE "x" TO LREC. WRITE LREC AFTER ADVANCING 2 LINES.
           CLOSE LF.
           OPEN INPUT LF.
           MOVE "N" TO EOF-FLAG.
           PERFORM UNTIL AT-EOF
               READ LF AT END SET AT-EOF TO TRUE
                   NOT AT END DISPLAY "[" FUNCTION TRIM(LREC) "]"
               END-READ
           END-PERFORM.
           CLOSE LF.
           OPEN INPUT NF.
           DISPLAY "optional " FS.
           READ NF AT END DISPLAY "nf at end".
           CLOSE NF.
           OPEN INPUT SF.
           CLOSE SF.
           CLOSE SF.
           DISPLAY "close again " FS.
           STOP RUN.
