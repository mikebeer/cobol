       IDENTIFICATION DIVISION.
       PROGRAM-ID. LOAN.
      * Monthly payment and amortization table for a fixed-rate loan.
       DATA DIVISION.
       WORKING-STORAGE SECTION.
       01 PRINCIPAL     PIC 9(7)V99   VALUE 12000.00.
       01 ANNUAL-RATE   PIC 9V9999    VALUE 0.0600.
       01 MONTHS        PIC 99        VALUE 12.
       01 MONTHLY-RATE  PIC 9V9(8).
       01 PAYMENT       PIC 9(7)V99.
       01 BALANCE       PIC 9(7)V99.
       01 INTEREST      PIC 9(7)V99.
       01 TO-PRINCIPAL  PIC 9(7)V99.
       01 TOTAL-INT     PIC 9(7)V99   VALUE ZERO.
       01 MONTH-NO      PIC 99.
       01 HEADING-LINE.
          05 FILLER     PIC X(12) VALUE "MO   PAYMENT".
          05 FILLER     PIC X(10) VALUE "  INTEREST".
          05 FILLER     PIC X(10) VALUE " PRINCIPAL".
          05 FILLER     PIC X(12) VALUE "     BALANCE".
       01 DETAIL-LINE.
          05 D-MONTH    PIC Z9.
          05 D-PAYMENT  PIC ZZZ,ZZ9.99.
          05 D-INTEREST PIC ZZZ,ZZ9.99.
          05 D-PRINC    PIC ZZZ,ZZ9.99.
          05 D-BALANCE  PIC Z,ZZZ,ZZ9.99.
       01 TOTAL-LINE.
          05 FILLER     PIC X(16) VALUE "TOTAL INTEREST: ".
          05 T-INTEREST PIC Z,ZZZ,ZZ9.99.
       PROCEDURE DIVISION.
       MAIN-PARA.
           COMPUTE MONTHLY-RATE = ANNUAL-RATE / 12.
           COMPUTE PAYMENT ROUNDED = PRINCIPAL * MONTHLY-RATE
               / (1 - (1 + MONTHLY-RATE) ** (0 - MONTHS)).
           DISPLAY HEADING-LINE.
           MOVE PRINCIPAL TO BALANCE.
           PERFORM VARYING MONTH-NO FROM 1 BY 1
                   UNTIL MONTH-NO > MONTHS
               COMPUTE INTEREST ROUNDED = BALANCE * MONTHLY-RATE
               IF MONTH-NO = MONTHS
                   MOVE BALANCE TO TO-PRINCIPAL
                   COMPUTE PAYMENT = BALANCE + INTEREST
               ELSE
                   COMPUTE TO-PRINCIPAL = PAYMENT - INTEREST
               END-IF
               SUBTRACT TO-PRINCIPAL FROM BALANCE
               ADD INTEREST TO TOTAL-INT
               MOVE MONTH-NO     TO D-MONTH
               MOVE PAYMENT      TO D-PAYMENT
               MOVE INTEREST     TO D-INTEREST
               MOVE TO-PRINCIPAL TO D-PRINC
               MOVE BALANCE      TO D-BALANCE
               DISPLAY DETAIL-LINE
           END-PERFORM.
           MOVE TOTAL-INT TO T-INTEREST.
           DISPLAY TOTAL-LINE.
           STOP RUN.
