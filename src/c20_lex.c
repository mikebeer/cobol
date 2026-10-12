/* c20_lex.c -- source reader and tokenizer (fixed and free format) */

static int opt_free = 0;        /* -free */
static int opt_debug = 0;       /* compile D lines */
static int dpc = 0;             /* DECIMAL-POINT IS COMMA seen */
static char *incdirs[32];
static int nincdirs = 0;

typedef struct DefV DefV;
struct DefV { char *name, *val; DefV *next; };
static DefV *defs = 0;

static char *def_get(const char *n)
{
    DefV *d;
    for (d = defs; d; d = d->next)
        if (!strcmp(d->name, n)) return d->val ? d->val : "";
    return 0;
}

static void def_set(const char *n, const char *v)
{
    DefV *d;
    for (d = defs; d; d = d->next)
        if (!strcmp(d->name, n)) { d->val = xdup(v); return; }
    d = (DefV *)xalloc(sizeof(DefV));
    d->name = xdup(n);
    d->val = xdup(v);
    d->next = defs;
    defs = d;
}

static int is_wc(int c)
{
    return isalnum(c) || c == '-' || c == '_';
}

static int hexv(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    c = toupper(c);
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static void add_tok(TV *v, int k, const char *s, long len, int line,
                    const char *file, int sp, int fl)
{
    Tok t;
    memset(&t, 0, sizeof t);
    t.k = k;
    t.len = len;
    t.line = line;
    t.file = (char *)file;
    t.sp = sp;
    t.fl = fl;
    if (k == T_WORD) {
        t.raw = xdupn(s, len);
        t.s = upcase(t.raw);
    } else {
        t.s = xdupn(s, len);
        t.raw = xdupn(s, len);
    }
    tv_add(v, t);
}

/* tokenize one logical line */
static void lex_line(const char *s, long n, int line, const char *file,
                     TV *out)
{
    long i = 0;
    int sp = 1;
    while (i < n) {
        int c = (unsigned char)s[i];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            i++; sp = 1; continue;
        }
        if ((c == ',' || c == ';') &&
            (i + 1 >= n || s[i + 1] == ' ')) {
            i++; sp = 1; continue;
        }
        /* literals, possibly with a prefix */
        {
            long j = i;
            int fl = 0, q;
            int pre = 0;
            if (c == 'N' || c == 'n') {
                if (j + 1 < n && (s[j + 1] == 'X' || s[j + 1] == 'x')
                    && j + 2 < n && (s[j + 2] == '"' || s[j
                        + 2] == '\''))
                { fl = LF_NAT | LF_HEX; pre = 2; }
                else if (j + 1 < n && (s[j + 1] == '"' ||
                                       s[j + 1] == '\''))
                { fl = LF_NAT; pre = 1; }
            } else if (c == 'X' || c == 'x' || c == 'Z' || c == 'z' ||
                       c == 'B' || c == 'b' || c == 'H' || c == 'h') {
                if (j + 1 < n && (s[j + 1] == '"' || s[j
                    + 1] == '\'')) {
                    pre = 1;
                    if (c == 'X' || c == 'x') fl = LF_HEX;
                    else if (c == 'Z' || c == 'z') fl = LF_ZSTR;
                    else if (c == 'B' || c == 'b') fl = LF_BOOL;
                    else fl = LF_HEX;
                } else if ((c == 'B' || c == 'b') && j + 2 < n &&
                           (s[j + 1] == 'X' || s[j + 1] == 'x') &&
                           (s[j + 2] == '"' || s[j + 2] == '\'')) {
                    pre = 2; fl = LF_BOOL | LF_HEX;
                }
            }
            if (pre || c == '"' || c == '\'') {
                Sb b;
                memset(&b, 0, sizeof b);
                j = i + pre;
                q = s[j++];
                for (;;) {
                    if (j >= n) break;
                    if (s[j] == q) {
                        if (j + 1 < n && s[j + 1] == q) {
                            sb_c(&b, q); j += 2; continue;
                        }
                        j++;
                        break;
                    }
                    sb_c(&b, s[j++]);
                }
                if (fl & LF_HEX) {
                    Sb h;
                    long k;
                    memset(&h, 0, sizeof h);
                    for (k = 0; k + 1 < b.n; k += 2) {
                        int a = hexv(b.s[k]), d = hexv(b.s[k + 1]);
                        if (a < 0 || d < 0)
                            perr(file, line, "bad hex literal", 0);
                        sb_c(&h, a * 16 + d);
                    }
                    b = h;
                    if (fl & LF_BOOL) {
                        /* BX"..": bytes to bit string */
                        Sb z;
                        long k;
                        int bt;
                        memset(&z, 0, sizeof z);
                        for (k = 0; k < b.n; k++)
                            for (bt = 7; bt >= 0; bt--)
                                sb_c(&z, ((b.s[k] >> bt) & 1) ? '1'
                                                               : '0');
                        b = z;
                    }
                }
                if (fl & LF_ZSTR) sb_c(&b, 0);
                {
                    Tok t;
                    memset(&t, 0, sizeof t);
                    t.k = T_STR;
                    t.s = b.s ? b.s : xdup("");
                    t.len = b.n;
                    t.line = line;
                    t.file = (char *)file;
                    t.sp = sp;
                    t.fl = fl;
                    if (out->n >= 2 && out->a[out->n - 1].k == T_OP &&
                        out->a[out->n - 1].s[0] == '&' &&
                        out->a[out->n - 1].len == 1 &&
                        out->a[out->n - 2].k == T_STR &&
                        out->a[out->n - 2].fl == fl) {
                        Tok *q = &out->a[out->n - 2];
                        char *ns = (char *)xalloc(q->len + t.len + 1);
                        memcpy(ns, q->s, q->len);
                        memcpy(ns + q->len, t.s, t.len);
                        ns[q->len + t.len] = 0;
                        q->s = ns;
                        q->raw = ns;
                        q->len += t.len;
                        out->n--;
                    } else
                        tv_add(out, t);
                }
                i = j; sp = 0;
                continue;
            }
        }
        if (isdigit(c)) {
            long j = i;
            int isw = 0;
            while (j < n && is_wc(s[j])) {
                if (!isdigit((unsigned char)s[j])) isw = 1;
                j++;
            }
            if (isw) {
                /* a number followed by letters or hyphen: a word,
                   unless it is 1E+3 style */
                add_tok(out, T_WORD, s + i, j - i, line, file, sp, 0);
                i = j; sp = 0;
                continue;
            }
            /* number: digits [. digits] [E [+-] digits] */
            {
                char dpch = dpc ? ',' : '.';
                if (j + 1 < n && s[j] == dpch &&
                    isdigit((unsigned char)s[j + 1])) {
                    j++;
                    while (j < n && isdigit((unsigned char)s[j])) j++;
                }
                if (j < n && (s[j] == 'E' || s[j] == 'e')) {
                    long k = j + 1;
                    if (k < n && (s[k] == '+' || s[k] == '-')) k++;
                    if (k < n && isdigit((unsigned char)s[k])) {
                        while (k < n && isdigit((unsigned char)s[k]))
                            k++;
                        j = k;
                    }
                }
                add_tok(out, T_NUM, s + i, j - i, line, file, sp, 0);
                if (dpc) {
                    char *p = out->a[out->n - 1].s;
                    for (; *p; p++) if (*p == ',') *p = '.';
                }
                i = j; sp = 0;
                continue;
            }
        }
        if (c == '.') {
            if (i + 1 >= n || s[i + 1] == ' ' || s[i + 1] == '\t' ||
                s[i + 1] == '\r' || s[i + 1] == '\n') {
                add_tok(out, T_PERIOD, ".", 1, line, file, sp, 0);
                i++; sp = 0;
                continue;
            }
            if (i + 1 < n && isdigit((unsigned char)s[i + 1])
                && !dpc) {
                /* .5 */
                long j = i + 1;
                while (j < n && isdigit((unsigned char)s[j])) j++;
                add_tok(out, T_NUM, s + i, j - i, line, file, sp, 0);
                i = j; sp = 0;
                continue;
            }
            add_tok(out, T_OP, ".", 1, line, file, sp, 0);
            i++; sp = 0;
            continue;
        }
        if (dpc && c == ',' && i + 1 < n &&
            isdigit((unsigned char)s[i + 1])) {
            long j = i + 1;
            char *p;
            while (j < n && isdigit((unsigned char)s[j])) j++;
            add_tok(out, T_NUM, s + i, j - i, line, file, sp, 0);
            for (p = out->a[out->n - 1].s; *p; p++)
                if (*p == ',') *p = '.';
            i = j; sp = 0;
            continue;
        }
        if (isalpha(c) || c == '_') {
            long j = i;
            while (j < n && is_wc(s[j])) j++;
            add_tok(out, T_WORD, s + i, j - i, line, file, sp, 0);
            i = j; sp = 0;
            continue;
        }
        /* operators */
        {
            const char *two[] = { "**", "<=", ">=", "<>", "==", "::",
                                  "->", 0 };
            int k, done = 0;
            for (k = 0; two[k]; k++)
                if (i + 1 < n && s[i] == two[k][0] &&
                    s[i + 1] == two[k][1]) {
                    add_tok(out, T_OP, two[k], 2, line, file, sp, 0);
                    i += 2; sp = 0; done = 1;
                    break;
                }
            if (done) continue;
            add_tok(out, T_OP, s + i, 1, line, file, sp, 0);
            i++; sp = 0;
        }
    }
}

/* does a text end inside an unclosed literal?  returns the quote */
static int open_lit(const char *s, long n)
{
    int q = 0;
    long i;
    for (i = 0; i < n; i++) {
        if (q) {
            if (s[i] == q) {
                if (i + 1 < n && s[i + 1] == q) i++;
                else q = 0;
            }
        } else if (s[i] == '"' || s[i] == '\'') q = s[i];
    }
    return q;
}

/* conditional compilation state */
#define CCALL_MAX 32
static int cc_on[CCALL_MAX], cc_taken[CCALL_MAX], cc_n = 0;

static int cc_active(void)
{
    int i;
    for (i = 0; i < cc_n; i++) if (!cc_on[i]) return 0;
    return 1;
}

static int cc_eval(char **w, int nw)
{
    /* >>IF name DEFINED | name = value | NOT ... */
    int neg = 0, r = 0;
    int i = 0;
    if (i < nw && !strcmp(w[i], "NOT")) { neg = 1; i++; }
    if (i < nw) {
        char *v = def_get(w[i]);
        if (i + 1 < nw && !strcmp(w[i + 1], "DEFINED")) r = v != 0;
        else if (i + 2 < nw && !strcmp(w[i + 1], "=")) {
            const char *rhs = w[i + 2];
            r = v && !strcmp(v, rhs);
        } else if (i + 2 < nw && !strcmp(w[i + 1], "NOT")) {
            r = !(v && !strcmp(v, w[i + 3 < nw ? i + 3 : i + 2]));
        } else r = v && *v && strcmp(v, "0") && strcmp(v, "FALSE");
    }
    return neg ? !r : r;
}

static int split_words(char *s, char **w, int max)
{
    int n = 0;
    char *p = s;
    while (*p && n < max) {
        char *q;
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        q = p;
        if (*p == '"' || *p == '\'') {
            int qc = *p++;
            q = p;
            while (*p && *p != qc) p++;
            if (*p) *p++ = 0;
            w[n++] = q;
            continue;
        }
        while (*p && *p != ' ' && *p != '\t') p++;
        if (*p) *p++ = 0;
        {
            char *u;
            for (u = q; *u; u++) *u = (char)toupper((unsigned char)*u);
        }
        w[n++] = q;
    }
    return n;
}

static int directive(char *ln, int *freefmt)
{
    char *w[16];
    int nw;
    char *t = xdup(ln);
    char *p = t;
    while (*p == ' ') p++;
    if (p[0] != '>' || p[1] != '>') return 0;
    p += 2;
    nw = split_words(p, w, 16);
    if (nw == 0) return 1;
    if (!strcmp(w[0], "IF")) {
        int r = cc_active() ? cc_eval(w + 1, nw - 1) : 0;
        if (cc_n < CCALL_MAX) {
            cc_on[cc_n] = r;
            cc_taken[cc_n] = r;
            cc_n++;
        }
        return 1;
    }
    if (!strcmp(w[0], "ELIF") || !strcmp(w[0], "ELSE")) {
        if (cc_n > 0) {
            int r = 1;
            if (cc_taken[cc_n - 1]) r = 0;
            else if (w[0][2] == 'I') r = cc_eval(w + 1, nw - 1);
            cc_on[cc_n - 1] = r;
            if (r) cc_taken[cc_n - 1] = 1;
        }
        return 1;
    }
    if (!strcmp(w[0], "END-IF")) {
        if (cc_n > 0) cc_n--;
        return 1;
    }
    if (!cc_active()) return 1;
    if (!strcmp(w[0], "SOURCE")) {
        int i;
        for (i = 1; i < nw; i++) {
            if (!strcmp(w[i], "FREE")) *freefmt = 1;
            if (!strcmp(w[i], "FIXED")) *freefmt = 0;
        }
        return 1;
    }
    if (!strcmp(w[0], "DEFINE") && nw >= 2) {
        int i = 1;
        if (!strcmp(w[i], "CONSTANT")) i++;
        if (i < nw) {
            const char *v = "1";
            if (i + 2 < nw && !strcmp(w[i + 1], "AS")) v = w[i + 2];
            else if (i + 1 < nw && strcmp(w[i + 1], "PARAMETER"))
                v = w[i + 1];
            def_set(w[i], v);
        }
        return 1;
    }
    if (!strcmp(w[0], "SET")) return 1;
    return 1;
}

/* length of the text before an inline *> comment (outside literals) */
static long strip_inline(const char *t, long n)
{
    long j;
    int q = 0;
    for (j = 0; j < n; j++) {
        if (q) { if (t[j] == q) q = 0; }
        else if (t[j] == '"' || t[j] == '\'') q = t[j];
        else if (t[j] == '*' && j + 1 < n && t[j + 1] == '>') return j;
    }
    return n;
}

static void lex_text(const char *text, const char *file, int freefmt,
                     TV *out)
{
    const char *p = text;
    int line = 0;
    Sb cur;
    int curline = 0;
    memset(&cur, 0, sizeof cur);
    for (;;) {
        const char *e;
        char *ln;
        long len, i, k;
        Sb x;
        int eof = (*p == 0);
        if (eof) {
            if (cur.n) lex_line(cur.s, cur.n, curline, file, out);
            break;
        }
        e = p;
        while (*e && *e != '\n') e++;
        len = (long)(e - p);
        /* expand tabs, strip CR */
        memset(&x, 0, sizeof x);
        for (i = 0, k = 0; i < len; i++) {
            if (p[i] == '\r') continue;
            if (p[i] == '\t') {
                do { sb_c(&x, ' '); k++; } while (k % 8);
                continue;
            }
            sb_c(&x, p[i]); k++;
        }
        ln = sb_str(&x);
        len = x.n;
        line++;
        p = *e ? e + 1 : e;
        if (directive(ln, &freefmt)) continue;
        if (!cc_active()) continue;
        if (!freefmt) {
            int ind;
            if (len < 7) continue;
            ind = ln[6];
            if (ind == '*' || ind == '/') continue;
            if ((ind == 'D' || ind == 'd') && !opt_debug) continue;
            if (len > 72) len = 72;
            if (ind == '-') {
                const char *t = ln + 7;
                long tl = len - 7;
                int q = open_lit(cur.s ? cur.s : "", cur.n);
                if (!q) tl = strip_inline(t, tl);
                while (tl > 0 && *t == ' ') { t++; tl--; }
                if (q) {
                    /* pad the open literal line to column 72 */
                    if (tl > 0 && *t == q) { t++; tl--; }
                    sb_m(&cur, t, tl);
                } else {
                    sb_c(&cur, ' ');
                    sb_m(&cur, t, tl);
                }
                continue;
            }
            if (cur.n && open_lit(cur.s, cur.n))
                perr(file, curline, "unterminated literal", 0);
            if (cur.n) lex_line(cur.s, cur.n, curline, file, out);
            memset(&cur, 0, sizeof cur);
            curline = line;
            /* keep trailing spaces only if the literal stays open */
            {
                long tl = len - 7;
                const char *t = ln + 7;
                if (tl < 0) tl = 0;
                if (!open_lit(t, tl)) tl = strip_inline(t, tl);
                if (open_lit(t, tl)) {
                    sb_m(&cur, t, tl);
                    while (cur.n < 65) sb_c(&cur, ' ');
                } else sb_m(&cur, t, tl);
            }
        } else {
            /* free format: strip *> comments */
            long j;
            int q = 0;
            if (cur.n) lex_line(cur.s, cur.n, curline, file, out);
            memset(&cur, 0, sizeof cur);
            curline = line;
            for (j = 0; j < len; j++) {
                if (q) { if (ln[j] == q) q = 0; }
                else if (ln[j] == '"' || ln[j] == '\'') q = ln[j];
                else if (ln[j] == '*' && j + 1 < len &&
                         ln[j + 1] == '>') { len = j; break; }
            }
            /* a continuation: literal left open ends the line with
               a hyphen in column 1 of the next one (rare) */
            sb_m(&cur, ln, len);
        }
    }
}

/* scan the raw text for DECIMAL-POINT IS COMMA before tokenizing */
static void scan_dpc(const char *text)
{
    const char *p = text;
    while (*p) {
        if ((*p == 'D' || *p == 'd') &&
            !strncasecmp_(p, "DECIMAL-POINT", 13)) {
            const char *q = p + 13;
            while (*q == ' ' || *q == '\n' || *q == '\r') q++;
            if (!strncasecmp_(q, "IS", 2)) {
                q += 2;
                while (*q == ' ' || *q == '\n' || *q == '\r') q++;
            }
            if (!strncasecmp_(q, "COMMA", 5)) { dpc = 1; return; }
        }
        p++;
    }
}
