       IDENTIFICATION DIVISION.
       PROGRAM-ID. CUSTMAINT.
      * Maintenance of an indexed file of customers with an alternate
      * key: add, change, delete, look up by key, list by name.
       ENVIRONMENT DIVISION.
       INPUT-OUTPUT SECTION.
       FILE-CONTROL.
           SELECT CUST ASSIGN TO "cust.idx"
               ORGANIZATION IS INDEXED
               ACCESS MODE IS DYNAMIC
               RECORD KEY IS C-ID
               ALTERNATE RECORD KEY IS C-NAME WITH DUPLICATES
               FILE STATUS IS ST.
       DATA DIVISION.
       FILE SECTION.
       FD CUST.
       01 C-REC.
          05 C-ID        PIC 9(4).
          05 C-NAME      PIC X(12).
          05 C-CITY      PIC X(10).
       WORKING-STORAGE SECTION.
       01 ST             PIC XX.
       01 MORE           PIC X.
          88 NO-MORE     VALUE "N".
       PROCEDURE DIVISION.
       MAIN-PARA.
           OPEN OUTPUT CUST.
           MOVE 1003 TO C-ID. MOVE "SMITH" TO C-NAME.
           MOVE "BERN" TO C-CITY. PERFORM ADD-REC.
           MOVE 1001 TO C-ID. MOVE "JONES" TO C-NAME.
           MOVE "ZURICH" TO C-CITY. PERFORM ADD-REC.
           MOVE 1002 TO C-ID. MOVE "SMITH" TO C-NAME.
           MOVE "BASEL" TO C-CITY. PERFORM ADD-REC.
           MOVE 1004 TO C-ID. MOVE "TEMP" TO C-NAME.
           MOVE "LUGANO" TO C-CITY. PERFORM ADD-REC.
           MOVE 1002 TO C-ID. MOVE "DUPLICATE" TO C-NAME.
           PERFORM ADD-REC.
           CLOSE CUST.
           OPEN I-O CUST.
           MOVE 1001 TO C-ID.
           READ CUST
               INVALID KEY DISPLAY "no customer 1001"
               NOT INVALID KEY
                   MOVE "GENEVA" TO C-CITY
                   REWRITE C-REC
                   DISPLAY "changed 1001"
           END-READ.
           MOVE 1004 TO C-ID.
           DELETE CUST
               INVALID KEY DISPLAY "cannot delete 1004"
           END-DELETE.
           MOVE 9999 TO C-ID.
           READ CUST INVALID KEY DISPLAY "no customer 9999" END-READ.
           DISPLAY "-- by key".
           MOVE LOW-VALUES TO C-ID.
           START CUST KEY IS NOT LESS THAN C-ID.
           PERFORM LIST-NEXT.
           DISPLAY "-- by name".
           MOVE "A" TO C-NAME.
           START CUST KEY IS NOT LESS THAN C-NAME.
           PERFORM LIST-NEXT.
           CLOSE CUST.
           STOP RUN.
       ADD-REC.
           WRITE C-REC
               INVALID KEY DISPLAY "duplicate key " C-ID " status " ST
           END-WRITE.
       LIST-NEXT.
           MOVE "Y" TO MORE.
           PERFORM UNTIL NO-MORE
               READ CUST NEXT
                   AT END MOVE "N" TO MORE
                   NOT AT END DISPLAY C-ID " " C-NAME " " C-CITY
               END-READ
           END-PERFORM.
