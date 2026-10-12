/* c10_util.c -- memory, errors, string builder */

static char *progname = "cobol";

static void die(const char *m)
{
    fprintf(stderr, "%s: %s\n", progname, m);
    exit(8);
}

static void perr(const char *file, int line, const char *m,
                 const char *a)
{
    fprintf(stderr, "%s: %s: line %d: %s%s%s\n", progname,
            file ? file : "", line, m, a ? " " : "", a ? a : "");
    exit(8);
}

static void *xalloc(long n)
{
    void *p = calloc(1, (size_t)(n > 0 ? n : 1));
    if (!p) die("out of memory");
    return p;
}

static char *xdup(const char *s)
{
    char *p = (char *)xalloc((long)strlen(s) + 1);
    strcpy(p, s);
    return p;
}

static char *xdupn(const char *s, long n)
{
    char *p = (char *)xalloc(n + 1);
    memcpy(p, s, (size_t)n);
    return p;
}

static void sb_grow(Sb *b, long k)
{
    char *p;
    if (b->n + k + 1 <= b->cap) return;
    b->cap = (b->cap + k + 1) * 2;
    p = (char *)xalloc(b->cap);
    if (b->n) memcpy(p, b->s, (size_t)b->n);
    b->s = p;
}

static void sb_c(Sb *b, int c)
{
    sb_grow(b, 1);
    b->s[b->n++] = (char)c;
    b->s[b->n] = 0;
}

static void sb_s(Sb *b, const char *s)
{
    long k = (long)strlen(s);
    sb_grow(b, k);
    memcpy(b->s + b->n, s, (size_t)k);
    b->n += k;
    b->s[b->n] = 0;
}

static void sb_m(Sb *b, const char *s, long k)
{
    sb_grow(b, k);
    memcpy(b->s + b->n, s, (size_t)k);
    b->n += k;
    b->s[b->n] = 0;
}

static char *sb_str(Sb *b)
{
    return b->s ? b->s : "";
}

static char *cat2(const char *a, const char *b)
{
    char *p = (char *)xalloc((long)(strlen(a) + strlen(b)) + 1);
    strcpy(p, a);
    strcat(p, b);
    return p;
}

static char *cat3(const char *a, const char *b, const char *c)
{
    char *t = cat2(a, b);
    return cat2(t, c);
}

static char *cat4(const char *a, const char *b, const char *c,
                  const char *d)
{
    char *t = cat3(a, b, c);
    return cat2(t, d);
}

static char *cat5(const char *a, const char *b, const char *c,
                  const char *d, const char *e)
{
    char *t = cat4(a, b, c, d);
    return cat2(t, e);
}

static char *itoa_(long v)
{
    char b[40];
    sprintf(b, "%ld", v);
    return xdup(b);
}

static char *upcase(const char *s)
{
    char *p = xdup(s), *q;
    for (q = p; *q; q++) *q = (char)toupper((unsigned char)*q);
    return p;
}

/* C identifier made from a COBOL name */
static char *mangle(const char *s)
{
    char *p = xdup(s), *q;
    for (q = p; *q; q++)
        if (!isalnum((unsigned char)*q)) *q = '_';
    return p;
}

static void tv_add(TV *v, Tok t)
{
    if (v->n >= v->cap) {
        Tok *p;
        v->cap = v->cap ? v->cap * 2 : 1024;
        p = (Tok *)xalloc(v->cap * (long)sizeof(Tok));
        if (v->n) memcpy(p, v->a, (size_t)v->n * sizeof(Tok));
        v->a = p;
    }
    v->a[v->n++] = t;
}

static char *read_file(const char *name)
{
    FILE *f = fopen(name, "rb");
    char *buf;
    long n;
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = (char *)xalloc(n + 2);
    if (n > 0 && fread(buf, 1, (size_t)n, f) != (size_t)n) {
        fclose(f);
        return 0;
    }
    fclose(f);
    buf[n] = 0;
    return buf;
}

static int file_ok(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fclose(f);
    return 1;
}

static int strncasecmp_(const char *a, const char *b, long n)
{
    long i;
    for (i = 0; i < n; i++) {
        int x = toupper((unsigned char)a[i]);
        int y = toupper((unsigned char)b[i]);
        if (x != y) return x - y;
        if (!x) return 0;
    }
    return 0;
}

static char *fmt(const char *f, ...)
{
    static char buf[65536];
    va_list ap;
    va_start(ap, f);
    vsprintf(buf, f, ap);
    va_end(ap);
    return xdup(buf);
}
