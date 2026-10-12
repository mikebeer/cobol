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

static int cob_call(const char *name, int na, CobFld **a, CobFld *ret)
{
    char u[40];
    int i;
    cob_upname(u, name, (long)strlen(name));
    for (i = 0; i < cob_nprogs; i++)
        if (!strcmp(cob_progs[i].name, u)) {
            cob_progs[i].fn(na, a, ret);
            return 0;
        }
    if (cob_builtin_call(u, na, a)) return 0;
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
