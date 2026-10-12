       IDENTIFICATION DIVISION.
       PROGRAM-ID. STRS.
       DATA DIVISION.
       WORKING-STORAGE SECTION.
       77 S1 PIC X(30).
       77 S2 PIC X(10) VALUE "HELLO".
       77 S3 PIC X(10) VALUE "WORLD".
       77 PTR PIC 99.
       77 CNT PIC 99.
       77 T1 PIC X(10).
       77 T2 PIC X(10).
       77 T3 PIC X(10).
       77 DLM PIC X.
       77 TALLYV PIC 99.
       77 NUM PIC 9(5)V99.
       01 TEXT1 PIC X(20) VALUE "BANANA BREAD".
       PROCEDURE DIVISION.
           STRING S2 DELIMITED BY SPACE
                  ", " DELIMITED BY SIZE
                  S3 DELIMITED BY SPACE
                  "!" DELIMITED BY SIZE
              INTO S1.
           DISPLAY "[" S1 "]".
           MOVE SPACES TO S1.
           MOVE 3 TO PTR.
           STRING "AB" DELIMITED BY SIZE INTO S1 WITH POINTER PTR
               ON OVERFLOW DISPLAY "OVF" END-STRING.
           DISPLAY "[" S1 "] " PTR.
           MOVE SPACES TO T1.
           STRING "0123456789ABC" DELIMITED BY SIZE INTO T1
               ON OVERFLOW DISPLAY "OVERFLOW" END-STRING.
           DISPLAY T1.
           UNSTRING "A,B;C" DELIMITED BY "," OR ";"
               INTO T1 T2 T3.
           DISPLAY T1 "|" T2 "|" T3.
           UNSTRING "ONE  TWO   THREE" DELIMITED BY ALL SPACE
               INTO T1 T2 T3 COUNT IN CNT DELIMITER IN DLM.
           DISPLAY T1 "|" T2 "|" T3 "|" CNT "|" "[" DLM "]".
           MOVE 1 TO PTR.
           UNSTRING "X=12" DELIMITED BY "=" INTO T1 T2
               WITH POINTER PTR TALLYING IN TALLYV.
           DISPLAY T1 "|" T2 "|" PTR "|" TALLYV.
           MOVE 0 TO TALLYV.
           INSPECT TEXT1 TALLYING TALLYV FOR ALL "A".
           DISPLAY "A count " TALLYV.
           MOVE 0 TO TALLYV.
           INSPECT TEXT1 TALLYING TALLYV FOR CHARACTERS BEFORE " ".
           DISPLAY "chars before blank " TALLYV.
           INSPECT TEXT1 REPLACING ALL "A" BY "o".
           DISPLAY TEXT1.
           INSPECT TEXT1 REPLACING FIRST "o" BY "0".
           DISPLAY TEXT1.
           INSPECT TEXT1 CONVERTING "BNRDE" TO "bnrde".
           DISPLAY TEXT1.
           INSPECT TEXT1 REPLACING LEADING "b" BY "#".
           DISPLAY TEXT1.
           MOVE "  hello  " TO T1.
           DISPLAY "[" FUNCTION TRIM(T1) "]".
           DISPLAY "[" FUNCTION UPPER-CASE(T1) "]".
           DISPLAY FUNCTION LENGTH(T1) " " FUNCTION REVERSE("ABC").
           DISPLAY FUNCTION NUMVAL(" -12.5 ") " "
                   FUNCTION NUMVAL("  7  ").
           DISPLAY S2(2:3) "|" S2(4:).
           COMPUTE NUM = FUNCTION NUMVAL("123.45").
           DISPLAY NUM.
           DISPLAY FUNCTION CONCATENATE("AB", "CD", "EF").
           DISPLAY FUNCTION ORD("A") " " FUNCTION CHAR(66).
           DISPLAY FUNCTION LOWER-CASE("MiXeD").
           MOVE FUNCTION CURRENT-DATE(1:4) TO T1.
           IF T1 > "2000" DISPLAY "YEAR OK".
           STOP RUN.
