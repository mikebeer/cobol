       IDENTIFICATION DIVISION.
       PROGRAM-ID. SORTT.
       ENVIRONMENT DIVISION.
       INPUT-OUTPUT SECTION.
       FILE-CONTROL.
           SELECT IN-F ASSIGN TO "t13_in.txt"
               ORGANIZATION IS LINE SEQUENTIAL.
           SELECT OUT-F ASSIGN TO "t13_out.txt"
               ORGANIZATION IS LINE SEQUENTIAL.
           SELECT SRT ASSIGN TO "t13_work".
           SELECT BAD ASSIGN TO "t13_missing.dat"
               FILE STATUS IS BFS.
       DATA DIVISION.
       FILE SECTION.
       FD IN-F.
       01 IN-REC PIC X(20).
       FD OUT-F.
       01 OUT-REC PIC X(20).
       SD SRT.
       01 SRT-REC.
          05 S-NAME PIC X(10).
          05 S-NUM PIC 9(3).
          05 FILLER PIC X(7).
       FD BAD.
       01 BAD-REC PIC X(5).
       WORKING-STORAGE SECTION.
       77 BFS PIC XX.
       77 EOF-FLAG PIC X VALUE "N".
          88 AT-EOF VALUE "Y".
       77 TOTAL PIC 9(5) VALUE 0.
       PROCEDURE DIVISION.
       DECLARATIVES.
       BAD-ERR SECTION.
           USE AFTER STANDARD EXCEPTION PROCEDURE ON BAD.
       BAD-HANDLER.
           DISPLAY "declarative: status " BFS.
       END DECLARATIVES.
       MAIN SECTION.
       M1.
           OPEN OUTPUT IN-F.
           MOVE "pear      020" TO IN-REC. WRITE IN-REC.
           MOVE "apple     010" TO IN-REC. WRITE IN-REC.
           MOVE "plum      015" TO IN-REC. WRITE IN-REC.
           MOVE "apple     005" TO IN-REC. WRITE IN-REC.
           CLOSE IN-F.
           SORT SRT ON ASCENDING KEY S-NAME
                       DESCENDING KEY S-NUM
               USING IN-F GIVING OUT-F.
           OPEN INPUT OUT-F.
           PERFORM UNTIL AT-EOF
               READ OUT-F AT END SET AT-EOF TO TRUE
                   NOT AT END DISPLAY OUT-REC
               END-READ
           END-PERFORM.
           CLOSE OUT-F.
           SORT SRT ON DESCENDING KEY S-NUM
               INPUT PROCEDURE IS FEED
               OUTPUT PROCEDURE IS DRAIN.
           DISPLAY "total " TOTAL.
           OPEN INPUT BAD.
           DISPLAY "after open " BFS.
           STOP RUN.
       FEED SECTION.
       F1.
           MOVE "x         001" TO SRT-REC. RELEASE SRT-REC.
           MOVE "y         300" TO SRT-REC. RELEASE SRT-REC.
           MOVE "z         042" TO SRT-REC. RELEASE SRT-REC.
       DRAIN SECTION.
       D1.
           MOVE "N" TO EOF-FLAG.
           PERFORM UNTIL AT-EOF
               RETURN SRT AT END SET AT-EOF TO TRUE
                   NOT AT END
                       DISPLAY S-NAME S-NUM
                       ADD S-NUM TO TOTAL
               END-RETURN
           END-PERFORM.
