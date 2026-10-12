       IDENTIFICATION DIVISION.
       PROGRAM-ID. COPYT.
       DATA DIVISION.
       WORKING-STORAGE SECTION.
       COPY t09_copy REPLACING ==TAGV== BY =="REPLACED"==.
       REPLACE ==MAXV== BY ==99==.
       77 LIM PIC 99 VALUE MAXV.
       REPLACE OFF.
       77 INP PIC X(10).
       PROCEDURE DIVISION.
           DISPLAY CP-NAME CP-NUM.
           DISPLAY LIM.
           ACCEPT INP.
           DISPLAY "GOT:" INP.
           ACCEPT INP.
           DISPLAY "GOT:" INP.
           STOP RUN.
