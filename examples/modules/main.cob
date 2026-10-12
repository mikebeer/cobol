       IDENTIFICATION DIVISION.
       PROGRAM-ID. SHOP.
      * The main program calls the subprogram TAXCALC.
       DATA DIVISION.
       WORKING-STORAGE SECTION.
       01 PRICE        PIC 9(5)V99 VALUE 250.00.
       01 TAX          PIC 9(5)V99.
       01 TAX-RATE     PIC 9V999   VALUE 0.077.
       01 SHOWN        PIC ZZ,ZZ9.99.
       PROCEDURE DIVISION.
           CALL "TAXCALC" USING PRICE TAX-RATE TAX.
           MOVE TAX TO SHOWN.
           DISPLAY "tax on 250.00 at 7.7 percent: " SHOWN.
           STOP RUN.
