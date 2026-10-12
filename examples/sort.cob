       IDENTIFICATION DIVISION.
       PROGRAM-ID. SORTDEMO.
      * SORT with an input procedure that releases records built in
      * storage and an output procedure that prints a total for each
      * region (a control break).
       ENVIRONMENT DIVISION.
       INPUT-OUTPUT SECTION.
       FILE-CONTROL.
           SELECT SORT-FILE ASSIGN TO "sortwork".
       DATA DIVISION.
       FILE SECTION.
       SD SORT-FILE.
       01 SORT-REC.
          05 S-REGION    PIC X(5).
          05 S-AMOUNT    PIC 9(5)V99.
       WORKING-STORAGE SECTION.
       01 SALES-DATA.
          05 FILLER PIC X(12) VALUE "NORTH0012050".
          05 FILLER PIC X(12) VALUE "SOUTH0004000".
          05 FILLER PIC X(12) VALUE "NORTH0030000".
          05 FILLER PIC X(12) VALUE "EAST 0007575".
          05 FILLER PIC X(12) VALUE "SOUTH0010025".
          05 FILLER PIC X(12) VALUE "NORTH0001500".
       01 SALES-TABLE REDEFINES SALES-DATA.
          05 SALE OCCURS 6 TIMES.
             10 SALE-REGION PIC X(5).
             10 SALE-AMOUNT PIC 9(5)V99.
       01 I               PIC 9.
       01 PREV-REGION     PIC X(5) VALUE SPACES.
       01 REGION-TOTAL    PIC 9(6)V99 VALUE ZERO.
       01 SHOW-AMOUNT     PIC ZZ,ZZ9.99.
       01 EOF-FLAG        PIC X VALUE "N".
          88 NO-MORE      VALUE "Y".
       PROCEDURE DIVISION.
       MAIN-PARA.
           SORT SORT-FILE
               ON ASCENDING KEY S-REGION
               ON DESCENDING KEY S-AMOUNT
               INPUT PROCEDURE IS LOAD-SALES
               OUTPUT PROCEDURE IS PRINT-SALES.
           STOP RUN.
       LOAD-SALES SECTION.
           PERFORM VARYING I FROM 1 BY 1 UNTIL I > 6
               MOVE SALE-REGION (I) TO S-REGION
               MOVE SALE-AMOUNT (I) TO S-AMOUNT
               RELEASE SORT-REC
           END-PERFORM.
       PRINT-SALES SECTION.
           PERFORM UNTIL NO-MORE
               RETURN SORT-FILE
                   AT END SET NO-MORE TO TRUE
               END-RETURN
               IF NOT NO-MORE
                   IF S-REGION NOT = PREV-REGION
                       PERFORM REGION-BREAK
                       MOVE S-REGION TO PREV-REGION
                   END-IF
                   MOVE S-AMOUNT TO SHOW-AMOUNT
                   DISPLAY "  " S-REGION SHOW-AMOUNT
                   ADD S-AMOUNT TO REGION-TOTAL
               END-IF
           END-PERFORM.
           PERFORM REGION-BREAK.
       REGION-BREAK SECTION.
           IF PREV-REGION NOT = SPACES
               MOVE REGION-TOTAL TO SHOW-AMOUNT
               DISPLAY "  total" SHOW-AMOUNT
           END-IF.
           MOVE ZERO TO REGION-TOTAL.
