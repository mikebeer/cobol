/* c30_pic.c -- pictures, usage, layout, data emission */

static int bin_size(int digits)
{
    if (opt_binsize == 12) {
        if (digits <= 2) return 1;
    }
    if (digits <= 4) return 2;
    if (digits <= 9) return 4;
    if (digits <= 18) return 8;
    return 16;
}

/* expand repeat counts and normalize a picture */
static char *pic_expand(Item *it, const char *raw)
{
    Sb b;
    const char *p = raw;
    memset(&b, 0, sizeof b);
    while (*p) {
        int c = toupper((unsigned char)*p);
        p++;
        if (*p == '(') {
            long n = 0;
            const char *q = p + 1;
            while (isdigit((unsigned char)*q)) n = n * 10 + (*q++
                - '0');
            if (*q == ')' && n > 0) {
                long i;
                for (i = 1; i < n; i++) sb_c(&b, c);
                p = q + 1;
            }
        }
        sb_c(&b, c);
    }
    (void)it;
    return sb_str(&b);
}

/* mirror of the runtime's digit count for an edited picture */
static long cpic_runend(const char *pic, long i, char cm)
{
    long j = i, last = i, len = (long)strlen(pic);
    int c = pic[i];
    while (j + 1 < len && (pic[j + 1] == c || pic[j + 1] == cm ||
           pic[j + 1] == 'B' || pic[j + 1] == '0' ||
           pic[j + 1] == '/')) {
        j++;
        if (pic[j] == c) last = j;
    }
    return last;
}

static void pic_analyze(Item *it)
{
    char *pic = pic_expand(it, it->pictxt);
    char *q;
    long len = (long)strlen(pic), i;
    int hasax = 0, hasn = 0, hasedit = 0, hasdig = 0, hasa = 0;
    int hasx = 0, signd = 0, has1 = 0;
    int digits = 0, scale = 0, afterv = 0, size = 0, nine = 0;
    char dp = dpc ? ',' : '.';
    char cm = dpc ? '.' : ',';
    char cur_ = P->currency ? P->currency : '$';
    for (q = pic; *q; q++)
        if (*q == cur_ && cur_ != '$') *q = '$';
    for (i = 0; i < len; i++) {
        int c = pic[i];
        switch (c) {
        case 'A': hasa = 1; hasax = 1; break;
        case 'X': hasx = 1; hasax = 1; break;
        case 'N': hasn = 1; break;
        case '9': hasdig = 1; break;
        case '1': has1 = 1; break;
        case 'S': signd = 1; break;
        case 'V': case 'P': break;
        case 'E': break;
        default: hasedit = 1; break;
        }
    }
    it->digits = 0;
    it->scale = 0;
    if (hasn) {
        it->ctype = CT_NAT;
        it->size = len * 2;
        it->pic = 0;
        return;
    }
    if (has1 && !hasdig && !hasax) {
        it->ctype = CT_BOOL;
        it->size = len;
        it->digits = (int)len;
        return;
    }
    if (hasax) {
        if (hasedit || hasdig) {
            /* B 0 / editing or mixed 9: alphanumeric edited only
               when it has editing characters */
            int ed = 0;
            for (i = 0; i < len; i++)
                if (pic[i] == 'B' || pic[i] == '0' || pic[i] == '/')
                    ed = 1;
            it->ctype = ed ? CT_ALNEDIT : CT_ALNUM;
        } else it->ctype = CT_ALNUM;
        if (hasa && !hasx && !hasedit && !hasdig)
            it->flags |= CF_ALPHA;
        it->size = len;
        if (it->ctype == CT_ALNEDIT) it->pic = pic;
        return;
    }
    if (!hasedit) {
        /* plain numeric */
        int lead = 1;
        for (i = 0; i < len; i++) {
            int c = pic[i];
            if (c == 'V') { afterv = 1; lead = 0; }
            else if (c == '9') {
                digits++;
                nine = 1;
                lead = 0;
                if (afterv) scale++;
            } else if (c == 'P') {
                if (nine) scale--;
                else { scale++; afterv = 1; }
            }
        }
        (void)lead;
        it->ctype = CT_NUMDISP;
        it->digits = digits;
        it->scale = scale;
        if (signd) it->flags |= CF_SIGNED;
        it->size = digits;
        return;
    }
    /* numeric edited */
    {
        int dec = 0, nd = 0;
        afterv = 0;
        for (i = 0; i < len; i++) {
            int c = pic[i];
            if (c == 'V' || c == dp) { afterv = 1; }
            if (c == '9' || c == 'Z' || c == '*') {
                nd++;
                if (afterv) dec++;
            } else if ((c == '+' || c == '-' || c == '$') &&
                       cpic_runend(pic, i, cm) > i) {
                long e = cpic_runend(pic, i, cm), j;
                for (j = i + 1; j <= e; j++)
                    if (pic[j] == c) {
                        nd++;
                        if (afterv) dec++;
                    }
                i = e;
            }
        }
        for (i = 0; i < len; i++)
            if (pic[i] != 'V' && pic[i] != 'S') size++;
        it->ctype = CT_NUMEDIT;
        it->digits = nd;
        it->scale = dec;
        it->size = size;
        it->pic = pic;
        if (dpc) it->flags |= CF_DPC;
        (void)hasdig;
    }
}

/* usage is inherited from the group */
static int eff_usage(Item *it)
{
    Item *p;
    for (p = it; p; p = p->parent)
        if (p->usage_set) return p->usage;
    return U_DISPLAY;
}

static void resolve_item(Item *it)
{
    int u = eff_usage(it);
    if (it->resolved) return;
    it->resolved = 1;
    if (it->level == 88) return;
    if (it->child) {
        it->ctype = CT_GROUP;
        return;
    }
    if (it->level == 66) { it->ctype = CT_GROUP; return; }
    switch (u) {
    case U_COMP1: it->ctype = CT_FLT; it->size = 4; return;
    case U_COMP2: it->ctype = CT_DBL; it->size = 8; return;
    case U_INDEX:
        it->ctype = CT_BIN5; it->size = 4; it->digits = 9;
        it->flags |= CF_IDX;
        return;
    case U_POINTER: case U_PROCPTR: case U_FUNCPTR:
        it->ctype = CT_PTR; it->size = 8; return;
    case U_OBJECT:
        it->ctype = CT_OBJ; it->size = 8; return;
    default: break;
    }
    if (it->pictxt == 0) {
        if (it->ctype && it->size) return;      /* BINARY-LONG ... */
        perr(srcname, it->line, "item needs a PICTURE:",
             it->name ? it->name : "FILLER");
    }
    pic_analyze(it);
    if (u == U_NATIONAL && it->ctype == CT_ALNUM) {
        it->ctype = CT_NAT;
        it->size *= 2;
    }
    if (u != U_DISPLAY && u != U_NATIONAL) {
        if (it->ctype != CT_NUMDISP) {
            /* COMP on a non-numeric picture: treat as display */
            return;
        }
        {
            int d = it->digits;
            int sg = (it->flags & CF_SIGNED) != 0;
            it->flags &= ~CF_SEP;
            switch (u) {
            case U_COMP:
                it->ctype = CT_BIN;
                it->size = bin_size(d);
                break;
            case U_COMP5:
                it->ctype = CT_BIN5;
                it->size = bin_size(d);
                it->flags |= CF_NOTRUNC;
                if (opt_binsize == 24 && d > 18) it->size = 16;
                break;
            case U_COMP3:
                it->ctype = CT_PACKED;
                it->size = d / 2 + 1;
                break;
            default: break;
            }
            (void)sg;
        }
        return;
    }
    if (it->ctype == CT_NUMDISP && (it->signsep)) {
        it->flags |= CF_SEP;
        it->size += 1;
    }
    if (it->ctype == CT_NUMDISP && it->signlead) it->flags |= CF_LEAD;
    if (it->justified) it->flags |= CF_JUST;
    if (it->blankzero) it->flags |= CF_BLANK;
}

static long occ_total(Item *it)
{
    return it->occmax > 0 ? it->occmax : 1;
}

static void lay_item(Item *it, long off)
{
    Item *c;
    it->offset = off;
    resolve_item(it);
    if (!it->child) return;
    {
        long cur_ = off, maxend = off;
        for (c = it->child; c; c = c->next) {
            long start, end;
            if (c->level == 88) continue;
            if (c->redef) start = c->redef->offset;
            else {
                start = cur_;
                if (c->sync && !c->child &&
                    (c->ctype == CT_BIN || c->ctype == CT_BIN5 ||
                     eff_usage(c) >= U_COMP)) {
                    long a;
                    resolve_item(c);
                    a = c->size;
                    if (a == 2 || a == 4 || a == 8) {
                        long rel = start;
                        if (rel % a) start += a - rel % a;
                    }
                }
            }
            lay_item(c, start);
            end = start + c->size * occ_total(c);
            if (!c->redef || end > cur_) cur_ = end > cur_ ? end
                : cur_;
            if (end > maxend) maxend = end;
        }
        it->size = maxend - off;
    }
}

static void lay_rename(Item *it)
{
    Item *f = it->rfrom, *t = it->rthru ? it->rthru : it->rfrom;
    it->offset = f->offset;
    it->size = t->offset + t->size * occ_total(t) - f->offset;
    it->ctype = CT_GROUP;
    it->rec = f->rec;
    it->master = f->master;
    it->storage = f->storage;
}

/* number the dimensions of an item: ancestors with OCCURS, outer
   first.  Returns the count, fills d[]. */
static int item_dims(Item *it, Item **d)
{
    Item *stack[16];
    int n = 0, i, k = 0;
    Item *p;
    for (p = it; p; p = p->parent)
        if (p->occmax > 0 && n < 16) stack[n++] = p;
    for (i = n - 1; i >= 0; i--) d[k++] = stack[i];
    return k;
}

/* --- emission --- */

static int is_dynamic(Item *r)
{
    return r->storage == 1 || r->storage == 2 || r->based ||
           r->storage == 4 || r->storage == 5;
}

static void emit_attr(Sb *o, Item *it)
{
    char *pic = it->pic ? it->pic : 0;
    sb_s(o, fmt("static const CobAttr A%d = { %d, %d, %d, %d, ",
                it->id, it->ctype, it->digits, it->scale, it->flags));
    if (pic) c_str(o, pic, (long)strlen(pic));
    else sb_c(o, '0');
    sb_s(o, " };\n");
}

static void emit_fields(Sb *o, Sb *bind, Item *it, Item *rec, int dyn)
{
    Item *c;
    if (it->level != 88) {
        emit_attr(o, it);
        if (dyn) {
            sb_s(o, fmt("static CobFld F%d = { %ld, 0, &A%d };\n",
                        it->id, it->size, it->id));
            sb_s(bind, fmt("    F%d.data = b + %ld;\n", it->id,
                           it->offset));
        } else {
            sb_s(o, fmt(
                    "static CobFld F%d = { %ld, R%d + %ld, &A%d };\n",
                        it->id, it->size, rec->master->id,
                        it->offset, it->id));
        }
    }
    for (c = it->child; c; c = c->next) emit_fields(o, bind, c, rec,
        dyn);
}

static int all_plain(Item *it)
{
    Item *c;
    if (it->vals || it->redef || (it->occmax > 0 && it->odo)) return 0;
    if (!it->child) return it->ctype == CT_ALNUM ||
                           it->ctype == CT_ALNEDIT;
    for (c = it->child; c; c = c->next)
        if (c->level != 88 && !all_plain(c)) return 0;
    return 1;
}

typedef struct {
    Sb s;
    int n;
} InitTab;

static void init_entry(InitTab *t, Item *it, int kind, const char *lit,
                       int fig)
{
    Item *d[16];
    int nd = item_dims(it, d), i;
    Sb c2, s2;
    if (nd > 3)
        perr(srcname, it->line,
             "more than 3 OCCURS levels are not supported", 0);
    memset(&c2, 0, sizeof c2);
    memset(&s2, 0, sizeof s2);
    for (i = 0; i < 3; i++) {
        if (i) { sb_c(&c2, ','); sb_c(&s2, ','); }
        sb_s(&c2, i < nd ? fmt("%ld", (long)d[i]->occmax) : "0");
        sb_s(&s2, i < nd ? fmt("%ld", d[i]->size) : "0");
    }
    sb_s(&t->s, fmt("    { &F%d, %d, %s, %d, %d, { %s }, { %s } },\n",
                    it->id, kind, lit ? lit : "0", fig, nd,
                    c2.s, s2.s));
    t->n++;
}

static char *val_lit(Val *v, Item *it, int *kind, int *fig)
{
    Tok *t = &v->lo;
    *kind = 1;
    *fig = 0;
    if (v->kind == 1) { *kind = 2; *fig = v->fig; return 0; }
    if (v->kind == 2) {
        *kind = 3;
        return cat2("&", lit_name(t, 0));
    }
    (void)it;
    return cat2("&", lit_name(t, v->neg));
}

static void init_item(InitTab *t, Item *it, int covered, int inred)
{
    Item *c;
    int hasv = it->vals != 0 && it->level != 88;
    if (it->level == 88 || it->level == 66) return;
    if (hasv) {
        int kind, fig;
        char *lit = val_lit(it->vals, it, &kind, &fig);
        init_entry(t, it, kind, lit, fig);
        covered = 1;
    } else if (!covered && !inred && !it->redef) {
        if (!it->child) {
            init_entry(t, it, 0, 0, 0);
        } else if (all_plain(it)) {
            init_entry(t, it, 2, 0, FG_SPACE);
            covered = 1;
        }
    }
    if (it->redef) inred = 1;
    for (c = it->child; c; c = c->next) {
        if (c->level == 88) continue;
        init_item(t, c, covered, inred);
    }
}

static void emit_renames(Prog *pr, Sb *o, Sb *bind, Item *r, int dyn)
{
    Item *x;
    for (x = pr->items; x; x = x->inext)
        if (x->level == 66 && x->rec == r && x->rfrom)
            emit_fields(o, bind, x, r, dyn);
}

static void emit_data(Prog *pr, Sb *o)
{
    Item *r;
    InitTab ws, loc, obt, fct;
    long oo = 0, fo = 0;
    memset(&ws, 0, sizeof ws);
    memset(&loc, 0, sizeof loc);
    memset(&obt, 0, sizeof obt);
    memset(&fct, 0, sizeof fct);
    for (r = pr->recs; r; r = r->rnext) {
        if (r->master != r) continue;
        if (r->storage == 4) {
            r->objoff = oo;
            oo += (r->bufsize + 8 + 7) / 8 * 8;
        } else if (r->storage == 5) {
            r->objoff = fo;
            fo += (r->bufsize + 8 + 7) / 8 * 8;
        }
    }
    pr->objsz = oo;
    pr->facsz = fo;
    /* storage arrays */
    for (r = pr->recs; r; r = r->rnext) {
        if (r->master != r) continue;
        if (is_dynamic(r)) {
            sb_s(o, fmt("static unsigned char *B%d;\n", r->id));
        } else {
            sb_s(o, fmt("static unsigned char R%d[%ld];\n", r->id,
                        r->bufsize > 0 ? r->bufsize + 8 : 8));
        }
    }
    for (r = pr->recs; r; r = r->rnext) {
        Sb bind;
        int dyn = is_dynamic(r);
        memset(&bind, 0, sizeof bind);
        if (r->level == 66 || r->level == 78) continue;
        if (dyn) {
            Item *m = r->master;
            emit_fields(o, &bind, r, r, 1);
            emit_renames(pr, o, &bind, r, 1);
            sb_s(o, fmt("static void BIND%d(unsigned char *b)\n{\n",
                r->id));
            sb_s(o, fmt("    B%d = b;\n", m->id));
            sb_s(o, sb_str(&bind));
            sb_s(o, "}\n");
        } else {
            emit_fields(o, &bind, r, r, 0);
            emit_renames(pr, o, &bind, r, 0);
        }
    }
    for (r = pr->recs; r; r = r->rnext) {
        if (r->level == 66 || r->level == 78 || r->level == 88)
            continue;
        if (r->storage == 1 || r->based) continue;
        if (r->storage == 4 || r->storage == 5) {
            init_item(r->storage == 4 ? &obt : &fct, r, 0, 0);
            continue;
        }
        init_item(r->storage == 2 ? &loc : &ws, r, 0, 0);
    }
    sb_s(o, "static CobInit INI");
    sb_s(o, fmt("%d[] = {\n", pr->ndec));
    sb_s(o, ws.s.s ? ws.s.s : "");
    sb_s(o, "    { 0, 0, 0, 0, 0, { 0, 0, 0 }, { 0, 0, 0 } }\n};\n");
    sb_s(o, fmt("static CobInit INL%d[] = {\n", pr->ndec));
    sb_s(o, loc.s.s ? loc.s.s : "");
    sb_s(o, "    { 0, 0, 0, 0, 0, { 0, 0, 0 }, { 0, 0, 0 } }\n};\n");
    if (pr->iscls) {
        sb_s(o, fmt("#define OSZ%d %ld\n#define FSZ%d %ld\n",
                    pr->id_, oo + 8, pr->id_, fo + 8));
        sb_s(o, fmt("static CobInit INO%d[] = {\n", pr->ndec));
        sb_s(o, obt.s.s ? obt.s.s : "");
        sb_s(o, "    { 0, 0, 0, 0, 0, { 0, 0, 0 }, { 0, 0, 0 } }\n"
                "};\n");
        sb_s(o, fmt("static CobInit INF%d[] = {\n", pr->ndec));
        sb_s(o, fct.s.s ? fct.s.s : "");
        sb_s(o, "    { 0, 0, 0, 0, 0, { 0, 0, 0 }, { 0, 0, 0 } }\n"
                "};\n");
    }
    pr->nws = ws.n;
    pr->nloc = loc.n;
}
