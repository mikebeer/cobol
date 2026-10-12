/* rt3_core.h -- start and stop, DISPLAY, ACCEPT, references,
   initialization, arithmetic helpers */

static int cob_argc = 0;
static char **cob_argv = 0;
static int cob_argi = 0;
static int cob_check = 0;               /* subscript checking */
static int cob_files_open = 0;

static unsigned char cob_rc_buf[4];
static const CobAttr cob_rc_attr = { CT_BIN, 9, 0, CF_SIGNED, 0 };
static CobFld cob_return_code = { 4, cob_rc_buf, &cob_rc_attr };

static void cob_close_all(void);

static void cob_fatal(const char *m)
{
    fflush(stdout);
    fprintf(stderr, "cobol: runtime error: %s\n", m);
    cob_close_all();
    exit(1);
}

static void cob_stop(int rc)
{
    fflush(stdout);
    cob_close_all();
    exit(rc);
}

static void cob_init(int argc, char **argv)
{
    unsigned short t = 1;
    cob_argc = argc;
    cob_argv = argv;
    cob_little = *(unsigned char *)&t == 1;
    if (getenv("COB_CHECK")) cob_check = 1;
}

static long cob_getrc(void)
{
    return cob_toi(&cob_return_code);
}

/* ---- DISPLAY ---- */

static FILE *cob_out = 0;

static void cob_put(const unsigned char *s, long n)
{
    if (!cob_out) cob_out = stdout;
    if (n > 0) fwrite(s, 1, (size_t)n, cob_out);
}

static void cob_disp(const CobFld *f)
{
    const CobAttr *a = f->attr;
    unsigned char b[240];
    CobDec v;
    long k, i;
    switch (a->type) {
    case CT_NUMDISP: case CT_BIN: case CT_BIN5: case CT_PACKED: {
        int neg;
        cob_ld(&v, f);
        neg = v.neg;
        k = 0;
        if (a->flags & CF_PLAIN) {
            long z = 0, sc, ni = v.n - v.scale;
            while (z < v.scale && (z >= v.n || v.d[z] == 0)) z++;
            sc = v.scale - z;
            if (v.n == 0) neg = 0;
            if (neg) b[k++] = '-';
            if (ni < 1) ni = 1;
            for (i = ni - 1; i >= 0; i--)
                b[k++] = (unsigned char)('0' + (i + v.scale < v.n ?
                                               v.d[i + v.scale] : 0));
            if (sc > 0) {
                b[k++] = '.';
                for (i = v.scale - 1; i >= z; i--)
                    b[k++] = (unsigned char)('0' + (i < v.n ?
                                                    v.d[i] : 0));
            }
            cob_put(b, k);
            break;
        }
        if (a->flags & CF_SIGNED) b[k++] = (unsigned char)(neg ? '-' :
                                                          '+');
        {
            long nd = a->digits, dg = a->scale > 0 ? a->scale : 0;
            long ni = nd - dg;
            if (a->type == CT_BIN || a->type == CT_BIN5) {
                if (!(a->flags & CF_NOTRUNC) && nd < 1) nd = 1;
            }
            if (ni < 1) ni = 1;
            for (i = ni - 1; i >= 0; i--) {
                long pos = i + dg;
                b[k++] = (unsigned char)('0' + (pos < v.n ?
                                               v.d[pos] : 0));
            }
            if (dg > 0) {
                b[k++] = '.';
                for (i = dg - 1; i >= 0; i--)
                    b[k++] = (unsigned char)('0' + (i < v.n ?
                                                    v.d[i] : 0));
            }
        }
        cob_put(b, k);
        break;
    }
    case CT_FLT: case CT_DBL: {
        char t[40];
        double x;
        if (a->type == CT_FLT) {
            float y;
            memcpy(&y, f->data, sizeof y);
            x = y;
        } else {
            memcpy(&x, f->data, sizeof x);
        }
        sprintf(t, "%.15g", x);
        cob_put((unsigned char *)t, (long)strlen(t));
        break;
    }
    case CT_PTR: {
        char t[40];
        void *p;
        memcpy(&p, f->data, sizeof p < (size_t)f->size ? sizeof p :
               (size_t)f->size);
        sprintf(t, "%p", p);
        cob_put((unsigned char *)t, (long)strlen(t));
        break;
    }
    case CT_NAT: {
        for (i = 1; i < f->size; i += 2) cob_put(f->data + i, 1);
        break;
    }
    default:
        cob_put(f->data, f->size);
        break;
    }
}

static void cob_disp_nl(void)
{
    cob_put((const unsigned char *)"\n", 1);
}

static void cob_disp_upon(int err)
{
    cob_out = err ? stderr : stdout;
    fflush(stdout);
}

/* ---- ACCEPT ---- */

static void cob_acc_text(CobFld *f, const char *s)
{
    if (cob_isnum(f)) {
        CobDec v;
        cob_parse_num(&v, (const unsigned char *)s, (long)strlen(s),
                      0);
        cob_st(&v, f, CR_NONE, 0);
    } else {
        cob_just(f, (const unsigned char *)s, (long)strlen(s));
    }
}

static void cob_accept(CobFld *f)
{
    char line[1024];
    long n;
    fflush(stdout);
    if (!fgets(line, sizeof line, stdin)) line[0] = 0;
    n = (long)strlen(line);
    while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r'))
        line[--n] = 0;
    cob_acc_text(f, line);
}

#define AC_DATE 1               /* YYMMDD */
#define AC_DAY 2                /* YYDDD */
#define AC_DOW 3
#define AC_TIME 4               /* HHMMSShh */
#define AC_DATE8 5              /* YYYYMMDD */
#define AC_DAY7 6               /* YYYYDDD */
#define AC_CMDLINE 7
#define AC_ARGNUM 8
#define AC_ARGVAL 9

static void cob_accept_from(CobFld *f, int what)
{
    time_t t = time(0);
    struct tm *m = localtime(&t);
    char b[300];
    switch (what) {
    case AC_DATE:
        sprintf(b, "%02d%02d%02d", m->tm_year % 100, m->tm_mon + 1,
                m->tm_mday);
        break;
    case AC_DATE8:
        sprintf(b, "%04d%02d%02d", m->tm_year + 1900, m->tm_mon + 1,
                m->tm_mday);
        break;
    case AC_DAY:
        sprintf(b, "%02d%03d", m->tm_year % 100, m->tm_yday + 1);
        break;
    case AC_DAY7:
        sprintf(b, "%04d%03d", m->tm_year + 1900, m->tm_yday + 1);
        break;
    case AC_DOW:
        sprintf(b, "%d", m->tm_wday == 0 ? 7 : m->tm_wday);
        break;
    case AC_TIME:
        sprintf(b, "%02d%02d%02d00", m->tm_hour, m->tm_min, m->tm_sec);
        break;
    case AC_CMDLINE: {
        int i;
        b[0] = 0;
        for (i = 1; i < cob_argc; i++) {
            if (strlen(b) + strlen(cob_argv[i]) + 2 > sizeof b) break;
            if (i > 1) strcat(b, " ");
            strcat(b, cob_argv[i]);
        }
        break;
    }
    case AC_ARGNUM:
        sprintf(b, "%d", cob_argc - 1);
        break;
    case AC_ARGVAL:
        cob_argi++;
        if (cob_argi < cob_argc) {
            strncpy(b, cob_argv[cob_argi], sizeof b - 1);
            b[sizeof b - 1] = 0;
        } else {
            b[0] = 0;
        }
        break;
    default:
        b[0] = 0;
    }
    cob_acc_text(f, b);
}

static void cob_accept_env(CobFld *f, const CobFld *name)
{
    char nm[200];
    const char *v;
    long n = name->size;
    while (n > 0 && name->data[n - 1] == ' ') n--;
    if (n > 199) n = 199;
    memcpy(nm, name->data, (size_t)n);
    nm[n] = 0;
    v = getenv(nm);
    cob_acc_text(f, v ? v : "");
}

/* ---- references: subscripts and reference modification ---- */

static long cob_ix(long i, long max)
{
    if (cob_check && (i < 1 || i > max))
        cob_fatal("subscript out of range");
    return i - 1;
}

static CobFld *cob_el(CobFld *t, const CobFld *f, long off)
{
    t->size = f->size;
    t->data = f->data + off;
    t->attr = f->attr;
    return t;
}

/* start is 1-based; len < 0 means to the end of the item */
static CobFld *cob_rm(CobFld *t, const CobFld *f, long start, long len)
{
    long sz = (f->attr->type == CT_NAT) ? 2 : 1;
    if (cob_check && (start < 1 || start > f->size / sz ||
                      len > f->size / sz - start + 1))
        cob_fatal("reference modification out of range");
    if (start < 1) start = 1;
    if (len < 0) len = f->size / sz - start + 1;
    if (len < 0) len = 0;
    t->size = len * sz;
    t->data = f->data + (start - 1) * sz;
    t->attr = f->attr->type == CT_NAT ? f->attr : &cob_alnum_attr;
    return t;
}

/* ---- initialization tables ---- */

typedef struct {
    CobFld *f;
    int kind;                   /* 0 default, 1 value, 2 figurative,
                                   3 ALL literal */
    const CobFld *lit;
    int fig;
    int nd;
    long cnt[3];
    long str[3];
    long rel;
} CobInit;

static void cob_init_one(CobFld *f, const CobInit *t)
{
    switch (t->kind) {
    case 0:
        if (cob_isnum(f)) cob_fig(f, FG_ZERO);
        else if (f->attr->type == CT_PTR || f->attr->type == CT_OBJ)
            memset(f->data, 0, (size_t)f->size);
        else cob_fig(f, FG_SPACE);
        break;
    case 1:
        cob_move(t->lit, f);
        break;
    case 2:
        cob_fig(f, t->fig);
        break;
    case 3:
        cob_move_all(f, t->lit);
        break;
    }
}

static void cob_run_init(const CobInit *t, int n)
{
    int i;
    long a, b, c;
    CobFld e;
    for (i = 0; i < n; i++) {
        const CobInit *x = &t[i];
        long n1 = x->nd > 0 ? x->cnt[0] : 1;
        long n2 = x->nd > 1 ? x->cnt[1] : 1;
        long n3 = x->nd > 2 ? x->cnt[2] : 1;
        for (a = 0; a < n1; a++)
            for (b = 0; b < n2; b++)
                for (c = 0; c < n3; c++) {
                    e = *x->f;
                    e.data += (x->nd > 0 ? a * x->str[0] : 0) +
                              (x->nd > 1 ? b * x->str[1] : 0) +
                              (x->nd > 2 ? c * x->str[2] : 0);
                    cob_init_one(&e, x);
                }
    }
}

/* ---- arithmetic as C expressions ---- */

static int cob_dz = 0;                  /* division by zero seen */

static CobDec *cdx_ld(CobDec *r, const CobFld *f)
{
    cob_ld(r, f);
    return r;
}

static CobDec *cdx_lit(CobDec *r, const char *s)
{
    cd_from_str(r, s, (long)strlen(s));
    return r;
}

static CobDec *cdx_add(CobDec *r, CobDec *a, CobDec *b)
{
    cd_add(r, a, b);
    return r;
}

static CobDec *cdx_sub(CobDec *r, CobDec *a, CobDec *b)
{
    cd_sub(r, a, b);
    return r;
}

static CobDec *cdx_mul(CobDec *r, CobDec *a, CobDec *b)
{
    cd_mul(r, a, b);
    return r;
}

static CobDec *cdx_div(CobDec *r, CobDec *a, CobDec *b)
{
    if (!cd_div(r, a, b, CD_DIVS)) cob_dz = 1;
    return r;
}

static CobDec *cdx_pow(CobDec *r, CobDec *a, CobDec *b)
{
    cd_pow(r, a, b);
    return r;
}

static CobDec *cdx_neg(CobDec *r, CobDec *a)
{
    *r = *a;
    cd_neg(r);
    return r;
}

static CobDec *cdx_int(CobDec *r, long v)
{
    cd_from_long(r, v);
    return r;
}

static void cob_clr_ex(void)
{
    cob_dz = 0;
    cd_ovf = 0;
    cob_size_err = 0;
}

/* store with the ON SIZE ERROR logic: returns 1 when it failed */
static int cob_stx(CobDec *v, CobFld *f, int mode, int handler)
{
    int e;
    if (cob_dz) return 1;
    e = cob_st(v, f, mode, handler);
    return e || cd_ovf;
}

/* ADD a TO f and the like: f = f op v; returns size error */
static int cob_addto(CobDec *v, CobFld *f, int mode, int handler)
{
    CobDec t, r;
    if (cob_dz) return 1;
    cob_ld(&t, f);
    cd_add(&r, &t, v);
    return cob_stx(&r, f, mode, handler);
}

static int cob_subfrom(CobDec *v, CobFld *f, int mode, int handler)
{
    CobDec t, r;
    if (cob_dz) return 1;
    cob_ld(&t, f);
    cd_sub(&r, &t, v);
    return cob_stx(&r, f, mode, handler);
}

static int cob_mulby(CobDec *v, CobFld *f, int mode, int handler)
{
    CobDec t, r;
    if (cob_dz) return 1;
    cob_ld(&t, f);
    cd_mul(&r, &t, v);
    return cob_stx(&r, f, mode, handler);
}

static int cob_divby(CobDec *v, CobFld *f, int mode, int handler)
{
    CobDec t, r;
    if (cob_dz) return 1;
    cob_ld(&t, f);
    if (!cd_div(&r, &t, v, CD_DIVS)) return 1;
    return cob_stx(&r, f, mode, handler);
}

/* a decimal compared with a field or another decimal */
static int cob_cmpd(CobDec *a, CobDec *b)
{
    return cd_cmp(a, b);
}

static int cob_dtrue(CobDec *a)
{
    return a->n != 0;
}
