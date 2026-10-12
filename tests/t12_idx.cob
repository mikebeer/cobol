       IDENTIFICATION DIVISION.
       PROGRAM-ID. IDXT.
       ENVIRONMENT DIVISION.
       INPUT-OUTPUT SECTION.
       FILE-CONTROL.
           SELECT IXF ASSIGN TO "t12_idx.dat"
               ORGANIZATION IS INDEXED
               ACCESS MODE IS DYNAMIC
               RECORD KEY IS IX-KEY
               ALTERNATE RECORD KEY IS IX-ALT WITH DUPLICATES
               FILE STATUS IS FS.
           SELECT RF ASSIGN TO "t12_rel.dat"
               ORGANIZATION IS RELATIVE
               ACCESS MODE IS DYNAMIC
               RELATIVE KEY IS RK
               FILE STATUS IS FS.
       DATA DIVISION.
       FILE SECTION.
       FD IXF.
       01 IX-REC.
          05 IX-KEY PIC X(5).
          05 IX-ALT PIC X(3).
          05 IX-DATA PIC X(10).
       FD RF.
       01 RF-REC PIC X(12).
       WORKING-STORAGE SECTION.
       77 FS PIC XX.
       77 RK PIC 9(4).
       77 EOF-FLAG PIC X VALUE "N".
          88 AT-EOF VALUE "Y".
       PROCEDURE DIVISION.
           OPEN OUTPUT IXF.
           MOVE "K0003" TO IX-KEY. MOVE "AAA" TO IX-ALT.
           MOVE "third" TO IX-DATA. WRITE IX-REC.
           MOVE "K0001" TO IX-KEY. MOVE "BBB" TO IX-ALT.
           MOVE "first" TO IX-DATA. WRITE IX-REC.
           MOVE "K0002" TO IX-KEY. MOVE "AAA" TO IX-ALT.
           MOVE "second" TO IX-DATA. WRITE IX-REC.
           MOVE "K0002" TO IX-KEY. MOVE "ZZZ" TO IX-ALT.
           WRITE IX-REC
               INVALID KEY DISPLAY "dup key " FS.
           CLOSE IXF.
           OPEN I-O IXF.
           MOVE LOW-VALUES TO IX-KEY.
           START IXF KEY IS NOT LESS THAN IX-KEY
               INVALID KEY DISPLAY "start failed".
           PERFORM UNTIL AT-EOF
               READ IXF NEXT
                   AT END SET AT-EOF TO TRUE
                   NOT AT END DISPLAY IX-KEY " " IX-ALT " " IX-DATA
               END-READ
           END-PERFORM.
           MOVE "K0002" TO IX-KEY.
           READ IXF INVALID KEY DISPLAY "not found".
           DISPLAY "random " IX-DATA.
           MOVE "changed" TO IX-DATA.
           REWRITE IX-REC.
           MOVE "K9999" TO IX-KEY.
           READ IXF INVALID KEY DISPLAY "K9999 not found " FS.
           MOVE "K0001" TO IX-KEY.
           DELETE IXF INVALID KEY DISPLAY "del failed".
           MOVE "AAA" TO IX-ALT.
           START IXF KEY IS = IX-ALT INVALID KEY DISPLAY "no alt".
           MOVE "N" TO EOF-FLAG.
           PERFORM UNTIL AT-EOF
               READ IXF NEXT
                   AT END SET AT-EOF TO TRUE
                   NOT AT END DISPLAY "alt: " IX-KEY " " IX-ALT " "
                       IX-DATA
               END-READ
           END-PERFORM.
           CLOSE IXF.
           OPEN INPUT IXF.
           MOVE "K0002" TO IX-KEY.
           READ IXF.
           DISPLAY "persisted " IX-DATA.
           CLOSE IXF.
           OPEN OUTPUT RF.
           MOVE 5 TO RK. MOVE "five" TO RF-REC. WRITE RF-REC.
           MOVE 2 TO RK. MOVE "two" TO RF-REC. WRITE RF-REC.
           MOVE 5 TO RK. MOVE "again" TO RF-REC.
           WRITE RF-REC INVALID KEY DISPLAY "rel dup " FS.
           CLOSE RF.
           OPEN I-O RF.
           MOVE "N" TO EOF-FLAG.
           PERFORM UNTIL AT-EOF
               READ RF NEXT
                   AT END SET AT-EOF TO TRUE
                   NOT AT END DISPLAY RK " " RF-REC
               END-READ
           END-PERFORM.
           MOVE 2 TO RK.
           READ RF INVALID KEY DISPLAY "no 2".
           MOVE "TWO!" TO RF-REC.
           REWRITE RF-REC.
           MOVE 5 TO RK.
           DELETE RF.
           MOVE 5 TO RK.
           READ RF INVALID KEY DISPLAY "5 gone " FS.
           MOVE 2 TO RK.
           READ RF.
           DISPLAY RF-REC.
           CLOSE RF.
           STOP RUN.
