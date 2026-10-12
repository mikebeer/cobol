/* c32_data.c -- identification, environment and data divisions */

typedef struct SN SN;
struct SN { char *name, *target; int kind; SN *next; };
static SN *specnames = 0;

typedef struct UCls UCls;
struct UCls { char *name; int id; unsigned char set[256]; UCls *next;
    };
static UCls *uclasses = 0;

typedef struct Sym Sym;
struct Sym { char *name; int val; Sym *next; };
static Sym *symchars = 0;

static Item *lvl_stack[64];
static int lvl_sp = 0;
static Item *cur_rec = 0;
static Item *last_data = 0;
static Item *pend66[64];
static int npend66 = 0;
static int cur_storage = 0;
static Fil *cur_fil = 0;
static int ws_storage = 0;      /* 4/5 inside OBJECT/FACTORY */

static const char *clause_words[] = {
    "REDEFINES", "PIC", "PICTURE", "USAGE", "DISPLAY", "COMP",
    "COMPUTATIONAL", "COMP-1", "COMP-2", "COMP-3", "COMP-4", "COMP-5",
    "COMP-X", "COMPUTATIONAL-1", "COMPUTATIONAL-2", "COMPUTATIONAL-3",
    "COMPUTATIONAL-4", "COMPUTATIONAL-5", "BINARY", "PACKED-DECIMAL",
    "INDEX", "POINTER", "FUNCTION-POINTER", "PROCEDURE-POINTER",
    "OBJECT", "NATIONAL", "VALUE", "VALUES", "OCCURS", "SIGN",
    "LEADING", "TRAILING", "SYNC", "SYNCHRONIZED", "JUST", "JUSTIFIED",
    "BLANK", "EXTERNAL", "GLOBAL", "BASED", "TYPEDEF", "TYPE",
    "BINARY-CHAR", "BINARY-SHORT", "BINARY-LONG", "BINARY-DOUBLE",
    "FLOAT-SHORT", "FLOAT-LONG", "FLOAT-EXTENDED", "CONSTANT",
    "DYNAMIC", "PROPERTY", "IS", "RENAMES", "ALIGNED", 0
};

static int is_clause_word(const char *s)
{
    int i;
    for (i = 0; clause_words[i]; i++)
        if (!strcmp(s, clause_words[i])) return 1;
    return 0;
}

static void walk_items(Item *it, void (*fn)(Item *))
{
    Item *c;
    fn(it);
    for (c = it->child; c; c = c->next) walk_items(c, fn);
}

static void set_rec(Item *it)
{
    it->rec = cur_rec;
    it->storage = cur_rec->storage;
    it->based = cur_rec->based;
    it->file = cur_rec->file;
}

/* ---- figurative constants and literals in VALUE ---- */

static int fig_code(const char *s)
{
    if (!strcmp(s, "ZERO") || !strcmp(s, "ZEROS") ||
        !strcmp(s, "ZEROES")) return FG_ZERO;
    if (!strcmp(s, "SPACE") || !strcmp(s, "SPACES")) return FG_SPACE;
    if (!strcmp(s, "HIGH-VALUE") || !strcmp(s, "HIGH-VALUES"))
        return FG_HIGH;
    if (!strcmp(s, "LOW-VALUE") || !strcmp(s, "LOW-VALUES"))
        return FG_LOW;
    if (!strcmp(s, "QUOTE") || !strcmp(s, "QUOTES")) return FG_QUOTE;
    if (!strcmp(s, "NULL") || !strcmp(s, "NULLS")) return FG_LOW;
    return 0;
}

static Val *parse_one_value(void)
{
    Val *v = (Val *)xalloc(sizeof(Val));
    Tok *t = cur();
    int neg = 0;
    if (t->k == T_WORD && !strcmp(t->s, "ALL")) {
        tp++;
        t = cur();
        if (t->k == T_STR || t->k == T_NUM) {
            v->kind = 2;
            v->lo = *t;
            tp++;
            return v;
        }
        if (t->k == T_WORD && fig_code(t->s)) {
            v->kind = 1;
            v->fig = fig_code(t->s);
            tp++;
            return v;
        }
        syn("literal expected after ALL");
    }
    if (t->k == T_WORD && fig_code(t->s)) {
        v->kind = 1;
        v->fig = fig_code(t->s);
        tp++;
        return v;
    }
    if (t->k == T_OP && (!strcmp(t->s, "-") || !strcmp(t->s, "+"))) {
        neg = t->s[0] == '-';
        tp++;
        t = cur();
    }
    if (t->k == T_NUM || t->k == T_STR) {
        v->kind = 0;
        v->lo = *t;
        v->neg = neg;
        tp++;
        return v;
    }
    if (t->k == T_WORD) {
        Item *c = lookup(t->s, 0, 0);
        if (c && (c->level == 78) && c->vals) {
            *v = *c->vals;
            tp++;
            return v;
        }
        {
            Sym *s;
            for (s = symchars; s; s = s->next)
                if (!strcmp(s->name, t->s)) {
                    char b[2];
                    Tok nt = *t;
                    b[0] = (char)s->val;
                    b[1] = 0;
                    nt.k = T_STR;
                    nt.s = xdupn(b, 1);
                    nt.len = 1;
                    v->lo = nt;
                    tp++;
                    return v;
                }
        }
    }
    syn("literal expected");
    return v;
}

static Val *parse_values(int thru_ok)
{
    Val *head = 0, *tail = 0;
    for (;;) {
        Val *v;
        Tok *t = cur();
        skip_is();
        t = cur();
        if (!(t->k == T_STR || t->k == T_NUM || t->k == T_OP ||
              (t->k == T_WORD && (fig_code(t->s) ||
                                  !strcmp(t->s, "ALL") ||
                                  (lookup(t->s, 0, 0) &&
                                   lookup(t->s, 0, 0)->level == 78)))))
            break;
        if (t->k == T_OP && strcmp(t->s, "-") && strcmp(t->s, "+"))
            break;
        v = parse_one_value();
        if (thru_ok && (isw("THRU") || isw("THROUGH"))) {
            Val *h;
            tp++;
            h = parse_one_value();
            v->hasthru = 1;
            v->hi = h->lo;
            v->neg = v->neg;
        }
        if (tail) tail->next = v;
        else head = v;
        tail = v;
        if (!thru_ok) break;
    }
    return head;
}

/* ---- data entries ---- */

static Item *find_sibling(Item *parent, const char *name, int level)
{
    Item *c;
    if (parent) {
        for (c = parent->child; c; c = c->next)
            if (c->name && !strcmp(c->name, name)) return c;
        return 0;
    }
    for (c = P->recs; c; c = c->rnext)
        if (c->name && !strcmp(c->name, name) && c->level == level)
            return c;
    return 0;
}

static void parse_usage(Item *it)
{
    Tok *t = cur();
    const char *w = t->s;
    it->usage_set = 1;
    it->usage = U_DISPLAY;
    if (t->k != T_WORD) syn("usage expected");
    tp++;
    if (!strcmp(w, "DISPLAY")) it->usage = U_DISPLAY;
    else if (!strcmp(w, "NATIONAL")) it->usage = U_NATIONAL;
    else if (!strcmp(w, "COMP") || !strcmp(w, "COMPUTATIONAL") ||
             !strcmp(w, "BINARY") || !strcmp(w, "COMP-4") ||
             !strcmp(w, "COMPUTATIONAL-4")) it->usage = U_COMP;
    else if (!strcmp(w, "COMP-5") || !strcmp(w, "COMPUTATIONAL-5") ||
             !strcmp(w, "COMP-X") || !strcmp(w, "COMPUTATIONAL-X"))
        it->usage = U_COMP5;
    else if (!strcmp(w, "COMP-1") || !strcmp(w, "COMPUTATIONAL-1") ||
             !strcmp(w, "FLOAT-SHORT")) it->usage = U_COMP1;
    else if (!strcmp(w, "COMP-2") || !strcmp(w, "COMPUTATIONAL-2") ||
             !strcmp(w, "FLOAT-LONG") || !strcmp(w, "FLOAT-EXTENDED"))
        it->usage = U_COMP2;
    else if (!strcmp(w, "COMP-3") || !strcmp(w, "COMPUTATIONAL-3") ||
             !strcmp(w, "PACKED-DECIMAL")) it->usage = U_COMP3;
    else if (!strcmp(w, "INDEX")) it->usage = U_INDEX;
    else if (!strcmp(w, "POINTER")) {
        it->usage = U_POINTER;
        if (isw("TO")) { tp++; tp++; }
    } else if (!strcmp(w, "FUNCTION-POINTER") ||
               !strcmp(w, "PROCEDURE-POINTER")) {
        it->usage = U_PROCPTR;
        if (isw("TO")) { tp++; tp++; }
    } else if (!strcmp(w, "OBJECT")) {
        acc("REFERENCE");
        it->usage = U_OBJECT;
        if (isw("FACTORY")) { tp++; acc("OF"); }
        if (cur()->k == T_WORD && !at_period() && !isw("VALUE") &&
            !isw("OCCURS") && !isw("SYNC") && !isw("GLOBAL") &&
            !isw("EXTERNAL") && !isw("BASED"))
            it->objtype = cur()->s, tp++;
    } else if (!strcmp(w, "BINARY-CHAR") || !strcmp(w, "BINARY-SHORT")
               || !strcmp(w, "BINARY-LONG") ||
               !strcmp(w, "BINARY-DOUBLE")) {
        int sz = !strcmp(w, "BINARY-CHAR") ? 1 :
                 !strcmp(w, "BINARY-SHORT") ? 2 :
                 !strcmp(w, "BINARY-LONG") ? 4 : 8;
        int uns = 0;
        if (isw("SIGNED")) tp++;
        else if (isw("UNSIGNED")) { tp++; uns = 1; }
        it->usage = U_COMP5;
        it->ctype = CT_BIN5;
        it->size = sz;
        it->digits = sz == 1 ? 3 : sz == 2 ? 5 : sz == 4 ? 10 : 20;
        if (!uns) it->flags |= CF_SIGNED;
        it->flags |= CF_NOTRUNC;
    } else syn("unknown USAGE");
}

static void parse_occurs(Item *it)
{
    long n = need_int(), m = n;
    int i;
    if (isw("TO")) { tp++; m = need_int(); }
    acc("TIMES");
    it->occmin = (int)n;
    it->occmax = (int)m;
    if (isw("DYNAMIC")) {
        tp++;
        it->dynocc = 1;
    }
    for (;;) {
        if (isw("DEPENDING")) {
            tp++; acc("ON");
            it->oddn = get_name();
            continue;
        }
        if (isw("ASCENDING") || isw("DESCENDING")) {
            int d = isw("DESCENDING");
            tp++;
            acc("KEY"); skip_is();
            while (cur()->k == T_WORD && !isw("INDEXED") &&
                   !isw("ASCENDING") && !isw("DESCENDING") &&
                   !isw("DEPENDING") && !is_clause_word(cur()->s) &&
                   it->nkeys < 8) {
                it->keyn[it->nkeys] = cur()->s;
                it->keydir[it->nkeys++] = d;
                tp++;
            }
            continue;
        }
        if (isw("INDEXED")) {
            int k = 0;
            tp++; acc("BY");
            while (cur()->k == T_WORD && !is_clause_word(cur()->s) &&
                   !isw("ASCENDING") && !isw("DESCENDING") &&
                   !isw("DEPENDING") && k < 15)
                it->idxn[k++] = cur()->s, tp++;
            continue;
        }
        break;
    }
    (void)i;
}

/* the picture is the run of adjacent tokens */
static char *parse_pic_string(void)
{
    Sb b;
    int first = 1;
    memset(&b, 0, sizeof b);
    skip_is();
    for (;;) {
        Tok *t = cur();
        if (t->k == T_PERIOD || t->k == T_EOF) break;
        if (!first && t->sp) break;
        if (t->k == T_STR) break;
        if (t->k == T_NUM || t->k == T_WORD || t->k == T_OP) {
            sb_s(&b, t->raw ? t->raw : t->s);
            tp++;
            first = 0;
            continue;
        }
        break;
    }
    if (!b.n) syn("picture expected");
    return sb_str(&b);
}

static void new_index_items(Item *it)
{
    int k;
    for (k = 0; it->idxn[k] && k < 16; k++) {
        Item *x = new_item();
        x->name = it->idxn[k];
        x->level = 77;
        x->usage = U_INDEX;
        x->usage_set = 1;
        x->isindex = 1;
        x->storage = it->storage;
        x->rec = x;
        x->master = x;
        x->based = 0;
        x->file = 0;
        x->global = it->global || (it->rec && it->rec->global);
        item_link(x);
        if (P->lastrec) P->lastrec->rnext = x;
        else P->recs = x;
        P->lastrec = x;
        x->dims = 0;
        lay_item(x, 0);
        x->bufsize = x->size;
    }
}

static void close_record(void);

static Item *data_entry(int storage)
{
    Item *it;
    int level;
    Tok *t;
    level = (int)need_int();
    if (level == 66) {
        it = new_item();
        it->level = 66;
        it->name = get_name();
        need("RENAMES");
        {
            char *a = get_name();
            Item *f = lookup(a, 0, 0), *th = 0;
            if (!f) perr(srcname, it->line, "unknown name", a);
            if (isw("THRU") || isw("THROUGH")) {
                char *b;
                tp++;
                b = get_name();
                th = lookup(b, 0, 0);
                if (!th) perr(srcname, it->line, "unknown name", b);
            }
            it->rfrom = f;
            it->rthru = th;
        }
        it->rec = cur_rec;
        it->parent = 0;
        item_link(it);
        if (npend66 < 64) pend66[npend66++] = it;
        need_period();
        return it;
    }
    if (level == 88) {
        Item *par = last_data;
        if (!par) syn("level 88 without a parent");
        it = new_item();
        it->level = 88;
        it->name = get_name();
        it->parent = par;
        it->rec = par->rec;
        it->storage = par->storage;
        acc("VALUE"); acc("VALUES");
        skip_is();
        it->vals = parse_values(1);
        if (isw("WHEN")) {                 /* WHEN SET TO FALSE IS x */
            tp++;
            need("SET"); need("TO"); need("FALSE");
            skip_is();
            it->falsev = parse_one_value();
        }
        {
            Item **pp = &par->conds;
            while (*pp) pp = &(*pp)->cnext;
            *pp = it;
        }
        item_link(it);
        need_period();
        return it;
    }
    t = cur();
    it = new_item();
    it->level = level;
    it->storage = storage;
    if (storage == 4 || storage == 5) it->global = 1;
    if (t->k == T_WORD && !strcmp(t->s, "FILLER")) {
        tp++;
        it->filler = 1;
    } else if (t->k == T_WORD && !is_clause_word(t->s)) {
        it->name = t->s;
        tp++;
    } else it->filler = 1;
    if (!it->name) {
        static int fctr = 0;
        it->name = fmt("FILLER%d", ++fctr);
    }
    /* constants */
    if (level == 78 || isw("CONSTANT")) {
        int isc = isw("CONSTANT");
        if (isc) { tp++; acc("AS"); } else need("VALUE");
        skip_is();
        it->level = 78;
        if (isw("LENGTH") || isw("BYTE-LENGTH")) {
            /* CONSTANT x AS LENGTH OF y: size known later */
            tp++; acc("OF");
            {
                Item *o = lookup(get_name(), 0, 0);
                Val *v = (Val *)xalloc(sizeof(Val));
                if (o && !o->size && cur_rec) close_record();
                Tok nt;
                memset(&nt, 0, sizeof nt);
                nt.k = T_NUM;
                nt.s = o ? itoa_(o->size) : "0";
                v->lo = nt;
                it->vals = v;
            }
        } else it->vals = parse_one_value();
        item_link(it);
        if (cur_rec && 0) cur_rec = 0;
        need_period();
        return it;
    }
    if (level == 77) {
        close_record();
        lvl_sp = 0;
        it->level = 77;
        cur_rec = it;
        it->rec = it;
    } else if (level == 1) {
        close_record();
        lvl_sp = 0;
        cur_rec = it;
        it->rec = it;
    } else {
        if (lvl_sp == 0) syn("data item without a record level");
    }
    if (level != 1 && level != 77) {
        while (lvl_sp > 0 && lvl_stack[lvl_sp - 1]->level >= level)
            lvl_sp--;
        if (lvl_sp == 0) syn("bad level number");
        it->parent = lvl_stack[lvl_sp - 1];
        {
            Item *par = it->parent;
            if (par->lastchild) par->lastchild->next = it;
            else par->child = it;
            par->lastchild = it;
        }
        set_rec(it);
    }
    lvl_stack[lvl_sp++] = it;
    /* record list */
    if (level == 1 || level == 77) {
        if (P->lastrec) P->lastrec->rnext = it;
        else P->recs = it;
        P->lastrec = it;
        it->file = cur_fil;
        it->storage = storage;
    }
    /* clauses */
    while (!at_period() && !at_eof()) {
        Tok *c = cur();
        if (c->k != T_WORD)
            syn("unexpected token in data description");
        if (!strcmp(c->s, "REDEFINES")) {
            Item *r;
            char *n;
            tp++;
            n = get_name();
            r = find_sibling(it->parent, n, level);
            if (!r) perr(srcname, c->line, "REDEFINES: unknown", n);
            it->redef = r;
            r->redefd = 1;
        } else if (!strcmp(c->s, "PIC") || !strcmp(c->s, "PICTURE")) {
            tp++;
            it->pictxt = parse_pic_string();
        } else if (!strcmp(c->s, "USAGE")) {
            tp++;
            skip_is();
            parse_usage(it);
        } else if (!strcmp(c->s, "DISPLAY") || !strcmp(c->s, "COMP") ||
                   !strcmp(c->s, "COMPUTATIONAL") ||
                   !strncmp(c->s, "COMP-", 5) ||
                   !strncmp(c->s, "COMPUTATIONAL-", 14) ||
                   !strcmp(c->s, "BINARY") ||
                   !strcmp(c->s, "PACKED-DECIMAL") ||
                   !strcmp(c->s, "INDEX") || !strcmp(c->s, "POINTER")
                       ||
                   !strcmp(c->s, "FUNCTION-POINTER") ||
                   !strcmp(c->s, "PROCEDURE-POINTER") ||
                   !strcmp(c->s, "OBJECT") ||
                   !strcmp(c->s, "NATIONAL") ||
                   !strncmp(c->s, "BINARY-", 7) ||
                   !strncmp(c->s, "FLOAT-", 6)) {
            parse_usage(it);
        } else if (!strcmp(c->s, "VALUE") || !strcmp(c->s, "VALUES")) {
            tp++;
            skip_is();
            it->vals = parse_values(0);
            it->hasvalue = 1;
        } else if (!strcmp(c->s, "OCCURS")) {
            tp++;
            parse_occurs(it);
        } else if (!strcmp(c->s, "SIGN")) {
            tp++;
            skip_is();
            if (acc("LEADING")) it->signlead = 1;
            else { acc("TRAILING"); it->signlead = 0; }
            if (acc("SEPARATE")) { it->signsep = 1; acc("CHARACTER"); }
        } else if (!strcmp(c->s, "LEADING") ||
                   !strcmp(c->s, "TRAILING")) {
            it->signlead = c->s[0] == 'L';
            tp++;
            if (acc("SEPARATE")) { it->signsep = 1; acc("CHARACTER"); }
        } else if (!strcmp(c->s, "SEPARATE")) {
            tp++; acc("CHARACTER");
            it->signsep = 1;
        } else if (!strcmp(c->s, "SYNC") ||
                   !strcmp(c->s, "SYNCHRONIZED")) {
            tp++;
            if (isw("LEFT") || isw("RIGHT")) tp++;
            it->sync = 1;
        } else if (!strcmp(c->s, "JUST") ||
                   !strcmp(c->s, "JUSTIFIED")) {
            tp++; acc("RIGHT");
            it->justified = 1;
        } else if (!strcmp(c->s, "BLANK")) {
            tp++; acc("WHEN"); acc("ZERO"); acc("ZEROS");
                acc("ZEROES");
            it->blankzero = 1;
        } else if (!strcmp(c->s, "EXTERNAL")) {
            tp++;
            it->external = 1;
        } else if (!strcmp(c->s, "GLOBAL")) {
            tp++;
            it->global = 1;
        } else if (!strcmp(c->s, "BASED")) {
            tp++;
            it->based = 1;
        } else if (!strcmp(c->s, "IS")) {
            tp++;
        } else if (!strcmp(c->s, "SIGNED") ||
                   !strcmp(c->s, "UNSIGNED") ||
                   !strcmp(c->s, "ALIGNED") ||
                   !strcmp(c->s, "DYNAMIC")) {
            tp++;
        } else syn("unknown data description clause");
    }
    need_period();
    item_link(it);
    if (it->idxn[0]) new_index_items(it);
    last_data = it;
    if (it->level == 1 || it->level == 77) {
        if (it->external) it->global = it->global;
        if (it->external) { }
    }
    return it;
}

/* finish the record being built: layout, masters, sizes */
static void close_odo(Item *it)
{
    if (it->oddn) {
        it->odo = lookup(it->oddn, 0, 0);
        if (!it->odo)
            perr(srcname, it->line, "OCCURS DEPENDING ON: unknown",
                 it->oddn);
    }
}

static void close_record(void)
{
    Item *r = cur_rec;
    int i;
    if (!r) return;
    cur_rec = 0;
    if (r->level == 78) return;
    lay_item(r, 0);
    walk_items(r, close_odo);
    for (i = 0; i < npend66; i++) lay_rename(pend66[i]);
    npend66 = 0;
    /* storage master */
    if (r->redef && r->level == 1) {
        r->master = r->redef->master;
        r->rec = r;
    } else if (r->file && r->file->recs && r->file->recs != r) {
        r->master = r->file->recs->master;
    } else r->master = r;
    {
        long sz = r->size * (r->occmax > 0 ? r->occmax : 1);
        if (sz > r->master->bufsize) r->master->bufsize = sz;
    }
    {
        Item *c;
        for (c = r->child; c; c = c->next) { (void)c; }
    }
    /* children inherit the master through rec */
    r->rec = r;
}

static Item *synth77(const char *name, const char *pic, int usage)
{
    Item *it = new_item();
    it->level = 77;
    it->name = xdup(name);
    it->pictxt = xdup(pic);
    it->usage = usage;
    it->usage_set = 1;
    it->storage = 0;
    it->global = 1;
    it->rec = it;
    it->master = it;
    item_link(it);
    if (P->lastrec) P->lastrec->rnext = it;
    else P->recs = it;
    P->lastrec = it;
    lay_item(it, 0);
    it->bufsize = it->size;
    return it;
}

/* ---- environment division ---- */

static Fil *find_file(const char *name)
{
    Fil *f;
    Prog *pr;
    for (pr = P; pr; pr = pr->outer)
        for (f = pr->files; f; f = f->next)
            if (!strcmp(f->name, name)) return f;
    return 0;
}

static void parse_select(void)
{
    Fil *f = (Fil *)xalloc(sizeof(Fil));
    f->id = ++idctr;
    f->sel_line = cur()->line;
    if (acc("OPTIONAL")) f->optional = 1;
    f->name = get_name();
    f->cname = cat2("FL", itoa_(f->id));
    f->org = 0;
    f->access = 0;
    while (!at_period() && !at_eof()) {
        if (acc("ASSIGN")) {
            acc("TO");
            for (;;) {
                if (isw("EXTERNAL") || isw("DYNAMIC") || isw("DISK") ||
                    isw("PRINTER") || isw("TAPE") || isw("USING")) {
                    tp++;
                    continue;
                }
                break;
            }
            if (cur()->k == T_STR) {
                f->assign = cur()->s;
                tp++;
            } else if (cur()->k == T_WORD) {
                f->dependn = cur()->s;
                tp++;
                if (cur()->k == T_STR) {       /* ASSIGN TO DISK "x" */
                    f->assign = cur()->s;
                    f->dependn = 0;
                    tp++;
                }
            }
        } else if (acc("ORGANIZATION") || acc("ORGANISATION")) {
            skip_is();
            if (acc("LINE")) { acc("SEQUENTIAL"); f->org = 1; }
            else if (acc("SEQUENTIAL")) f->org = 0;
            else if (acc("RELATIVE")) f->org = 2;
            else if (acc("INDEXED")) f->org = 3;
            else if (acc("BINARY")) { acc("SEQUENTIAL"); f->org = 0; }
            else syn("bad ORGANIZATION");
        } else if (isw("RECORD") && (isw_at(1, "SEQUENTIAL"))) {
            tp += 2;
        } else if (acc("ACCESS")) {
            acc("MODE"); skip_is();
            if (acc("SEQUENTIAL")) f->access = 0;
            else if (acc("RANDOM")) f->access = 1;
            else if (acc("DYNAMIC")) f->access = 2;
            else syn("bad ACCESS MODE");
        } else if (acc("ALTERNATE")) {
            acc("RECORD"); acc("KEY"); skip_is();
            if (f->nakeys < 8) f->akn[f->nakeys] = get_name();
            else tp++;
            if (acc("WITH")) acc("DUPLICATES");
            else if (acc("DUPLICATES")) { }
            else goto adup0;
            f->akdup[f->nakeys < 8 ? f->nakeys : 0] = 1;
adup0:
            if (f->nakeys < 8) f->nakeys++;
        } else if (acc("RECORD")) {
            acc("KEY"); skip_is();
            f->keyn = get_name();
            if (acc("WITH")) acc("DUPLICATES");
            else if (acc("DUPLICATES")) { }
        } else if (acc("RELATIVE")) {
            acc("KEY"); skip_is();
            f->relkeyn = get_name();
        } else if (isw("FILE") || isw("STATUS")) {
            acc("FILE"); need("STATUS"); skip_is();
            f->statusn = get_name();
            if (cur()->k == T_WORD && !at_period() &&
                !isw("ORGANIZATION") && !isw("ACCESS") &&
                !isw("RECORD") && !isw("LOCK") && !isw("ASSIGN") &&
                !isw("RELATIVE") && !isw("ALTERNATE") &&
                !isw("SHARING") && !isw("RESERVE") && !isw("PADDING"))
                tp++;                   /* second status item */
        } else if (acc("LOCK")) {
            acc("MODE"); skip_is();
            while (!at_period() && !at_eof() && !isw("ORGANIZATION")
                   && !isw("ACCESS") && !isw("RECORD") &&
                   !isw("FILE") && !isw("STATUS") && !isw("RELATIVE")
                   && !isw("ALTERNATE") && !isw("ASSIGN"))
                tp++;
        } else if (acc("RESERVE")) {
            need_int(); acc("AREA"); acc("AREAS");
        } else if (acc("PADDING")) {
            acc("CHARACTER"); skip_is(); tp++;
        } else if (acc("SHARING")) {
            acc("WITH"); tp++; acc("OTHER"); acc("ONLY");
        } else if (acc("COLLATING")) {
            acc("SEQUENCE"); skip_is(); tp++;
        } else tp++;
    }
    need_period();
    if (P->lastfile) P->lastfile->next = f;
    else P->files = f;
    P->lastfile = f;
}

static void parse_special_names(void)
{
    while (!at_eof()) {
        Tok *t = cur();
        if (t->k != T_WORD) { tp++; continue; }
        if (!strcmp(t->s, "DECIMAL-POINT")) {
            tp++; skip_is(); acc("COMMA");
        } else if (!strcmp(t->s, "CURRENCY")) {
            tp++; acc("SIGN"); skip_is();
            if (cur()->k == T_STR) {
                P->currency = cur()->s[0];
                tp++;
            }
            if (acc("WITH")) { acc("PICTURE"); acc("SYMBOL"); tp++; }
        } else if (!strcmp(t->s, "CLASS")) {
            UCls *c = (UCls *)xalloc(sizeof(UCls));
            tp++;
            c->name = get_name();
            c->id = ++idctr;
            skip_is();
            while (cur()->k == T_STR || cur()->k == T_NUM) {
                int lo, hi;
                Tok *a = cur();
                lo = a->k == T_STR ? (unsigned char)a->s[0]
                                   : atoi(a->s) - 1;
                tp++;
                hi = lo;
                if (isw("THRU") || isw("THROUGH")) {
                    Tok *b;
                    tp++;
                    b = cur();
                    hi = b->k == T_STR ? (unsigned char)b->s[0]
                                       : atoi(b->s) - 1;
                    tp++;
                } else if (a->k == T_STR && a->len > 1) {
                    long q;
                    for (q = 1; q < a->len; q++)
                        c->set[(unsigned char)a->s[q]] = 1;
                }
                {
                    int q;
                    if (lo > hi) { int tt = lo; lo = hi; hi = tt; }
                    for (q = lo; q <= hi && q < 256; q++)
                        c->set[q] = 1;
                }
            }
            c->next = uclasses;
            uclasses = c;
        } else if (!strcmp(t->s, "SYMBOLIC")) {
            tp++; acc("CHARACTERS");
            for (;;) {
                Sym *s;
                if (cur()->k != T_WORD || isw("ALPHABET")) break;
                s = (Sym *)xalloc(sizeof(Sym));
                s->name = get_name();
                skip_is();
                s->val = (int)need_int() - 1;
                s->next = symchars;
                symchars = s;
                if (at_period()) break;
            }
        } else if (!strcmp(t->s, "ALPHABET")) {
            tp++;
            tp++;
            while (!at_period() && !at_eof() && !isw("CLASS") &&
                   !isw("SYMBOLIC") && !isw("CURRENCY") &&
                   !isw("DECIMAL-POINT") && !isw("ALPHABET")) tp++;
        } else if (!strcmp(t->s, "CRT") || !strcmp(t->s, "CURSOR")) {
            tp++; acc("STATUS"); skip_is(); tp++;
        } else if (!strcmp(t->s, "REPOSITORY")) {
            return;
        } else if (!strcmp(t->s, "INPUT-OUTPUT") ||
                   !strcmp(t->s, "DATA") ||
                   !strcmp(t->s, "PROCEDURE") ||
                   !strcmp(t->s, "FILE-CONTROL")) {
            return;
        } else {
            /* implementor-name IS mnemonic [ON STATUS IS x ...] */
            SN *s = (SN *)xalloc(sizeof(SN));
            char *w = t->s;
            tp++;
            s->target = w;
            if (acc("IS")) {
                s->name = get_name();
                s->kind = 1;
                s->next = specnames;
                specnames = s;
            }
            while (isw("ON") || isw("OFF")) {
                SN *q = (SN *)xalloc(sizeof(SN));
                int on = isw("ON");
                tp++; acc("STATUS"); skip_is();
                q->name = get_name();
                q->target = w;
                q->kind = on ? 2 : 3;
                q->next = specnames;
                specnames = q;
            }
        }
        if (at_period()) { tp++; return; }
    }
}

static void parse_repository(void)
{
    while (!at_eof() && !at_period()) tp++;
    acc_period();
}

static void parse_environment(void)
{
    if (!(isw("ENVIRONMENT"))) return;
    tp++; need("DIVISION"); need_period();
    for (;;) {
        if (isw("CONFIGURATION")) {
            tp++; need("SECTION"); need_period();
        } else if (isw("SOURCE-COMPUTER") || isw("OBJECT-COMPUTER")) {
            skip_sentence();
        } else if (isw("SPECIAL-NAMES")) {
            tp++; need_period();
            parse_special_names();
        } else if (isw("REPOSITORY")) {
            tp++; need_period();
            parse_repository();
        } else if (isw("INPUT-OUTPUT")) {
            tp++; need("SECTION"); need_period();
        } else if (isw("FILE-CONTROL")) {
            tp++; need_period();
            while (isw("SELECT")) { tp++; parse_select(); }
        } else if (isw("I-O-CONTROL")) {
            skip_sentence();
            while (!at_eof() && !isw("DATA") && !isw("PROCEDURE") &&
                   !isw("IDENTIFICATION")
                       && !isw("ID")) skip_sentence();
        } else break;
    }
}

/* ---- data division ---- */

static void parse_fd(int sd)
{
    Fil *f;
    char *n;
    tp++;
    n = get_name();
    f = find_file(n);
    if (!f) perr(srcname, cur()->line, "FD without SELECT:", n);
    f->isort = sd;
    while (!at_period() && !at_eof()) {
        if (isw("RECORD") || isw("BLOCK") || isw("LABEL") ||
            isw("VALUE") || isw("DATA") || isw("CODE-SET") ||
            isw("RECORDING") || isw("IS") || isw("EXTERNAL") ||
            isw("GLOBAL") || isw("LINAGE") || isw("REPORT") ||
            isw("REPORTS")) {
            if (isw("RECORD")) {
                tp++;
                acc("CONTAINS");
                if (acc("VARYING")) {
                    acc("IN"); acc("SIZE");
                    if (acc("FROM")) f->minrec = need_int();
                    if (acc("TO")) f->maxrec = need_int();
                    acc("CHARACTERS");
                    if (acc("DEPENDING")) {
                        acc("ON");
                        f->depn = get_name();
                    }
                    f->rectype = 1;
                } else if (cur()->k == T_NUM) {
                    f->minrec = need_int();
                    if (acc("TO")) {
                        f->maxrec = need_int();
                        f->rectype = 1;
                    } else f->maxrec = f->minrec;
                    acc("CHARACTERS");
                    if (acc("DEPENDING")) {
                        acc("ON");
                        f->depn = get_name();
                    }
                } else if (acc("IS") || 1) {
                    /* RECORD IS VARYING ... */
                    if (acc("VARYING")) {
                        acc("IN"); acc("SIZE");
                        if (acc("FROM")) f->minrec = need_int();
                        if (acc("TO")) f->maxrec = need_int();
                        acc("CHARACTERS");
                        if (acc("DEPENDING")) {
                            acc("ON");
                            f->depn = get_name();
                        }
                        f->rectype = 1;
                    }
                }
            } else if (isw("LINAGE")) {
                tp++; skip_is();
                f->linage = (int)need_int();
                acc("LINES");
                while (isw("WITH") || isw("FOOTING") || isw("AT") ||
                       isw("LINES") || isw("TOP") || isw("BOTTOM")) {
                    tp++;
                    if (cur()->k == T_NUM) tp++;
                }
            } else {
                tp++;
            }
        } else tp++;
    }
    need_period();
    cur_fil = f;
    f->recs = 0;
}

static void parse_data(void)
{
    if (!isw("DATA")) return;
    tp++; need("DIVISION"); need_period();
    for (;;) {
        if (isw("FILE")) {
            tp++; need("SECTION"); need_period();
            cur_storage = 3;
            while (isw("FD") || isw("SD")) {
                int sd = isw("SD");
                Item *first = 0;
                parse_fd(sd);
                while (cur()->k == T_NUM) {
                    Item *r = data_entry(3);
                    if (r->level == 1 || r->level == 77) {
                        if (!first) {
                            first = r;
                            cur_fil->recs = r;
                        }
                        cur_fil->lastrec = r;
                        r->file = cur_fil;
                    }
                }
                close_record();
                cur_fil = 0;
            }
            close_record();
        } else if (isw("WORKING-STORAGE") || isw("LOCAL-STORAGE") ||
                   isw("LINKAGE")) {
            int st = isw("LINKAGE") ? 1 : isw("LOCAL-STORAGE") ? 2 : 0;
            if (st == 0 && ws_storage) st = ws_storage;
            tp++; need("SECTION"); need_period();
            cur_storage = st;
            cur_fil = 0;
            lvl_sp = 0;
            last_data = 0;
            while (cur()->k == T_NUM) data_entry(st);
            close_record();
        } else if (isw("REPORT") || isw("SCREEN") ||
                   isw("COMMUNICATION")) {
            tp++; need("SECTION"); need_period();
            while (cur()->k == T_NUM || isw("RD") || isw("CD"))
                skip_sentence();
        } else break;
    }
}
