/* rt4_misc.h -- pointers, memory, call registry, small helpers */

/* ---- pointers and memory ---- */

static unsigned char cob_null_buf[8];
static const CobAttr cob_ptr_attr = { CT_PTR, 18, 0, 0, 0 };
static CobFld cob_null = { 8, cob_null_buf, &cob_ptr_attr };

static unsigned char *cob_ptr_get(const CobFld *p)
{
    unsigned char *q;
    memcpy(&q, p->data, sizeof q);
    return q;
}

static void cob_set_ptr(CobFld *p, void *q)
{
    memset(p->data, 0, (size_t)p->size);
    memcpy(p->data, &q, sizeof q);
}

static void cob_set_addr(CobFld *p, const CobFld *x)
{
    cob_set_ptr(p, x->data);
}

static CobFld *cob_addr(CobFld *t, const CobFld *x)
{
    static unsigned char ring[16][8];
    static int k = 0;
    unsigned char *b = ring[k++ & 15];
    t->size = 8;
    t->data = b;
    t->attr = &cob_ptr_attr;
    cob_set_ptr(t, x->data);
    return t;
}

static void cob_ptr_add(CobFld *p, long n)
{
    unsigned char *q = cob_ptr_get(p);
    cob_set_ptr(p, q + n);
}

static unsigned char *cob_alloc(long n, int zero)
{
    void *p = calloc(1, (size_t)(n > 0 ? n : 1));
    (void)zero;
    if (!p) cob_fatal("out of memory");
    return (unsigned char *)p;
}

static void cob_free(void *p)
{
    if (p) free(p);
}

static void cob_free_ptr(CobFld *p)
{
    cob_free(cob_ptr_get(p));
    cob_set_ptr(p, 0);
}

/* ---- helpers for generated code ---- */

static long cob_dtoi(const CobDec *d)
{
    int ok;
    return cd_to_long(d, &ok);
}

static long cob_idiv(long a, long b)
{
    if (b == 0) {
        cob_dz = 1;
        return 0;
    }
    return a / b;
}

static int cob_dsign(const CobDec *d)
{
    if (d->n == 0) return 0;
    return d->neg ? -1 : 1;
}

static CobFld *cob_dtof(const CobDec *d)
{
    typedef struct { unsigned char b[CD_MAX + 4]; CobAttr a; CobFld f;
        }
        Slot;
    static Slot ring[32];
    static int k = 0;
    Slot *s = &ring[k++ & 31];
    int nd = d->n > d->scale ? d->n : d->scale + 1, i;
    if (nd > CD_MAX) nd = CD_MAX;
    s->b[0] = (unsigned char)(d->neg && d->n ? '-' : '+');
    cob_dstr(d, s->b + 1, nd);
    for (i = 1; i <= nd; i++) s->b[i] = (unsigned char)('0' + s->b[i]);
    s->a.type = CT_NUMDISP;
    s->a.digits = nd;
    s->a.scale = d->scale;
    s->a.flags = CF_SIGNED | CF_SEP | CF_LEAD | CF_PLAIN;
    s->a.pic = 0;
    s->f.size = nd + 1;
    s->f.data = s->b;
    s->f.attr = &s->a;
    return &s->f;
}

static int cob_is_true(const CobFld *f)
{
    if (cob_isnum(f)) return cob_sign(f) != 0;
    return 1;
}

static int cob_inclass(const CobFld *f, const unsigned char *set)
{
    long i;
    for (i = 0; i < f->size; i++)
        if (!set[f->data[i]]) return 0;
    return 1;
}

static void cob_move_rel(const CobFld *sg, long soff, const CobFld *sf,
                         const CobFld *dg, long doff, const CobFld *df)
{
    CobFld s, d;
    s.size = sf->size;
    s.data = sg->data + soff;
    s.attr = sf->attr;
    d.size = df->size;
    d.data = dg->data + doff;
    d.attr = df->attr;
    cob_move(&s, &d);
}

static CobFld *cob_relf(CobFld *t, const CobFld *g, long off,
                        const CobFld *leaf)
{
    t->size = leaf->size;
    t->data = g->data + off;
    t->attr = leaf->attr;
    return t;
}

static void cob_run_init_at(const CobFld *g, const CobInit *t, int n)
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
                    e.data = g->data + x->rel +
                             (x->nd > 0 ? a * x->str[0] : 0) +
                             (x->nd > 1 ? b * x->str[1] : 0) +
                             (x->nd > 2 ? c * x->str[2] : 0);
                    cob_init_one(&e, x);
                }
    }
}

/* DIVIDE ... REMAINDER: rem = dividend - stored quotient * divisor */
static void cob_remainder(CobDec *dividend, CobDec *divisor,
                          const CobFld *quot, CobFld *rem)
{
    CobDec q, m, r;
    if (cob_dz) return;
    cob_ld(&q, quot);
    cd_mul(&m, &q, divisor);
    cd_sub(&r, dividend, &m);
    cob_st(&r, rem, CR_NONE, 0);
}

/* BY CONTENT argument copy */
static void cob_content(CobFld *dst, const CobFld *src, long cap)
{
    long n = src->size > cap ? cap : src->size;
    memcpy(dst->data, src->data, (size_t)n);
    dst->size = n;
    dst->attr = src->attr;
}

static const CobAttr cob_bv_attr = { CT_BIN5, 9, 0, CF_SIGNED, 0 };

/* BY VALUE argument: a native 4-byte integer */
static void cob_byvalue(CobFld *dst, const CobDec *v)
{
    long n = cob_dtoi(v);
    int x = (int)n;
    memcpy(dst->data, &x, sizeof x);
    dst->size = (long)sizeof x;
    dst->attr = &cob_bv_attr;
}

/* ---- the program registry and CALL ---- */

typedef int (*CobFn)(int, CobFld **, CobFld *);
typedef struct { char name[40]; CobFn fn; } CobProg;
static CobProg cob_progs[256];
static int cob_nprogs = 0;

static void cob_upname(char *d, const char *s, long n)
{
    long i;
    if (n > 38) n = 38;
    for (i = 0; i < n; i++) d[i] = (char)toupper((unsigned char)s[i]);
    d[n] = 0;
}

static void cob_reg(const char *name, CobFn fn)
{
    if (cob_nprogs >= 256) cob_fatal("too many programs");
    cob_upname(cob_progs[cob_nprogs].name, name, (long)strlen(name));
    cob_progs[cob_nprogs++].fn = fn;
}

static int cob_builtin_call(const char *name, int na, CobFld **a)
{
    if (!strcmp(name, "SYSTEM") && na >= 1 && a[0]) {
        char cmd[1024];
        long n = a[0]->size;
        int rc;
        if (n > 1000) n = 1000;
        memcpy(cmd, a[0]->data, (size_t)n);
        cmd[n] = 0;
        while (n > 0 && (cmd[n - 1] == ' ' || cmd[n - 1] == 0))
            cmd[--n] = 0;
        fflush(stdout);
        rc = system(cmd);
        cob_seti(&cob_return_code, rc);
        return 1;
    }
    return 0;
}

#ifdef COB_HAVE_DL
typedef size_t (*CobCFn)(size_t, size_t, size_t, size_t, size_t,
                         size_t, size_t, size_t);
typedef void (*CobRegFn)(void (*)(const char *, CobFn));

/* load NAME.so (or libNAME.so) from COB_LIBRARY_PATH or "." and let
   it register its programs */
static int cob_load_module(const char *name)
{
    char lo[48], path[600];
    const char *lp = getenv("COB_LIBRARY_PATH");
    char dirs[520];
    char *d, *e;
    int pass, k;
    long i, n = (long)strlen(name);
    if (n > 40) return 0;
    for (i = 0; i <= n; i++)
        lo[i] = (char)tolower((unsigned char)name[i]);
    strcpy(dirs, lp && strlen(lp) < 500 ? lp : "");
    if (dirs[0]) strcat(dirs, ":");
    strcat(dirs, ".");
    for (d = dirs; d && *d; d = e ? e + 1 : 0) {
        e = strchr(d, ':');
        if (e) *e = 0;
        for (pass = 0; pass < 4; pass++) {
            void *h;
            CobRegFn rf;
            const char *base = pass < 2 ? lo : name;
            sprintf(path, "%s/%s%s.so", d, pass % 2 ? "lib" : "",
                    base);
            h = dlopen(path, RTLD_NOW | RTLD_LOCAL);
            if (!h) continue;
            rf = (CobRegFn)dlsym(h, "cobmod_register");
            if (!rf) continue;
            k = cob_nprogs;
            rf(cob_reg);
            if (cob_nprogs > k) return 1;
        }
        if (!e) break;
    }
    return 0;
}

/* call a C function: BY REFERENCE arguments as addresses, BY VALUE
   arguments as integers */
static int cob_ccall(const char *name, int na, CobFld **a, CobFld *ret)
{
    char lo[48];
    void *p;
    size_t v[8];
    int i;
    size_t r;
    long n = (long)strlen(name);
    if (n > 40) return 0;
    for (i = 0; i <= n; i++)
        lo[i] = (char)tolower((unsigned char)name[i]);
    p = dlsym(RTLD_DEFAULT, name);
    if (!p) p = dlsym(RTLD_DEFAULT, lo);
    if (!p) return 0;
    memset(v, 0, sizeof v);
    for (i = 0; i < na && i < 8; i++) {
        if (!a[i]) v[i] = 0;
        else if (a[i]->attr == &cob_bv_attr) {
            int x;
            memcpy(&x, a[i]->data, sizeof x);
            v[i] = (size_t)(long)x;
        } else v[i] = (size_t)a[i]->data;
    }
    r = ((CobCFn)p)(v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7]);
    cob_seti(&cob_return_code, (long)(int)r);
    if (ret) cob_seti(ret, (long)(int)r);
    return 1;
}
#endif

static int cob_call(const char *name, int na, CobFld **a, CobFld *ret)
{
    char u[40];
    int i, pass;
    cob_upname(u, name, (long)strlen(name));
    for (pass = 0; pass < 2; pass++) {
        for (i = 0; i < cob_nprogs; i++)
            if (!strcmp(cob_progs[i].name, u)) {
                cob_progs[i].fn(na, a, ret);
                return 0;
            }
        if (pass == 0 && cob_builtin_call(u, na, a)) return 0;
#ifdef COB_HAVE_DL
        if (pass == 0 && !cob_load_module(name)) {
            if (cob_ccall(name, na, a, ret)) return 0;
            break;
        }
#else
        break;
#endif
    }
    return 1;
}

static int cob_call_f(const CobFld *nf, int na, CobFld **a,
    CobFld *ret)
{
    char b[48];
    long n = nf->size;
    if (n > 40) n = 40;
    memcpy(b, nf->data, (size_t)n);
    while (n > 0 && (b[n - 1] == ' ' || b[n - 1] == 0)) n--;
    b[n] = 0;
    return cob_call(b, na, a, ret);
}

static void cob_cancel(const char *name)
{
    (void)name;
}

static void cob_cancel_f(const CobFld *nf)
{
    (void)nf;
}

/* ---- file runtime stub, replaced when files are added ---- */
