/* cfn.c -- C functions called from COBOL with CALL.
   BY REFERENCE arguments arrive as addresses of the COBOL data,
   BY VALUE arguments as ints. The data is not NUL terminated. */
#include <ctype.h>
#include <string.h>

int c_gcd(int a, int b)
{
    while (b != 0) {
        int t = a % b;
        a = b;
        b = t;
    }
    return a;
}

/* converts the first n characters of s to upper case, in place;
   returns the number of letters changed */
int c_upcase(char *s, int n)
{
    int i, changed = 0;
    for (i = 0; i < n; i++)
        if (islower((unsigned char)s[i])) {
            s[i] = (char)toupper((unsigned char)s[i]);
            changed++;
        }
    return changed;
}

/* the length of s without trailing blanks */
int c_trimlen(char *s, int n)
{
    while (n > 0 && s[n - 1] == ' ')
        n--;
    return n;
}
