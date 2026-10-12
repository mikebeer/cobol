       IDENTIFICATION DIVISION.
       PROGRAM-ID. REPORT1.
      * Reads a line sequential customer file and prints a report
      * with a page heading, one line per customer and a total.
       ENVIRONMENT DIVISION.
       INPUT-OUTPUT SECTION.
       FILE-CONTROL.
           SELECT CUST-FILE ASSIGN TO "customers.dat"
               ORGANIZATION IS LINE SEQUENTIAL
               FILE STATUS IS CUST-STATUS.
           SELECT REPORT-FILE ASSIGN TO "report.txt"
               ORGANIZATION IS LINE SEQUENTIAL.
       DATA DIVISION.
       FILE SECTION.
       FD CUST-FILE.
       01 CUST-REC.
          05 CUST-NO      PIC 9(4).
          05 CUST-NAME    PIC X(18).
          05 CUST-BALANCE PIC 9(5)V99.
       FD REPORT-FILE.
       01 REPORT-LINE     PIC X(40).
       WORKING-STORAGE SECTION.
       01 CUST-STATUS     PIC XX.
       01 END-FLAG        PIC X       VALUE "N".
          88 AT-END                   VALUE "Y".
       01 GRAND-TOTAL     PIC 9(7)V99 VALUE ZERO.
       01 COUNT-READ      PIC 999     VALUE ZERO.
       01 HEAD-1          PIC X(40)
                          VALUE "CUSTOMER BALANCES".
       01 HEAD-2          PIC X(40)
                          VALUE "NO.   NAME                BALANCE".
       01 DETAIL.
          05 DET-NO       PIC 9(4).
          05 FILLER       PIC XX      VALUE SPACES.
          05 DET-NAME     PIC X(18).
          05 DET-BALANCE  PIC ZZ,ZZ9.99.
          05 FILLER       PIC X(3)    VALUE SPACES.
          05 DET-FLAG     PIC X(5).
       01 TOTAL-LINE.
          05 FILLER       PIC X(21)   VALUE "TOTAL".
          05 TOT-BALANCE  PIC Z,ZZZ,ZZ9.99.
       PROCEDURE DIVISION.
       MAIN-PARA.
           OPEN INPUT CUST-FILE OUTPUT REPORT-FILE.
           IF CUST-STATUS NOT = "00"
               DISPLAY "cannot open customers.dat, status " CUST-STATUS
               STOP RUN
           END-IF.
           WRITE REPORT-LINE FROM HEAD-1.
           WRITE REPORT-LINE FROM HEAD-2.
           PERFORM READ-CUSTOMER.
           PERFORM UNTIL AT-END
               PERFORM PRINT-CUSTOMER
               PERFORM READ-CUSTOMER
           END-PERFORM.
           MOVE GRAND-TOTAL TO TOT-BALANCE.
           WRITE REPORT-LINE FROM TOTAL-LINE.
           CLOSE CUST-FILE REPORT-FILE.
           DISPLAY COUNT-READ " customers listed".
           STOP RUN.
       READ-CUSTOMER.
           READ CUST-FILE
               AT END SET AT-END TO TRUE
           END-READ.
       PRINT-CUSTOMER.
           ADD 1 TO COUNT-READ.
           ADD CUST-BALANCE TO GRAND-TOTAL.
           MOVE CUST-NO TO DET-NO.
           MOVE CUST-NAME TO DET-NAME.
           MOVE CUST-BALANCE TO DET-BALANCE.
           IF CUST-BALANCE = ZERO
               MOVE "PAID" TO DET-FLAG
           ELSE
               MOVE SPACES TO DET-FLAG
           END-IF.
           WRITE REPORT-LINE FROM DETAIL.
