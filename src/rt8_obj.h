/* rt8_obj.h -- classes and objects */

typedef struct CobClass CobClass;
typedef struct CobObj CobObj;
typedef struct { const char *name; CobFn fn; int fac; } CobMeth;

struct CobClass {
    const char *name;
    const char *base;
    CobMeth *meths;
    int nmeths;
    CobClass *bp;
    unsigned char *fac;
    int idx;
    CobObj *fobj;
};

struct CobObj {
    CobClass *cls;
    int isfac;
    unsigned char *blk[64];
};

static CobClass *cob_classes[64];
static int cob_ncls = 0;
static CobObj *cob_self = 0;
static CobClass *cob_fcls = 0;
static const CobAttr cob_obj_attr = { CT_OBJ, 18, 0, 0, 0 };

static void cob_class_reg(CobClass *c)
{
    CobObj *f;
    if (cob_ncls >= 64) cob_fatal("too many classes");
    c->idx = cob_ncls;
    cob_classes[cob_ncls++] = c;
    f = (CobObj *)calloc(1, sizeof(CobObj));
    if (!f) cob_fatal("out of memory");
    f->cls = c;
    f->isfac = 1;
    c->fobj = f;
}

static CobClass *cob_class_find(const char *name)
{
    int i;
    char u[48];
    cob_upname(u, name, (long)strlen(name));
    for (i = 0; i < cob_ncls; i++)
        if (!strcmp(cob_classes[i]->name, u)) return cob_classes[i];
    return 0;
}

static CobClass *cob_base(CobClass *c)
{
    if (!c->bp && c->base) c->bp = cob_class_find(c->base);
    return c->bp;
}

static CobObj *cob_class_obj(const char *name)
{
    CobClass *c = cob_class_find(name);
    if (!c) cob_fatal("class not found");
    return c->fobj;
}

static CobObj *cob_getobj(const CobFld *f)
{
    CobObj *o;
    memcpy(&o, f->data, sizeof o);
    return o;
}

static unsigned char *cob_objblk(CobObj *o, int idx, long size,
                                 int *isnew)
{
    *isnew = 0;
    if (!o) cob_fatal("method called without an object");
    if (!o->blk[idx]) {
        o->blk[idx] = (unsigned char *)cob_alloc(size, 1);
        *isnew = 1;
    }
    return o->blk[idx];
}

static CobMeth *cob_find_meth(CobClass *c, const char *name, int fac)
{
    for (; c; c = cob_base(c)) {
        int i;
        for (i = 0; i < c->nmeths; i++)
            if (c->meths[i].fac == fac &&
                !strcmp(c->meths[i].name, name)) return &c->meths[i];
    }
    return 0;
}

/* returns 0 when the method ran, 1 when there is no such method */
static int cob_invoke(CobObj *o, const char *name, int na, CobFld **a,
                      CobFld *ret, CobClass *super_from)
{
    char u[48];
    CobMeth *m;
    CobClass *start;
    if (!o) cob_fatal("INVOKE on a null object reference");
    cob_upname(u, name, (long)strlen(name));
    start = super_from ? cob_base(super_from) : o->cls;
    m = start ? cob_find_meth(start, u, o->isfac) : 0;
    if (!m) {
        if (o->isfac && !strcmp(u, "NEW")) {
            CobObj *n = (CobObj *)calloc(1, sizeof(CobObj));
            if (!n) cob_fatal("out of memory");
            n->cls = o->cls;
            if (ret) cob_set_ptr(ret, n);
            return 0;
        }
        if (!o->isfac && !strcmp(u, "FREE")) {
            int i;
            for (i = 0; i < 64; i++) free(o->blk[i]);
            free(o);
            return 0;
        }
        return 1;
    }
    cob_self = o;
    cob_fcls = o->cls;
    m->fn(na, a, ret);
    return 0;
}

static int cob_invoke_f(CobObj *o, const CobFld *nf, int na,
                        CobFld **a, CobFld *ret, CobClass *super_from)
{
    char b[48];
    long n = nf->size;
    if (n > 40) n = 40;
    memcpy(b, nf->data, (size_t)n);
    while (n > 0 && (b[n - 1] == ' ' || b[n - 1] == 0)) n--;
    b[n] = 0;
    return cob_invoke(o, b, na, a, ret, super_from);
}

/* run an initialization table that ends with an empty entry */
static void cob_run_init_z(const CobInit *t)
{
    int n = 0;
    while (t[n].f) n++;
    cob_run_init(t, n);
}
