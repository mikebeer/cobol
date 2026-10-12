>>SOURCE FORMAT FREE
identification division.
program-id. freet.
data division.
working-storage section.
01 i pic 99.
01 total pic 9(4) value 0.
procedure division.
    perform varying i from 1 by 1 until i > 10
        add i to total
    end-perform
    display "total=" total
    display "a long literal " &
            "continued"
    stop run.
