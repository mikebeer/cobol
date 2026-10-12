       IDENTIFICATION DIVISION.
       PROGRAM-ID. TAXCALC.
      * Computes a tax amount, rounded to the cent.
       DATA DIVISION.
       LINKAGE SECTION.
       01 L-AMOUNT     PIC 9(5)V99.
       01 L-RATE       PIC 9V999.
       01 L-TAX        PIC 9(5)V99.
       PROCEDURE DIVISION USING L-AMOUNT L-RATE L-TAX.
           COMPUTE L-TAX ROUNDED = L-AMOUNT * L-RATE.
           GOBACK.
