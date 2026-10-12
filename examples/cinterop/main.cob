       IDENTIFICATION DIVISION.
       PROGRAM-ID. CALLC.
      * Calls the C functions in cfn.c.
       DATA DIVISION.
       WORKING-STORAGE SECTION.
       01 A            PIC S9(4) COMP-5 VALUE 84.
       01 B            PIC S9(4) COMP-5 VALUE 36.
       01 RESULT       PIC S9(9) COMP-5.
       01 TEXT-1       PIC X(20) VALUE "mixed Case text".
       01 LEN          PIC S9(9) COMP-5.
       PROCEDURE DIVISION.
           CALL "c_gcd" USING BY VALUE A B
               RETURNING RESULT.
           DISPLAY "gcd(84, 36) = " RESULT.
           MOVE 20 TO LEN.
           CALL "c_upcase" USING TEXT-1 BY VALUE LEN
               RETURNING RESULT.
           DISPLAY TEXT-1 " (" RESULT " letters changed)".
           CALL "c_trimlen" USING TEXT-1 BY VALUE LEN
               RETURNING RESULT.
           DISPLAY "length without trailing blanks: " RESULT.
           MOVE ZERO TO RETURN-CODE.
           STOP RUN.
