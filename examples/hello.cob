       IDENTIFICATION DIVISION.
       PROGRAM-ID. HELLO.
      * The smallest useful program: a greeting and a little arithmetic.
       DATA DIVISION.
       WORKING-STORAGE SECTION.
       01 WHO       PIC X(12)  VALUE "WORLD".
       01 N1        PIC 9(3)   VALUE 17.
       01 N2        PIC 9(3)   VALUE 25.
       01 TOTAL     PIC 9(4).
       PROCEDURE DIVISION.
       MAIN-PARA.
           DISPLAY "Hello, " FUNCTION TRIM(WHO) "!".
           ADD N1 N2 GIVING TOTAL.
           DISPLAY N1 " + " N2 " = " TOTAL.
           IF TOTAL > 40
               DISPLAY "that is more than forty"
           ELSE
               DISPLAY "that is forty or less"
           END-IF.
           STOP RUN.
